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

// clang-format off
// static constexpr const char*const kFixedFunctionPrologue =
//     "    .globl %s\n"
//     "    .type %s, @function\n"
//     "%s:\n"
//     "    addi sp, sp, -128\n"
//     "    sw ra, 124(sp)\n"
//     "    sw s0, 120(sp)\n"
//     "    addi s0, sp, 128\n";

// static constexpr const char*const kFixedFunctionEpilogue =
//     "    lw ra, 124(sp)\n"
//     "    lw s0, 120(sp)\n"
//     "    addi sp, sp, 128\n"
//     "    jr ra\n"
//     "    .size %s, .-%s\n";
// clang-format on

void CodeGenerator::visit(ProgramNode &p_program) {
    // Generate RISC-V instructions for program header
    // clang-format off
    constexpr const char*const llvm_ir_file_prologue =
        "source_filename = \"%s\"\n"
        "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n"
        "target triple = \"x86_64-pc-linux-gnu\"\n\n"
        "declare i32 @printf(i8*, ...)\n\n"
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
        "}\n";
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
        m_local_var_offset_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(
                m_symbol_manager_ptr->lookup(p_variable.getName())),
            std::forward_as_tuple(m_local_var_offset));

        emitInstructions(m_output_file.get(),
                            "  %c%d = alloca i32, align 4"
                            " ; allocate %s\n",
                            '%', m_local_var_offset, p_variable.getName().c_str());
        if (constant_ptr) {
            std::stringstream target;
            target << "%" << p_variable.getName();
            emitInstructions(m_output_file.get(),
                             "  store i32 %d, i32* %%%d, align 4"
                             " ; store to %s\n",
                             constant_ptr->integer(), m_local_var_offset, target.str().c_str());
        }

        m_local_var_offset += 1;

        return;
    }

    assert(false &&
           "Shouln't reach here. It means that the context has wrong value");
}

void CodeGenerator::visit(ConstantValueNode &p_constant_value) {
    pushIntToStack(p_constant_value.getConstantPtr()->integer());
}

void CodeGenerator::visit(FunctionNode &p_function) {
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_function.getSymbolTable());
    m_context_stack.push(CodegenContext::kLocal);

    m_local_var_offset = 0;

    std::string return_type = "i32";
    std::stringstream function_head;
    function_head << "\ndefine " << return_type << " @" << p_function.getName() << "(";
    
    // support one decl node in function parameter list for now
    auto &variables = p_function.getParameters()[0]->getVariables();
    for (auto iter = variables.begin(); iter != variables.end(); ++iter) {
        function_head << "i32 %" << m_local_var_offset++;
        if (iter + 1 != variables.end())
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
        emitInstructions(m_output_file.get(),
                         "  store i32 %%%d, i32* %%%d, align 4\n",
                         param_number, alloca_var_number);
    }

    // storeArgumentsToParameters(p_function.getParameters());

    p_function.visitBodyChildNodes(*this);

    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(
        p_function.getSymbolTable());
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
        emitInstructions(m_output_file.get(), "  %%%d = mul nsw i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        break;
    case Operator::kDivideOp:
        //emitInstructions(m_output_file.get(), "    div t0, t1, t0\n");
        break;
    case Operator::kModOp:
        //emitInstructions(m_output_file.get(), "    rem t0, t1, t0\n");
        break;
    case Operator::kPlusOp:
        emitInstructions(m_output_file.get(), "  %%%d = add nsw i32 %%%d, %%%d\n", m_local_var_offset, reg1, reg2);
        break;
    case Operator::kMinusOp:
        //emitInstructions(m_output_file.get(), "    sub t0, t1, t0\n");
        break;
    case Operator::kLessOp:
        break;
    case Operator::kLessOrEqualOp:
        break;
    case Operator::kGreaterOp:
        emitInstructions(m_output_file.get(), "  %%%d = icmp sgt i32 %%%d, %d\n", m_local_var_offset, reg1, int2);
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

    // emitInstructions(m_output_file.get(), "    lw t0, 0(sp)\n"
    //                                       "    addi sp, sp, 4\n");

    switch (p_un_op.getOp()) {
    case Operator::kNegOp:
        //emitInstructions(m_output_file.get(), "    sub t0, zero, t0\n");
        break;
    default:
        assert(false && "unsupported unary operator");
        return;
    }
    // emitInstructions(m_output_file.get(), "    addi sp, sp, -4\n"
    //                                       "    sw t0, 0(sp)\n");
}

