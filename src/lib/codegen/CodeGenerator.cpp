#include "codegen/CodeGenerator.hpp"
#include "AST/operator.hpp"
#include "visitor/AstNodeInclude.hpp"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <sstream>
#include <iostream>

CodeGenerator::CodeGenerator(const std::string source_file_name,
                             const std::string save_path,
                             const SymbolManager *const p_symbol_manager)
    : m_symbol_manager_ptr(p_symbol_manager),
      m_source_file_path(source_file_name) {
    // FIXME: assume that the source file is always xxxx.p
    const std::string &real_path =
        (save_path == "") ? std::string{"."} : save_path;
    auto slash_pos = source_file_name.rfind("/");
    auto dot_pos = source_file_name.rfind(".");

    if (slash_pos != std::string::npos) {
        ++slash_pos;
    } else {
        slash_pos = 0;
    }
    std::string output_file_path(
        real_path + "/" +
        source_file_name.substr(slash_pos, dot_pos - slash_pos) + ".ll");
    m_output_file.reset(fopen(output_file_path.c_str(), "w"));
    assert(m_output_file.get() && "Failed to open output file");
}

static void emitInstructions(FILE *p_out_file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(p_out_file, format, args);
    va_end(args);
}

void CodeGenerator::visit(ProgramNode &p_program) {
    // Generate RISC-V instructions for program header
    // clang-format off
    constexpr const char*const llvm_ir_file_prologue =
        "source_filename = \"%s\"\n"
        "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n"
        "target triple = \"x86_64-pc-linux-gnu\"\n\n"
        "declare i32 @printf(i8*, ...)\n"
        "declare i32 @__isoc99_scanf(i8*, ...)\n\n"
        "@.str = private unnamed_addr constant [4 x i8] c\"%%d\\0A\\00\", align 1\n";

    // clang-format on
    emitInstructions(m_output_file.get(), llvm_ir_file_prologue,
                     m_source_file_path.c_str());

    // Reconstruct the hash table for looking up the symbol entry
    // Hint: Use symbol_manager->lookup(symbol_name) to get the symbol entry.
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(p_program.getSymbolTable());
    m_context_stack.push(CodegenContext::kGlobal);

    auto visit_ast_node = [&](auto &ast_node) { ast_node->accept(*this); };
    for_each(p_program.getDeclNodes().begin(), p_program.getDeclNodes().end(),
             visit_ast_node);
    for_each(p_program.getFuncNodes().begin(), p_program.getFuncNodes().end(),
             visit_ast_node);

    constexpr const char*const llvm_ir_main_prologue =
        "\ndefine i32 @main() {\n";
    emitInstructions(m_output_file.get(), llvm_ir_main_prologue,
                     m_source_file_path.c_str());

    m_local_var_offset = 1;
    const_cast<CompoundStatementNode &>(p_program.getBody()).accept(*this);

    constexpr const char*const llvm_ir_main_epilogue =
        "\n  ret i32 0\n"
        "}";
    emitInstructions(m_output_file.get(), llvm_ir_main_epilogue,
                     m_source_file_path.c_str());

    // Remove the entries in the hash table
    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_program.getSymbolTable());
}

void CodeGenerator::visit(DeclNode &p_decl) { p_decl.visitChildNodes(*this); }

