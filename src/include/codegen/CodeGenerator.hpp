#ifndef CODEGEN_CODE_GENERATOR_H
#define CODEGEN_CODE_GENERATOR_H

#include "sema/SymbolTable.hpp"
#include "visitor/AstNodeVisitor.hpp"

#include <map>
#include <memory>
#include <stack>
#include <utility>

int fclose(FILE *);

class CodeGenerator final : public AstNodeVisitor {
  private:
    enum class CodegenContext : uint8_t {
        kGlobal,
        kLocal
    };

    struct FileDeleter {
        void operator()(FILE *fp) const {
            fclose(fp);
        }
    };

  private:
    union StackValue {
      int d;
      int reg; // llvm register
      float f;
      const char *str;
      const char *global_var;
    };
    std::stack<StackValue> m_value_stack;
    enum class CurrentValueType {
      INT,
      REG,
      FLOAT,
      STR,
      GLOBAL
    };
    std::stack<CurrentValueType> m_type_stack;

    // In llvm ir, we can't put br after ret or generate a label for empty basic blocks.
    // To deal with if statements that constain ret, we need a variabel to indicate
    // weather there are any ret in "if clause" or "else clause".
    bool has_ret; 

    // When an array is passed as an argument, it is "passed by pointer",
    // the corresponding llvm ir is different with the case when dealing main function.
    std::stack<bool> call_stack; // it should be empty when dealing main function

    bool dealing_params = false;

    const SymbolManager *m_symbol_manager_ptr;
    std::string m_source_file_path;
    std::unique_ptr<FILE, FileDeleter> m_output_file;

    std::stack<CodegenContext> m_context_stack;

    size_t m_local_var_offset = 0;
    std::map<const SymbolEntry *, size_t> m_local_var_offset_map;

    bool m_ref_to_value = false;

    size_t m_label_sequence = 1;
    size_t m_comp_branch_true_label = 0;
    size_t m_comp_branch_false_label = 0;

  public:
    ~CodeGenerator() = default;
    CodeGenerator(const std::string source_file_name,
                  const std::string save_path,
                  const SymbolManager *const p_symbol_manager);

    void visit(ProgramNode &p_program) override;
    void visit(DeclNode &p_decl) override;
    void visit(VariableNode &p_variable) override;
    void visit(ConstantValueNode &p_constant_value) override;
    void visit(FunctionNode &p_function) override;
    void visit(CompoundStatementNode &p_compound_statement) override;
    void visit(PrintNode &p_print) override;
    void visit(BinaryOperatorNode &p_bin_op) override;
    void visit(UnaryOperatorNode &p_un_op) override;
    void visit(FunctionInvocationNode &p_func_invocation) override;
    void visit(VariableReferenceNode &p_variable_ref) override;
    void visit(AssignmentNode &p_assignment) override;
    void visit(ReadNode &p_read) override;
    void visit(IfNode &p_if) override;
    void visit(WhileNode &p_while) override;
    void visit(ForNode &p_for) override;
    void visit(ReturnNode &p_return) override;

  private:
    static bool isInGlobal(const std::stack<CodegenContext> &p_context_stack) {
        return p_context_stack.top() == CodegenContext::kGlobal;
    }
    static bool isInLocal(const std::stack<CodegenContext> &p_context_stack) {
        return p_context_stack.top() == CodegenContext::kLocal;
    }
    // void
    // storeArgumentsToParameters(const FunctionNode::DeclNodes &p_parameters);

    void pushIntToStack(int d);
    void pushRegToStack(int reg);
    void pushFloatToStack(float f);
    void pushStrToStack(const char *str);
    void pushGlobalVarToStack(const char *global_var);
    std::pair<StackValue, CurrentValueType> popFromStack();
};

#endif