void CodeGenerator::visit(FunctionInvocationNode &p_func_invocation) {
    const auto &arguments = p_func_invocation.getArguments();
    m_ref_to_value = true;
    auto visit_ast_node = [&](auto &ast_node) { ast_node->accept(*this); };
    for_each(arguments.begin(), arguments.end(), visit_ast_node);

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
        else if (type == CurrentValueType::REG)
            func << "i32 %" << value.reg;
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
    
    // dereference to get the value if needed
    if (m_ref_to_value) {
        const auto *entry_ptr =
            m_symbol_manager_ptr->lookup(p_variable_ref.getName());
        auto search = m_local_var_offset_map.find(entry_ptr);

        emitInstructions(m_output_file.get(), "  %%%ld = load i32, i32* ",
                        m_local_var_offset);

        pushRegToStack(m_local_var_offset++);

        if (search == m_local_var_offset_map.end()) {
            // global variable reference
            emitInstructions(m_output_file.get(), "@%s, align 4\n",
                            p_variable_ref.getNameCString());
        } else {
            // local variable reference
            emitInstructions(m_output_file.get(), "%%%ld, align 4\n",
                            search->second);
        }
    }

    // push onto stack
    // emitInstructions(m_output_file.get(), "    addi sp, sp, -4\n"
    //                                       "    sw t0, 0(sp)\n");
}

void CodeGenerator::visit(AssignmentNode &p_assignment) {
    m_ref_to_value = false; // as lval
    const_cast<VariableReferenceNode &>(p_assignment.getLvalue()).accept(*this);
    m_ref_to_value = true; // as rval
    const_cast<ExpressionNode &>(p_assignment.getExpr()).accept(*this);

    std::stringstream target, name;
    const auto *entry_ptr = m_symbol_manager_ptr->lookup(p_assignment.getLvalue().getName());
    if (!entry_ptr->getLevel()) { // global variable
        name << "@" << p_assignment.getLvalue().getName();
        target << "@" << p_assignment.getLvalue().getName();
    }
    else // local variable
    {
        auto search = m_local_var_offset_map.find(entry_ptr);
        assert(search != m_local_var_offset_map.end() &&
                        "Should find unnamed value for the variable reference!");
        name << "%" << search->second;
        target << "%" << p_assignment.getLvalue().getName();
    }

    auto value_type = popFromStack();
    CurrentValueType type = value_type.second;
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
    
    // emitInstructions(
    //     m_output_file.get(),
    //     "    jal ra, readInt\n"
    //     "    lw t0, 0(sp)\n"
    //     "    addi sp, sp, 4\n"
    //     "    sw a0, 0(t0)\n");
}

void CodeGenerator::visit(IfNode &p_if) {
    // const auto if_body_label = m_label_sequence;
    // ++m_label_sequence;

    const auto *else_body_ptr = p_if.getElseBodyPtr();
    // const size_t else_body_label = (else_body_ptr) ? m_label_sequence++ : 0;

    // const auto out_label = m_label_sequence;
    // ++m_label_sequence;

    // m_comp_branch_true_label = if_body_label;
    // m_comp_branch_false_label = (else_body_ptr) ? else_body_label : out_label;
    // m_ref_to_value = true;
    const_cast<ExpressionNode &>(p_if.getCondition()).accept(*this);

    auto value_type = popFromStack();
    auto value = value_type.first.reg;
    assert(value_type.second == CurrentValueType::REG && "Must be reg type!");

    emitInstructions(m_output_file.get(), "  ; if-else \n");
    fpos_t pos;
    emitInstructions(m_output_file.get(), "  br i1 %%%d, label %%%d, label %%", value, m_local_var_offset);
    fgetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values

    emitInstructions(m_output_file.get(), "%d:  ; if\n", m_local_var_offset++);
    const_cast<CompoundStatementNode &>(p_if.getIfBody()).accept(*this);
    emitInstructions(m_output_file.get(), "  br label %%");
    fpos_t cur_pos;
    fgetpos(m_output_file.get(), &cur_pos);
    fsetpos(m_output_file.get(), &pos);
    emitInstructions(m_output_file.get(), "%d", m_local_var_offset);
    fsetpos(m_output_file.get(), &cur_pos);
    pos = cur_pos;
    emitInstructions(m_output_file.get(), "               \n"); // a hack to deal with large label values

    if (else_body_ptr) {
        emitInstructions(m_output_file.get(), "%d:  ; else\n", m_local_var_offset++);
        // TODO: cannot handle nested compound statements
        // emitInstructions(m_output_file.get(),
        //                  "    j L%u\n"
        //                  "L%u:\n",
        //                  out_label, else_body_label);
        const_cast<CompoundStatementNode *>(else_body_ptr)->accept(*this);
        emitInstructions(m_output_file.get(), "  br label %%%d\n", m_local_var_offset);
        fgetpos(m_output_file.get(), &cur_pos);
        fsetpos(m_output_file.get(), &pos);
        emitInstructions(m_output_file.get(), "%d", m_local_var_offset);
        fsetpos(m_output_file.get(), &cur_pos);
        emitInstructions(m_output_file.get(), "%d:\n", m_local_var_offset++);
    }
    
}