void CodeGenerator::visit(VariableNode &p_variable) {
    const auto *constant_ptr = p_variable.getConstantPtr();
    if (isInGlobal(m_context_stack)) {
        int init_val = 0;
        if (constant_ptr)
            init_val = constant_ptr->integer();
        if (p_variable.getTypePtr()->isInteger()) {
            emitInstructions(m_output_file.get(), 
                    "@%s = global i32 %d, align 4\n",
                    p_variable.getNameCString(), init_val);
        }
        return;
    }

    if (isInLocal(m_context_stack)) {
        auto dim = p_variable.getTypePtr()->getDimensions();
        m_local_var_offset_map.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(
                    m_symbol_manager_ptr->lookup(p_variable.getName())),
                std::forward_as_tuple(m_local_var_offset));
        if (!dim.size()) { // primitive
            emitInstructions(m_output_file.get(),
                                "  %%%d = alloca i32, align 4"
                                " ; allocate %s\n",
                                 m_local_var_offset, p_variable.getName().c_str());
            if (constant_ptr) {
                std::stringstream target;
                target << "%" << p_variable.getName();
                emitInstructions(m_output_file.get(),
                                "  store i32 %d, i32* %%%d, align 4"
                                " ; store to %s\n",
                                constant_ptr->integer(), m_local_var_offset, target.str().c_str());
            }
        }
        else {
            if (!call_stack.size()) { // in main
                if (dim.size() == 1) { // 1D array
                    emitInstructions(m_output_file.get(),
                                    "  %%%d = alloca [%d x i32], align 16"
                                    " ; allocate %s\n",
                                    m_local_var_offset, dim[0], p_variable.getName().c_str());
                }
                else if (dim.size() == 2) { // 2D array
                    emitInstructions(m_output_file.get(),
                                    "  %%%d = alloca [%d x [%d x i32]], align 16"
                                    " ; allocate %s\n",
                                    m_local_var_offset, dim[0], dim[1], p_variable.getName().c_str());
                }
                else
                    assert(false && "Not Supported!");
            }
            else { // not in main
                if (dim.size() == 1) { // 1D array
                    emitInstructions(m_output_file.get(),
                                    "  %%%d = alloca i32*, align 8"
                                    " ; allocate %s\n",
                                    m_local_var_offset, p_variable.getName().c_str());
                }
                else if (dim.size() == 2) { // 2D array
                    emitInstructions(m_output_file.get(),
                                    "  %%%d = alloca [%d x i32]*, align 8"
                                    " ; allocate %s\n",
                                    m_local_var_offset, dim[1], p_variable.getName().c_str());
                }
                else
                    assert(false && "Not Supported!");
            }
        }
        m_local_var_offset += 1;

        return;
    }
    assert(false && "Shouln't reach here. It means that the context has wrong value");
}

void CodeGenerator::visit(ConstantValueNode &p_constant_value) {
    pushIntToStack(p_constant_value.getConstantPtr()->integer());
}

void CodeGenerator::visit(FunctionNode &p_function) {
    call_stack.push(1);

    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_function.getSymbolTable());
    m_context_stack.push(CodegenContext::kLocal);

    m_local_var_offset = 0;

    std::string return_type = "i32";
    std::stringstream function_head;
    function_head << "\ndefine " << return_type << " @" << p_function.getName() << "(";
    
    // support one decl node in function parameter list for now
    auto &variables = p_function.getParameters()[0]->getVariables();
    for (size_t i = 0; i < variables.size(); ++i) {
        auto type_ptr = variables[i]->getTypePtr();
        if (!type_ptr->getDimensions().size()) { // primitive
            if (type_ptr->getPrimitiveType() == PType::PrimitiveTypeEnum::kIntegerType)
                function_head << "i32 %" << m_local_var_offset++;
            else
                assert(false && "Not supported!");
        }
        else { // array, support 1D & 2D integer array for now
            auto dim = type_ptr->getDimensions();
            if (dim.size() == 1) { // 1D array
                function_head << "i32* %" << m_local_var_offset++;
            }
            else if (dim.size() == 2) { // 2D array
                function_head << "[" << dim[1] << " x i32]* %" << m_local_var_offset++;
            }
            else
                assert(false && "Not supported!");
        }
        if (i != variables.size() - 1)
            function_head << ", ";
    }
    function_head << ") {";

    emitInstructions(m_output_file.get(), "%s\n", function_head.str().c_str());

    m_local_var_offset += 1; // reserve one value for entry block

    auto visit_ast_node = [&](auto &ast_node) { ast_node->accept(*this); };
    for_each(p_function.getParameters().begin(),
             p_function.getParameters().end(), visit_ast_node);

    // store parameter's value to alloca variable
    for (int i = 0; i < variables.size(); ++i) {
        int alloca_var_number = m_local_var_offset - i - 1;
        int param_number = alloca_var_number - variables.size() - 1;
        auto type_ptr = variables[i]->getTypePtr();
        if (!type_ptr->getDimensions().size()) { // primitive
            if (type_ptr->getPrimitiveType() == PType::PrimitiveTypeEnum::kIntegerType) {
                emitInstructions(m_output_file.get(),
                            "  store i32 %%%d, i32* %%%d, align 4\n",
                            param_number, alloca_var_number);
            }
            else
                assert(false && "Not supported!");
        }
        else { // array
            auto dim = type_ptr->getDimensions();
            if (dim.size() == 1) { // 1D array
                emitInstructions(m_output_file.get(),
                            "  store i32* %%%d, i32** %%%d, align 8\n",
                            param_number, alloca_var_number);
            }
            else if (dim.size() == 2) { // 2D array
                emitInstructions(m_output_file.get(),
                            "  store [%d x i32]* %%%d, [%d x i32]** %%%d, align 8\n",
                            dim[1], param_number, dim[1], alloca_var_number);
            }
            else
                assert(false && "Not supported!");
        }
    }

    p_function.visitBodyChildNodes(*this);
    emitInstructions(m_output_file.get(), "}\n");

    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(
        p_function.getSymbolTable());

    call_stack.pop();
}

void CodeGenerator::visit(CompoundStatementNode &p_compound_statement) {
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_compound_statement.getSymbolTable());
    m_context_stack.push(CodegenContext::kLocal);

    p_compound_statement.visitChildNodes(*this);

    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(
        p_compound_statement.getSymbolTable());
}

void CodeGenerator::visit(PrintNode &p_print) {
    m_ref_to_value = true;
    p_print.visitChildNodes(*this);

    auto value_type = popFromStack();
    CurrentValueType type = value_type.second;
    if (type == CurrentValueType::REG) {
        int reg = value_type.first.reg;
        emitInstructions(m_output_file.get(), "  %%%ld = call i32 (i8*, ...) @printf(i8* getelementptr inbounds"
                                            " ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32 %%%d)\n",
                                            m_local_var_offset, reg);
        m_local_var_offset += 1;
    }
    else
        assert(false && "Shouldn't reach here!");
}

void CodeGenerator::visit(BinaryOperatorNode &p_bin_op) {
    p_bin_op.visitChildNodes(*this);

    assert(m_value_stack.size() > 1 && m_type_stack.size() > 1 &&
            "There should be at least two value on both stacks!");

    auto value_type2 = popFromStack();
    auto value_type1 = popFromStack();
    int int1 = value_type1.first.d;
    int int2 = value_type2.first.d;
    int reg1 = value_type1.first.reg;
    int reg2 = value_type2.first.reg;

    auto type1 = value_type1.second;
    auto type2 = value_type2.second;

    switch (p_bin_op.getOp()) {
    case Operator::kMultiplyOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = mul nsw i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        else if (type1 == CurrentValueType::REG && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = mul nsw i32 %%%d, %d\n", m_local_var_offset, int1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = mul nsw i32 %d, %%%d\n", m_local_var_offset, int1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = mul nsw i32 %d, %d\n", m_local_var_offset, int1, int2);
        else
            assert(false && "Not supported!\n");
        break;
    case Operator::kDivideOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = sdiv exact i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = sdiv exact i32 %d, %d\n", m_local_var_offset, int1, int2);
        else
            assert(false && "Not supported!\n");
        break;
    case Operator::kModOp:
         if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = srem i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = srem i32 %d, %d\n", m_local_var_offset, int1, int2);
        else
            assert(false && "Not supported!\n");
        break;
    case Operator::kPlusOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        else if (type1 == CurrentValueType::REG && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %%%d, %d\n", m_local_var_offset, reg1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %d, %%%d\n", m_local_var_offset, int1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %d, %d\n", m_local_var_offset, int1, int2);
        else
            assert(false && "Not supported!\n");
        break;
    case Operator::kMinusOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = sub nsw i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        else if (type1 == CurrentValueType::REG && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = sub nsw i32 %%%d, %d\n", m_local_var_offset, int1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = sub nsw i32 %d, %%%d\n", m_local_var_offset, int1, int2);
        else if (type1 == CurrentValueType::INT && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = sub nsw i32 %d, %d\n", m_local_var_offset, int1, int2);
        else
            assert(false && "Not supported!\n");
        break;
    case Operator::kLessOp:
        emitInstructions(m_output_file.get(), "  %%%d = icmp slt i32 %%%d, %d\n", m_local_var_offset, reg1, int2);
        break;
    case Operator::kLessOrEqualOp:
        emitInstructions(m_output_file.get(), "  %%%d = icmp sle i32 %%%d, %d\n", m_local_var_offset, reg1, int2);
        break;
    case Operator::kGreaterOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = icmp sgt i32 %%%d, %d\n", m_local_var_offset, reg1, int2);
        else if (type1 == CurrentValueType::REG && type2 == CurrentValueType::REG)
            emitInstructions(m_output_file.get(), "  %%%d = icmp sgt i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        break;
    case Operator::kGreaterOrEqualOp:
        break;
    case Operator::kEqualOp:
        if (type1 == CurrentValueType::REG && type2 == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = icmp eq i32 %%%d, %d\n",
                                m_local_var_offset, reg1, int2);
        break;
    case Operator::kNotEqualOp:
        break;
    default:
        assert(false && "unsupported binary operator");
        break;
    }
    
    pushRegToStack(m_local_var_offset++);
}

void CodeGenerator::visit(UnaryOperatorNode &p_un_op) {
    p_un_op.visitChildNodes(*this);

    auto value_type = popFromStack();
    auto type = value_type.second;
    auto value = value_type.first;

    switch (p_un_op.getOp()) {
    case Operator::kNegOp:
        if (type == CurrentValueType::INT)
            emitInstructions(m_output_file.get(), "  %%%d = sub nsw i32 0, %d\n", m_local_var_offset, value.d);
        else
            assert(false && "Should not reach here!");
        break;
    default:
        assert(false && "unsupported unary operator");
        return;
    }
    
    pushRegToStack(m_local_var_offset++);
}

void CodeGenerator::visit(FunctionInvocationNode &p_func_invocation) {
    const auto &arguments = p_func_invocation.getArguments();
    m_ref_to_value = true;
    auto visit_ast_node = [&](auto &ast_node) { ast_node->accept(*this); };
    dealing_params = true;
    for_each(arguments.begin(), arguments.end(), visit_ast_node);
    dealing_params = false;

    std::vector<std::pair<StackValue, CurrentValueType>> args(arguments.size());
    for (size_t i = 0; i < arguments.size(); ++i)
        args[arguments.size() - 1 - i] = popFromStack();
    pushRegToStack(m_local_var_offset);
    
    std::stringstream func;
    func << "%" << m_local_var_offset++ << " = call i32 @" 
        << p_func_invocation.getNameCString() << "(";
    for (size_t i = 0; i < arguments.size(); ++i) {
        auto value = args[i].first;
        auto type = args[i].second;
        if (type == CurrentValueType::INT) 
            func << "i32 " << value.d;
        else if (type == CurrentValueType::REG) {
            VariableReferenceNode* var_ptr = dynamic_cast<VariableReferenceNode*>(&*arguments[i]);
            bool is_array = false;
            std::map<const SymbolEntry *, size_t>::iterator search;
            if (var_ptr) {
                const auto *entry_ptr =
                m_symbol_manager_ptr->lookup(var_ptr->getName());
                search = m_local_var_offset_map.find(entry_ptr);
                if (search != m_local_var_offset_map.end()
                    && search->first->getTypePtr()->getDimensions().size())
                    is_array = true;
            }
            
            if (!is_array)
                func << "i32 %" << value.reg;
            else {
                auto dim = search->first->getTypePtr()->getDimensions();
                if (dim.size() == 1) {
                    func << "i32* %" << value.reg;
                }
                else if (dim.size() == 2) {
                    func << "[" << dim[1] << " x i32]* %" << value.reg;
                }
                else
                    assert(false && "Not supported!");
            }
        }
        else
            assert(false && "Should not reach here!");

        if (i != arguments.size() - 1)
            func << ", ";
    } 
    func << ")";
    emitInstructions(m_output_file.get(), "  %s\n", func.str().c_str());
}

void CodeGenerator::visit(VariableReferenceNode &p_variable_ref) {
    // Haven't supported array reference

    p_variable_ref.visitChildNodes(*this);
    const auto *entry_ptr =
            m_symbol_manager_ptr->lookup(p_variable_ref.getName());
        auto search = m_local_var_offset_map.find(entry_ptr);
    // assume we never access pointers through array references
    bool is_array = false;
    if (search != m_local_var_offset_map.end()
        && search->first->getTypePtr()->getDimensions().size())
        is_array = true;
    if (!is_array) { // primitive
        // dereference to get the value if needed
        if (m_ref_to_value) {
            emitInstructions(m_output_file.get(), "  %%%ld = load i32, i32* ",
                            m_local_var_offset);

            pushRegToStack(m_local_var_offset++);

            if (search == m_local_var_offset_map.end()) { // global variable reference
                emitInstructions(m_output_file.get(), "@%s, align 4\n",
                                p_variable_ref.getNameCString());
            } else { // local variable reference
                emitInstructions(m_output_file.get(), "%%%ld, align 4\n",
                                search->second);
            }
        }
        else {  // get the reg
            if (search == m_local_var_offset_map.end()) // global variable reference
                pushGlobalVarToStack(p_variable_ref.getNameCString());
            else // local variable reference
                pushRegToStack(search->second);
        }
    }
    else { // array
        auto dim_sz = p_variable_ref.getIndices().size();
        auto dim = search->first->getTypePtr()->getDimensions();
        if (!call_stack.size()) { // in main
            if (dealing_params) {
                if (dim.size() == 1) {
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 0\n",
                                m_local_var_offset, dim[0], dim[0], search->second);
                }
                else if (dim.size() == 2) {
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x [%d x i32]], [%d x [%d x i32]]* %%%d, i64 0, i64 0\n",
                                m_local_var_offset, dim[0], dim[1], dim[0], dim[1], search->second);
                }
                else
                    assert(false && "Not supported!");

                pushRegToStack(m_local_var_offset++);
            }
            else if (m_ref_to_value) { // dereference to get the value if needed
                auto value_type1 = popFromStack();
                assert(value_type1.second == CurrentValueType::INT && "Must be an integer!");
                auto index1 = value_type1.first.d;
                if (dim_sz == 1) {
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset++, dim[0], dim[0], search->second, index1);
                    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                                m_local_var_offset, m_local_var_offset - 1);
                }
                else if (dim_sz == 2) {
                    auto value_type2 = popFromStack();
                    assert(value_type2.second == CurrentValueType::INT && "Must be an integer!");
                    auto index2 = value_type2.first.d;
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x [%d x i32]], [%d x [%d x i32]]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset++, dim[0], dim[1], dim[0], dim[1], search->second, index2);
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset, dim[1], dim[1], m_local_var_offset - 1, index1);
                    m_local_var_offset += 1;
                    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                                m_local_var_offset, m_local_var_offset - 1);
                }
                else
                    assert(false && "Not supported!");

                pushRegToStack(m_local_var_offset++);
            }
            else {  // get the reg
                auto value_type1 = popFromStack();
                assert(value_type1.second == CurrentValueType::INT && "Must be an integer!");
                auto index1 = value_type1.first.d;
                if (dim_sz == 1) {
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset, dim[0], dim[0], search->second, index1);
                }
                else if (dim_sz == 2) {
                    auto value_type2 = popFromStack();
                    assert(value_type2.second == CurrentValueType::INT && "Must be an integer!");
                    auto index2 = value_type2.first.d;
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x [%d x i32]], [%d x [%d x i32]]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset++, dim[0], dim[1], dim[0], dim[1], search->second, index2);
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset, dim[1], dim[1], m_local_var_offset - 1, index1);
                }
                else
                    assert(false && "Not supported!");

                pushRegToStack(m_local_var_offset++);
            }
        }
        else { // not in main
            if (m_ref_to_value) {
                auto value_type1 = popFromStack();
                assert(value_type1.second == CurrentValueType::INT && "Must be an integer!");
                auto index1 = value_type1.first.d;
                if (dim_sz == 1) {
                    emitInstructions(m_output_file.get(), "  %%%d = load i32*, i32** %%%d, align 8\n",
                                m_local_var_offset++, search->second);
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds i32, i32* %%%d, i64 %d\n",
                                m_local_var_offset, m_local_var_offset - 1, index1);
                    m_local_var_offset += 1;
                    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                                m_local_var_offset, m_local_var_offset - 1);
                }
                else if (dim_sz == 2) {
                    auto value_type2 = popFromStack();
                    assert(value_type2.second == CurrentValueType::INT && "Must be an integer!");
                    auto index2 = value_type2.first.d;
                    emitInstructions(m_output_file.get(), "  %%%d = load [%d x i32]*, [%d x i32]** %%%d, align 8\n",
                                m_local_var_offset++, dim[1], dim[1], search->second);
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 %d\n",
                                m_local_var_offset, dim[1], dim[1], m_local_var_offset - 1, index2);
                    m_local_var_offset += 1;
                    emitInstructions(m_output_file.get(), "  %%%d = getelementptr inbounds [%d x i32], [%d x i32]* %%%d, i64 0, i64 %d\n",
                                m_local_var_offset, dim[1], dim[1], m_local_var_offset - 1, index1);
                    m_local_var_offset += 1;
                    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                                m_local_var_offset, m_local_var_offset - 1);
                }
                else
                    assert(false && "Not supported!");

                pushRegToStack(m_local_var_offset++);
            }
            else {  // get the reg
                assert(false && "Not supported!");
            }
        }
    }
}