void CodeGenerator::visit(WhileNode &p_while) {
    const auto while_head_label = m_label_sequence;
    const auto while_body_label = m_label_sequence + 1;
    const auto while_out_label = m_label_sequence + 2;
    m_label_sequence += 3;

    //emitInstructions(m_output_file.get(), "L%u:\n", while_head_label);
    m_comp_branch_true_label = while_body_label;
    m_comp_branch_false_label = while_out_label;
    m_ref_to_value = true;
    const_cast<ExpressionNode &>(p_while.getCondition()).accept(*this);

    //emitInstructions(m_output_file.get(), "L%u:\n", while_body_label);
    const_cast<CompoundStatementNode &>(p_while.getBody()).accept(*this);
    // TODO: cannot handle nested compound statements
    // emitInstructions(m_output_file.get(),
    //                  "    j L%u\n"
    //                  "L%u:\n",
    //                  while_head_label, while_out_label);
}

void CodeGenerator::visit(ForNode &p_for) {
    m_symbol_manager_ptr->reconstructHashTableFromSymbolTable(
        p_for.getSymbolTable());
    m_context_stack.push(CodegenContext::kLocal);

    const_cast<DeclNode &>(p_for.getLoopVarDecl()).accept(*this);
    const_cast<AssignmentNode &>(p_for.getLoopVarInitStmt()).accept(*this);

    const auto for_head_label = m_label_sequence;
    const auto for_body_label = m_label_sequence + 1;
    const auto for_out_label = m_label_sequence + 2;
    m_label_sequence += 3;

    //emitInstructions(m_output_file.get(), "L%u:\n", for_head_label);

    // hand-written comparison
    const auto *entry_ptr =
        m_symbol_manager_ptr->lookup(p_for.getLoopVarName());
    auto search = m_local_var_offset_map.find(entry_ptr);
    assert(search != m_local_var_offset_map.end() &&
           "Should have been defined before use");
    // emitInstructions(m_output_file.get(),
    //                  "    lw t1, -%u(s0)\n"
    //                  "    li t0, %u\n"
    //                  "    blt t1, t0, L%u\n"
    //                  "    j L%u\n",
    //                  search->second,
    //                  p_for.getUpperBound().getConstantPtr()->integer(),
    //                  for_body_label, for_out_label);

    //emitInstructions(m_output_file.get(), "L%u:\n", for_body_label);
    const_cast<CompoundStatementNode &>(p_for.getBody()).accept(*this);

    // loop_var += 1 & jump back to head for condition check
    // emitInstructions(m_output_file.get(),
    //                  "    lw t0, -%u(s0)\n"
    //                  "    li t1, 1\n"
    //                  "    add t0, t0, t1\n"
    //                  "    sw t0, -%u(s0)\n",
    //                  search->second, search->second);
    // TODO: cannot handle nested compound statements
    // emitInstructions(m_output_file.get(),
    //                  "    j L%u\n"
    //                  "L%u:\n",
    //                  for_head_label, for_out_label);

    m_context_stack.pop();
    m_symbol_manager_ptr->removeSymbolsFromHashTable(p_for.getSymbolTable());
}

void CodeGenerator::visit(ReturnNode &p_return) {
    m_ref_to_value = true;
    p_return.visitChildNodes(*this);

    std::stringstream function_end;
    function_end << "  ret ";

    auto value_type = popFromStack();
    CurrentValueType type = value_type.second;
    if (type == CurrentValueType::REG) {
        function_end << "i32 %" << value_type.first.reg << "\n}";
    }
    else
        assert(false && "Should not reach here!");

    emitInstructions(m_output_file.get(), "%s\n", function_end.str().c_str());
}


/* Utility functions */

void CodeGenerator::storeArgumentsToParameters(
    const FunctionNode::DeclNodes &p_parameters) {
    constexpr size_t kNumOfArgumentRegister = 8;
    size_t index = 0;

    for (const auto &parameter : p_parameters) {
        for (const auto &var_node_ptr : parameter->getVariables()) {
            const auto *entry_ptr =
                m_symbol_manager_ptr->lookup(var_node_ptr->getName());
            auto search = m_local_var_offset_map.find(entry_ptr);
            assert(search != m_local_var_offset_map.end() &&
                   "Should have been defined before use");

            if (index < kNumOfArgumentRegister) {
                // emitInstructions(m_output_file.get(), "    sw a%u, -%u(s0)\n",
                //                  index, search->second);
            } else {
                // emitInstructions(m_output_file.get(),
                //                  "    lw t0, %u(s0)\n"
                //                  "    sw t0, -%u(s0)\n",
                //                  4 * (index - kNumOfArgumentRegister),
                //                  search->second);
            }
            ++index;
        }
    }
}

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

void CodeGenerator::pushToFloatStack(float f) {
    StackValue value;
    value.f = f;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::FLOAT);
}

void CodeGenerator::pushToStrStack(char *str) {
    StackValue value;
    value.str = str;
    m_value_stack.push(value);
    m_type_stack.push(CurrentValueType::STR);
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
    else
        assert (false && "Shouldn't reach here!");

    m_type_stack.pop();
    m_value_stack.pop();

    return {value, type};
}