void CodeGenerator::visit(AssignmentNode &p_assignment) {
    m_ref_to_value = false; // as lval
    const_cast<VariableReferenceNode &>(p_assignment.getLvalue()).accept(*this);
    m_ref_to_value = true; // as rval
    const_cast<ExpressionNode &>(p_assignment.getExpr()).accept(*this);

    auto value_type = popFromStack();
    CurrentValueType type = value_type.second;

    std::stringstream target, name;
    const auto *entry_ptr = m_symbol_manager_ptr->lookup(p_assignment.getLvalue().getName());
    if (!entry_ptr->getLevel()) { // global variable
        name << "@" << p_assignment.getLvalue().getName();
        target << "@" << p_assignment.getLvalue().getName();
    }
    else { // local variable
        auto search = m_local_var_offset_map.find(entry_ptr);
        assert(search != m_local_var_offset_map.end() &&
                        "Should find unnamed value for the variable reference!");
        auto dim = search->first->getTypePtr()->getDimensions();
        if (!dim.size()) { // primitive type
            name << "%" << search->second;
            target << "%" << p_assignment.getLvalue().getName();
        }
        else { // array type
            auto value_type = popFromStack();
            assert(value_type.second == CurrentValueType::REG);
            name << "%" << value_type.first.reg;
            target << "%" << p_assignment.getLvalue().getName();
        }
    }

    if (type == CurrentValueType::INT) {
        int val = value_type.first.d;
        emitInstructions(m_output_file.get(),
                        "  store i32 %d, i32* %s, align 4"
                        " ; store to %s\n",
                        val, name.str().c_str(), target.str().c_str());
    }
    else if (type == CurrentValueType::REG) {
        int reg = value_type.first.reg;
        emitInstructions(m_output_file.get(),
                        "  store i32 %%%d, i32* %s, align 4"
                        " ; store to %s\n",
                        reg, name.str().c_str(), target.str().c_str());
    }
    else
        assert(false && "Should not reach here!");
}

void CodeGenerator::visit(ReadNode &p_read) {
    m_ref_to_value = false; 
    p_read.visitChildNodes(*this);
    auto value_type = popFromStack();
    if (value_type.second == CurrentValueType::GLOBAL) {
        auto var = value_type.first.global_var;
        emitInstructions(m_output_file.get(), 
                        "  %%%d = call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds "
                        "([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32* @%s)\n",
                        m_local_var_offset++, var);
    }
    else if (value_type.second == CurrentValueType::REG) {
        emitInstructions(m_output_file.get(), 
                        "  %%%d = call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds "
                        "([4 x i8], [4 x i8]* @.str, i64 0, i64 0), i32* %%%d)\n",
                        m_local_var_offset++, value_type.first.reg);
    }
    else
        assert(false && "Note supported!");
}

void CodeGenerator::visit(IfNode &p_if) {
    // TODO: cannot handle nested compound statements
    const auto *else_body_ptr = p_if.getElseBodyPtr();
    m_ref_to_value = true;
    const_cast<ExpressionNode &>(p_if.getCondition()).accept(*this);

    auto value_type = popFromStack();
    auto value = value_type.first.reg;
    assert(value_type.second == CurrentValueType::REG && "Must be reg type!");

    emitInstructions(m_output_file.get(), "  ; if-else \n");
    fpos_t pos;
    emitInstructions(m_output_file.get(), "  br i1 %%%d, label %%%d, label %%", value, m_local_var_offset);
    fgetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values

    has_ret = false;
    emitInstructions(m_output_file.get(), "%d:  ; if\n", m_local_var_offset++);
    const_cast<CompoundStatementNode &>(p_if.getIfBody()).accept(*this);
    if (!has_ret)
        emitInstructions(m_output_file.get(), "  br label %%");
    fpos_t cur_pos;
    fgetpos(m_output_file.get(), &cur_pos);
    fsetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "%d", m_local_var_offset);
    fsetpos(m_output_file.get(), &cur_pos);
    pos = cur_pos;
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values
    has_ret = false;

    if (else_body_ptr) {
        emitInstructions(m_output_file.get(), "%d:  ; else\n", m_local_var_offset++);
        const_cast<CompoundStatementNode *>(else_body_ptr)->accept(*this);
        if (!has_ret)
            emitInstructions(m_output_file.get(), "  br label %%%d\n", m_local_var_offset);
        fgetpos(m_output_file.get(), &cur_pos);
        fsetpos(m_output_file.get(), &pos);
        if (!has_ret)
            emitInstructions(m_output_file.get(), "%d", m_local_var_offset);
        fsetpos(m_output_file.get(), &cur_pos);
        if (!has_ret)
            emitInstructions(m_output_file.get(), "%d:\n", m_local_var_offset++);
        has_ret = false;
    }
}

void CodeGenerator::visit(WhileNode &p_while) {
    // TODO: cannot handle nested compound statements
    int head_label = m_local_var_offset;
    emitInstructions(m_output_file.get(), "  br label %%%d\n", head_label);
    m_ref_to_value = true;
    emitInstructions(m_output_file.get(), "%d:  ; while head\n", m_local_var_offset++);
    const_cast<ExpressionNode &>(p_while.getCondition()).accept(*this);

    auto value_type = popFromStack();
    auto value = value_type.first.reg;
    assert(value_type.second == CurrentValueType::REG && "Must be reg type!");
    
    fpos_t pos;
    emitInstructions(m_output_file.get(), "  br i1 %%%d, label %%%d, label %%", value, m_local_var_offset);
    fgetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values

    emitInstructions(m_output_file.get(), "%d:  ; while body\n", m_local_var_offset++);
    const_cast<CompoundStatementNode &>(p_while.getBody()).accept(*this);
    emitInstructions(m_output_file.get(), "  br label %%%d\n", head_label);

    emitInstructions(m_output_file.get(), "%d:\n", m_local_var_offset);
    fpos_t cur_pos;
    fgetpos(m_output_file.get(), &cur_pos);
    fsetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "%d", m_local_var_offset++);
    fsetpos(m_output_file.get(), &cur_pos);
}

void CodeGenerator::visit(ForNode &p_for) {
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_for.getSymbolTable());
    m_context_stack.push(CodegenContext::kLocal);

    emitInstructions(m_output_file.get(), "  ; for init\n");
    const_cast<DeclNode &>(p_for.getLoopVarDecl()).accept(*this);
    const_cast<AssignmentNode &>(p_for.getLoopVarInitStmt()).accept(*this);
    // hand-written comparison
    const auto *entry_ptr =
        m_symbol_manager_ptr->lookup(p_for.getLoopVarName());
    auto search = m_local_var_offset_map.find(entry_ptr);
    assert(search != m_local_var_offset_map.end() && "Should have been defined before use");

    int head_label = m_local_var_offset;
    emitInstructions(m_output_file.get(), "  br label %%%d\n", head_label);
    emitInstructions(m_output_file.get(), "%d:  ; for head\n", m_local_var_offset++);
    int loop_var = search->second;
    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                        m_local_var_offset++, loop_var);
    emitInstructions(m_output_file.get(), "  %%%d = icmp slt i32 %%%d, %d\n",
                        m_local_var_offset, m_local_var_offset - 1, p_for.getUpperBound().getConstantPtr()->integer());
    m_local_var_offset += 1;
    emitInstructions(m_output_file.get(), "  br i1 %%%d, label %%%d, label %%", 
                        m_local_var_offset - 1, m_local_var_offset);
    fpos_t pos;
    fgetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values

    emitInstructions(m_output_file.get(), "%d:  ; for body\n", m_local_var_offset++);
    const_cast<CompoundStatementNode &>(p_for.getBody()).accept(*this);
    emitInstructions(m_output_file.get(), "  %%%d = load i32, i32* %%%d, align 4\n",
                        m_local_var_offset++, loop_var);
    emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %%%d, 1\n", m_local_var_offset, m_local_var_offset - 1);
    m_local_var_offset += 1;
    emitInstructions(m_output_file.get(), "  store i32 %%%d, i32* %%%d, align 4\n", m_local_var_offset - 1, loop_var);
    emitInstructions(m_output_file.get(), "  br label %%%d\n", head_label);

    fpos_t cur_pos;
    fgetpos(m_output_file.get(), &cur_pos);
    fsetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "%d", m_local_var_offset);
    fsetpos(m_output_file.get(), &cur_pos);
    emitInstructions(m_output_file.get(), "%d:\n", m_local_var_offset++);

    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_for.getSymbolTable());
}

void CodeGenerator::visit(ReturnNode &p_return) {
    has_ret = true;
    m_ref_to_value = true;
    p_return.visitChildNodes(*this);

    std::stringstream function_end;
    function_end << "  ret ";

    auto value_type = popFromStack();
    CurrentValueType type = value_type.second;
    if (type == CurrentValueType::REG) 
        function_end << "i32 %" << value_type.first.reg;
    else if (type == CurrentValueType::INT) 
        function_end << "i32 " << value_type.first.d;
    else
        assert(false && "Should not reach here!");

    emitInstructions(m_output_file.get(), "%s\n", function_end.str().c_str());
}


/* Utility functions */

void CodeGenerator::pushIntToStack(int d) {
    StackValue value;
    value.d = d;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::INT);
}

void CodeGenerator::pushRegToStack(int reg) {
    StackValue value;
    value.reg = reg;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::REG);
}

void CodeGenerator::pushFloatToStack(float f) {
    StackValue value;
    value.f = f;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::FLOAT);
}

void CodeGenerator::pushStrToStack(const char *str) {
    StackValue value;
    value.str = str;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::STR);
}

void CodeGenerator::pushGlobalVarToStack(const char *global_var) {
    StackValue value;
    value.str = global_var;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::GLOBAL);
}

std::pair<CodeGenerator::StackValue, CodeGenerator::CurrentValueType>
CodeGenerator::popFromStack() {
    assert(m_value_stack.size() && m_type_stack.size() &&
            "There should be at least one value on both stacks!");

    CurrentValueType type = m_type_stack.top();
    StackValue value;
    if (type == CurrentValueType::INT) {
        value.d = m_value_stack.top().d;
    }
    else if (type == CurrentValueType::REG) {
        value.reg = m_value_stack.top().reg;
    }
    else if (type == CurrentValueType::FLOAT) {
        value.f = m_value_stack.top().f;
    }
    else if (type == CurrentValueType::STR) {
        value.str = m_value_stack.top().str;
    }
    else if (type == CurrentValueType::GLOBAL) {
        value.global_var = m_value_stack.top().global_var;
    }
    else
        assert (false && "Shouldn't reach here!");

    m_type_stack.pop();
    m_value_stack.pop();

    return {value, type};
}