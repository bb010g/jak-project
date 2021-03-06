#pragma once

#include <vector>
#include <unordered_set>
#include <memory>
#include <functional>
#include "decompiler/Disasm/Register.h"
#include "decompiler/IR2/AtomicOp.h"
#include "common/goos/Object.h"

namespace decompiler {
class Form;
class Env;
class FormStack;

/*!
 * A "FormElement" represents a single LISP form that's not a begin.
 * This is a abstract base class that all types of forms should be based on.
 */
class FormElement {
 public:
  Form* parent_form = nullptr;

  goos::Object to_form(const Env& env) const;
  virtual goos::Object to_form_internal(const Env& env) const = 0;
  virtual goos::Object to_form_as_condition_internal(const Env& env) const;
  virtual ~FormElement() = default;
  virtual void apply(const std::function<void(FormElement*)>& f) = 0;
  virtual void apply_form(const std::function<void(Form*)>& f) = 0;
  virtual bool is_sequence_point() const { return true; }
  virtual void collect_vars(VariableSet& vars) const = 0;
  virtual void get_modified_regs(RegSet& regs) const = 0;
  virtual bool active() const;

  std::string to_string(const Env& env) const;
  bool has_side_effects();

  // push the result of this operation to the operation stack
  virtual void push_to_stack(const Env& env, FormPool& pool, FormStack& stack);
  virtual void update_from_stack(const Env& env,
                                 FormPool& pool,
                                 FormStack& stack,
                                 std::vector<FormElement*>* result,
                                 bool allow_side_effects);

 protected:
  friend class Form;
};

/*!
 * A SimpleExpressionElement is a form which has the value of a SimpleExpression.
 * Like a SimpleExpression, it has no side effects.
 */
class SimpleExpressionElement : public FormElement {
 public:
  explicit SimpleExpressionElement(SimpleExpression expr, int my_idx);

  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  bool is_sequence_point() const override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  //  void push_to_stack(const Env& env, FormStack& stack) override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
  void update_from_stack_identity(const Env& env,
                                  FormPool& pool,
                                  FormStack& stack,
                                  std::vector<FormElement*>* result,
                                  bool allow_side_effects);
  void update_from_stack_gpr_to_fpr(const Env& env,
                                    FormPool& pool,
                                    FormStack& stack,
                                    std::vector<FormElement*>* result,
                                    bool allow_side_effects);
  void update_from_stack_fpr_to_gpr(const Env& env,
                                    FormPool& pool,
                                    FormStack& stack,
                                    std::vector<FormElement*>* result,
                                    bool allow_side_effects);
  void update_from_stack_div_s(const Env& env,
                               FormPool& pool,
                               FormStack& stack,
                               std::vector<FormElement*>* result,
                               bool allow_side_effects);
  void update_from_stack_float_2(const Env& env,
                                 FixedOperatorKind kind,
                                 FormPool& pool,
                                 FormStack& stack,
                                 std::vector<FormElement*>* result,
                                 bool allow_side_effects);
  void update_from_stack_float_1(const Env& env,
                                 FixedOperatorKind kind,
                                 FormPool& pool,
                                 FormStack& stack,
                                 std::vector<FormElement*>* result,
                                 bool allow_side_effects);
  void update_from_stack_add_i(const Env& env,
                               FormPool& pool,
                               FormStack& stack,
                               std::vector<FormElement*>* result,
                               bool allow_side_effects);
  void update_from_stack_mult_si(const Env& env,
                                 FormPool& pool,
                                 FormStack& stack,
                                 std::vector<FormElement*>* result,
                                 bool allow_side_effects);
  void update_from_stack_lognot(const Env& env,
                                FormPool& pool,
                                FormStack& stack,
                                std::vector<FormElement*>* result,
                                bool allow_side_effects);
  void update_from_stack_force_si_2(const Env& env,
                                    FixedOperatorKind kind,
                                    FormPool& pool,
                                    FormStack& stack,
                                    std::vector<FormElement*>* result,
                                    bool allow_side_effects);
  void update_from_stack_force_ui_2(const Env& env,
                                    FixedOperatorKind kind,
                                    FormPool& pool,
                                    FormStack& stack,
                                    std::vector<FormElement*>* result,
                                    bool allow_side_effects);
  void update_from_stack_int_to_float(const Env& env,
                                      FormPool& pool,
                                      FormStack& stack,
                                      std::vector<FormElement*>* result,
                                      bool allow_side_effects);
  void update_from_stack_copy_first_int_2(const Env& env,
                                          FixedOperatorKind kind,
                                          FormPool& pool,
                                          FormStack& stack,
                                          std::vector<FormElement*>* result,
                                          bool allow_side_effects);

  const SimpleExpression& expr() const { return m_expr; }

 private:
  SimpleExpression m_expr;
  int m_my_idx;
};

/*!
 * Represents storing a value into memory.
 * This is only used as a placeholder if AtomicOpForm fails to convert it to something nicer.
 */
class StoreElement : public FormElement {
 public:
  explicit StoreElement(const StoreOp* op);

  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  // todo - we may eventually want to use a different representation for more
  // complicated store paths.
  const StoreOp* m_op;
};

/*!
 * Representing a value loaded from memory.  Not the destination.
 * Unclear if this should have some common base with store?
 */
class LoadSourceElement : public FormElement {
 public:
  LoadSourceElement(Form* addr, int size, LoadVarOp::Kind kind);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  int size() const { return m_size; }
  LoadVarOp::Kind kind() const { return m_kind; }
  const Form* location() const { return m_addr; }
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  Form* m_addr = nullptr;
  int m_size = -1;
  LoadVarOp::Kind m_kind;
};

/*!
 * Representing an indivisible thing, like an integer constant variable, etc.
 * Just a wrapper around SimpleAtom.
 */
class SimpleAtomElement : public FormElement {
 public:
  explicit SimpleAtomElement(const SimpleAtom& var);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  const SimpleAtom& atom() const { return m_atom; }
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  //  void push_to_stack(const Env& env, FormStack& stack) override;

 private:
  SimpleAtom m_atom;
};

/*!
 * Set a variable to a Form.  This is the set! form to be used for expression building.
 */
class SetVarElement : public FormElement {
 public:
  SetVarElement(const Variable& var,
                Form* value,
                bool is_sequence_point,
                const SetVarInfo& info = {});
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  bool is_sequence_point() const override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
  bool active() const override;

  const Variable& dst() const { return m_dst; }
  const Form* src() const { return m_src; }
  Form* src() { return m_src; }
  bool is_eliminated_coloring_move() const { return m_var_info.is_eliminated_coloring_move; }
  void eliminate_as_coloring_move() { m_var_info.is_eliminated_coloring_move = true; }

  bool is_dead_set() const { return m_var_info.is_dead_set; }
  void mark_as_dead_set() { m_var_info.is_dead_set = true; }

  bool is_dead_false_set() const { return m_var_info.is_dead_false; }
  void mark_as_dead_false() { m_var_info.is_dead_false = true; }

  const SetVarInfo& info() const { return m_var_info; }

 private:
  Variable m_dst;
  Form* m_src = nullptr;
  bool m_is_sequence_point = true;

  SetVarInfo m_var_info;
};

/*!
 * Like SetVar, but sets a form to another form.
 * This is intended to be used with stores.
 * NOTE: do not use this when SetVarElement could be used instead.
 */
class SetFormFormElement : public FormElement {
 public:
  SetFormFormElement(Form* dst, Form* src);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  bool is_sequence_point() const override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;

  const Form* src() const { return m_src; }
  const Form* dst() const { return m_dst; }
  Form* src() { return m_src; }
  Form* dst() { return m_dst; }

 private:
  Form* m_dst = nullptr;
  Form* m_src = nullptr;
};

/*!
 * A wrapper around a single AtomicOp.
 * The "important" special AtomicOps have their own Form type, like FuncitonCallElement.
 */
class AtomicOpElement : public FormElement {
 public:
  explicit AtomicOpElement(const AtomicOp* op);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
  const AtomicOp* op() const { return m_op; }

 private:
  const AtomicOp* m_op;
};

/*!
 * A wrapper around a single AsmOp
 */
class AsmOpElement : public FormElement {
 public:
  explicit AsmOpElement(const AsmOp* op);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
  const AsmOp* op() const { return m_op; }

 private:
  const AsmOp* m_op;
};

/*!
 * A "condition" like (< a b). This can be used as a boolean value directly: (set! a (< b c))
 * or it can be used as a branch condition: (if (< a b)).
 *
 * In the first case, it can be either a conditional move or actually branching. GOAL seems to use
 * the branching when sometimes it could have used the conditional move, and for now, we don't
 * care about the difference.
 */
class ConditionElement : public FormElement {
 public:
  ConditionElement(IR2_Condition::Kind kind,
                   std::optional<SimpleAtom> src0,
                   std::optional<SimpleAtom> src1,
                   RegSet consumed,
                   bool flipped);
  goos::Object to_form_internal(const Env& env) const override;
  goos::Object to_form_as_condition_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
  void invert();
  const RegSet& consume() const { return m_consumed; }

  FormElement* make_generic(const Env& env,
                            FormPool& pool,
                            const std::vector<Form*>& source_forms,
                            const std::vector<TypeSpec>& types);

 private:
  IR2_Condition::Kind m_kind;
  std::optional<SimpleAtom> m_src[2];
  RegSet m_consumed;
  bool m_flipped;
};

/*!
 * Wrapper around an AtomicOp call.
 */
class FunctionCallElement : public FormElement {
 public:
  explicit FunctionCallElement(const CallOp* op);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  const CallOp* m_op;
};

/*!
 * Wrapper around an AtomicOp branch.  These are inserted when directly converting blocks to Form,
 * but should be eliminated after the cfg_builder pass completes.
 */
class BranchElement : public FormElement {
 public:
  explicit BranchElement(const BranchOp* op);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  const BranchOp* op() const { return m_op; }

 private:
  const BranchOp* m_op;
};

/*!
 * Represents a (return-from #f x) form, which immediately returns from the function.
 * This always has some "dead code" after it that can't be reached, which is the "dead_code".
 * We store the dead code because it may contain an unreachable jump to the next place that can
 * be stripped away in later analysis passes. Or they may have written code after the return.
 */
class ReturnElement : public FormElement {
 public:
  Form* return_code = nullptr;
  Form* dead_code = nullptr;
  ReturnElement(Form* _return_code, Form* _dead_code)
      : return_code(_return_code), dead_code(_dead_code) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * Represents a (return-from Lxxx x) form, which returns from a block which ends before the end
 * of the function.  These are used pretty rarely. As a result, I'm not planning to allow these to
 * next within other expressions. This means that the following code:
 *
 * (set! x (block my-block
 *           (if (condition?)
 *               (return-from my-block 12))
 *         2))
 *
 * Would become
 *
 * (block my-block
 *   (when (condition?)
 *         (set! x 12)
 *         (return-from my-block none))
 *   (set! x 2)
 * )
 *
 * which seems fine to me.
 */
class BreakElement : public FormElement {
 public:
  Form* return_code = nullptr;
  Form* dead_code = nullptr;
  BreakElement(Form* _return_code, Form* _dead_code)
      : return_code(_return_code), dead_code(_dead_code) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * Condition (cond, if, when, unless) which has an "else" case.
 * The condition of the first entry may contain too much and will need to be adjusted later.
 * Example:
 *
 * (set! x 10)
 * (if (something?) ... )
 *
 * might become
 * (if (begin (set! x 10) (something?)) ... )
 *
 * We want to wait until after expressions are built to move the extra stuff up to avoid splitting
 * up a complicated expression used as the condition.  But this should happen before variable
 * scoping.
 */
class CondWithElseElement : public FormElement {
 public:
  struct Entry {
    Form* condition = nullptr;
    Form* body = nullptr;
    bool cleaned = false;
  };
  std::vector<Entry> entries;
  Form* else_ir = nullptr;
  bool already_rewritten = false;
  CondWithElseElement(std::vector<Entry> _entries, Form* _else_ir)
      : entries(std::move(_entries)), else_ir(_else_ir) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * An empty element. This is used to fill the body of control forms with nothing in them.
 * For example, I believe that (cond ((x y) (else none))) will generate an else case with an
 * "empty" and looks different from (cond ((x y))).
 *
 * We _could_ simplify out the use of empty, but I think it's more "authentic" to leave them in, and
 * might give us more clues about how the code was originally written
 */
class EmptyElement : public FormElement {
 public:
  EmptyElement() = default;
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
};

/*!
 * Represents a GOAL while loop and more complicated loops which have the "while" format of checking
 * the condition before the first loop. This will not include infinite while loops.
 * Unlike CondWithElseElement, this will correctly identify the start and end of the condition.
 */
class WhileElement : public FormElement {
 public:
  WhileElement(Form* _condition, Form* _body) : condition(_condition), body(_body) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
  Form* condition = nullptr;
  Form* body = nullptr;
  bool cleaned = false;
};

/*!
 * Represents a GOAL until loop and more complicated loops which use the "until" format of checking
 * the condition after the first iteration. Has the same limitation as CondWithElseElement for the
 * condition.
 */
class UntilElement : public FormElement {
 public:
  UntilElement(Form* _condition, Form* _body) : condition(_condition), body(_body) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
  Form* condition = nullptr;
  Form* body = nullptr;
};

/*!
 * Represents a GOAL short-circuit expression, either AND or OR.
 * The first "element" in ShortCircuitElement may be too large, see the comment on
 * CondWithElseElement
 */
class ShortCircuitElement : public FormElement {
 public:
  struct Entry {
    Form* condition = nullptr;
    std::optional<SetVarOp> branch_delay;
    // in the case where there's no else, each delay slot will write #f to the "output" register.
    // this can be with an or <output>, s7, r0
    //    Form* output = nullptr; // todo, what? add to collect vars if we need it?
    bool is_output_trick = false;
    bool cleaned = false;
  };

  enum Kind { UNKNOWN, AND, OR } kind = UNKNOWN;

  Variable final_result;
  std::vector<Entry> entries;
  std::optional<bool> used_as_value = std::nullopt;
  bool already_rewritten = false;

  explicit ShortCircuitElement(std::vector<Entry> _entries) : entries(std::move(_entries)) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * Represents a GOAL cond/if/when/unless statement which does not have an explicit else case. The
 * compiler will then move #f into the result register in the delay slot. The first condition may be
 * too large at first, see CondWithElseElement
 */
class CondNoElseElement : public FormElement {
 public:
  struct Entry {
    Form* condition = nullptr;
    Form* body = nullptr;
    std::optional<Variable> false_destination;
    FormElement* original_condition_branch = nullptr;
    bool cleaned = false;
  };
  Variable final_destination;
  bool used_as_value = false;
  bool already_rewritten = false;
  std::vector<Entry> entries;
  explicit CondNoElseElement(std::vector<Entry> _entries) : entries(std::move(_entries)) {}
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * Represents a (abs x) expression.
 */
class AbsElement : public FormElement {
 public:
  explicit AbsElement(Variable _source, RegSet _consumed);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
  Variable source;
  RegSet consumed;
};

/*!
 * Represents an (ash x y) expression. There is also an "unsigned" version of this using logical
 * shifts. This only recognizes the fancy version where the shift amount isn't known at compile time
 * and the compiler emits code that branches depending on the sign of the shift amount.
 */
class AshElement : public FormElement {
 public:
  Variable shift_amount, value;
  std::optional<Variable> clobber;
  bool is_signed = true;
  RegSet consumed;
  AshElement(Variable _shift_amount,
             Variable _value,
             std::optional<Variable> _clobber,
             bool _is_signed,
             RegSet _consumed);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
};

/*!
 * Represents a form which gets the runtime type of a boxed object. This is for the most general
 * "object" case where we check for pair, binteger, or basic and there's actually branching.
 */
class TypeOfElement : public FormElement {
 public:
  Form* value;
  std::optional<Variable> clobber;
  TypeOfElement(Form* _value, std::optional<Variable> _clobber);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
};

/*!
 * Represents an unpaired cmove #f.  GOAL may emit code like
 * (set! x #t)
 * (... evaluate something)
 * (cmov x y #f)
 * where the stuff in between is potentially very large.
 * GOAL has no "condition move" keyword available to the programmer - this would only happen if when
 * doing something like (set! x (zero? y)), in the code for creating a GOAL boolean.
 *
 * Code like (if x (set! y z)) will branch, the compiler isn't smart enough to use movn/movz here.
 *
 * These cannot be compacted into a single form until expression building, so we leave these
 * placeholders in.
 *
 * Note - some conditionals put the (set! x #t) immediately before the cmove, but not all. Those
 * that do will be correctly recognized and will be a ConditionElement. zero! seems to be the most
 * common one that's split, and it happens reasonably often, so I will try to actually correct it.
 */
class ConditionalMoveFalseElement : public FormElement {
 public:
  Variable dest;
  Form* source = nullptr;
  bool on_zero = false;
  ConditionalMoveFalseElement(Variable _dest, Form* _source, bool _on_zero);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
};

std::string fixed_operator_to_string(FixedOperatorKind kind);

/*!
 * A GenericOperator is the head of a GenericElement.
 * It is used for the final output.
 */
class GenericOperator {
 public:
  enum class Kind { FIXED_OPERATOR, CONDITION_OPERATOR, FUNCTION_EXPR, INVALID };

  static GenericOperator make_fixed(FixedOperatorKind kind);
  static GenericOperator make_function(Form* value);
  static GenericOperator make_compare(IR2_Condition::Kind kind);
  void collect_vars(VariableSet& vars) const;
  goos::Object to_form(const Env& env) const;
  void apply(const std::function<void(FormElement*)>& f);
  void apply_form(const std::function<void(Form*)>& f);
  bool operator==(const GenericOperator& other) const;
  bool operator!=(const GenericOperator& other) const;
  void get_modified_regs(RegSet& regs) const;
  Kind kind() const { return m_kind; }
  FixedOperatorKind fixed_kind() const {
    assert(m_kind == Kind::FIXED_OPERATOR);
    return m_fixed_kind;
  }

  IR2_Condition::Kind condition_kind() const {
    assert(m_kind == Kind::CONDITION_OPERATOR);
    return m_condition_kind;
  }

  const Form* func() const {
    assert(m_kind == Kind::FUNCTION_EXPR);
    return m_function;
  }

  Form* func() {
    assert(m_kind == Kind::FUNCTION_EXPR);
    return m_function;
  }

 private:
  friend class GenericElement;
  Kind m_kind = Kind::INVALID;
  IR2_Condition::Kind m_condition_kind = IR2_Condition::Kind::INVALID;
  FixedOperatorKind m_fixed_kind = FixedOperatorKind::INVALID;
  Form* m_function = nullptr;
};

class GenericElement : public FormElement {
 public:
  explicit GenericElement(GenericOperator op);
  GenericElement(GenericOperator op, Form* arg);
  GenericElement(GenericOperator op, Form* arg0, Form* arg1);
  GenericElement(GenericOperator op, std::vector<Form*> forms);

  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;
  void push_to_stack(const Env& env, FormPool& pool, FormStack& stack) override;
  const GenericOperator& op() const { return m_head; }
  GenericOperator& op() { return m_head; }
  const std::vector<Form*>& elts() const { return m_elts; }

 private:
  GenericOperator m_head;
  std::vector<Form*> m_elts;
};

class CastElement : public FormElement {
 public:
  explicit CastElement(TypeSpec type, Form* source, bool numeric = false);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  const TypeSpec& type() const { return m_type; }
  const Form* source() const { return m_source; }
  Form* source() { return m_source; }

 private:
  TypeSpec m_type;
  Form* m_source = nullptr;
  bool m_numeric = false;  // if true, use the. otherwise the-as
};

class DerefToken {
 public:
  enum class Kind {
    INTEGER_CONSTANT,
    INTEGER_EXPRESSION,  // some form which evaluates to an integer index. Not offset, index.
    FIELD_NAME,
    EXPRESSION_PLACEHOLDER,
    INVALID
  };
  static DerefToken make_int_constant(s64 int_constant);
  static DerefToken make_int_expr(Form* expr);
  static DerefToken make_field_name(const std::string& name);
  static DerefToken make_expr_placeholder();

  void collect_vars(VariableSet& vars) const;
  goos::Object to_form(const Env& env) const;
  void apply(const std::function<void(FormElement*)>& f);
  void apply_form(const std::function<void(Form*)>& f);
  void get_modified_regs(RegSet& regs) const;

  Kind kind() const { return m_kind; }
  const std::string& field_name() const {
    assert(m_kind == Kind::FIELD_NAME);
    return m_name;
  }

  Form* expr() {
    assert(m_kind == Kind::INTEGER_EXPRESSION);
    return m_expr;
  }

 private:
  Kind m_kind = Kind::INVALID;
  s64 m_int_constant = -1;
  std::string m_name;
  Form* m_expr = nullptr;
};

class DerefElement : public FormElement {
 public:
  DerefElement(Form* base, bool is_addr_of, DerefToken token);
  DerefElement(Form* base, bool is_addr_of, std::vector<DerefToken> tokens);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;

  bool is_addr_of() const { return m_is_addr_of; }
  const Form* base() const { return m_base; }
  Form* base() { return m_base; }
  const std::vector<DerefToken>& tokens() const { return m_tokens; }

 private:
  Form* m_base = nullptr;
  bool m_is_addr_of = false;
  std::vector<DerefToken> m_tokens;
};

class DynamicMethodAccess : public FormElement {
 public:
  explicit DynamicMethodAccess(Variable source);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  Variable m_source;
};

class ArrayFieldAccess : public FormElement {
 public:
  ArrayFieldAccess(Variable source,
                   const std::vector<DerefToken>& deref_tokens,
                   int expected_stride,
                   int constant_offset);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void update_from_stack(const Env& env,
                         FormPool& pool,
                         FormStack& stack,
                         std::vector<FormElement*>* result,
                         bool allow_side_effects) override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  Variable m_source;
  std::vector<DerefToken> m_deref_tokens;
  int m_expected_stride = -1;
  int m_constant_offset = -1;
};

class GetMethodElement : public FormElement {
 public:
  GetMethodElement(Form* in, std::string name, bool is_object);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  Form* m_in = nullptr;
  std::string m_name;
  bool m_is_object = false;
};

class StringConstantElement : public FormElement {
 public:
  StringConstantElement(const std::string& value);
  goos::Object to_form_internal(const Env& env) const override;
  void apply(const std::function<void(FormElement*)>& f) override;
  void apply_form(const std::function<void(Form*)>& f) override;
  void collect_vars(VariableSet& vars) const override;
  void get_modified_regs(RegSet& regs) const override;

 private:
  std::string m_value;
};

/*!
 * A Form is a wrapper around one or more FormElements.
 * This is done for two reasons:
 *  - Easier to "inline" begins, prevents stupid nesting of begins.
 *  - Easier to manage ownership.
 */
class Form {
 public:
  Form() = default;
  Form(FormElement* parent, FormElement* single_child)
      : parent_element(parent), m_elements({single_child}) {
    single_child->parent_form = this;
  }

  Form(FormElement* parent, const std::vector<FormElement*>& sequence)
      : parent_element(parent), m_elements(sequence) {
    for (auto& x : sequence) {
      x->parent_form = this;
    }
  }

  FormElement* try_as_single_element() const {
    if (is_single_element()) {
      return m_elements.front();
    }
    return nullptr;
  }
  bool is_single_element() const { return m_elements.size() == 1; }
  FormElement* operator[](int idx) { return m_elements.at(idx); }
  FormElement*& at(int idx) { return m_elements.at(idx); }
  const FormElement* operator[](int idx) const { return m_elements.at(idx); }
  int size() const { return int(m_elements.size()); }
  FormElement* back() const {
    assert(!m_elements.empty());
    return m_elements.back();
  }

  FormElement** back_ref() {
    assert(!m_elements.empty());
    return &m_elements.back();
  }

  void pop_back() {
    assert(!m_elements.empty());
    m_elements.pop_back();
  }

  const std::vector<FormElement*>& elts() const { return m_elements; }
  std::vector<FormElement*>& elts() { return m_elements; }

  void push_back(FormElement* elt) {
    elt->parent_form = this;
    m_elements.push_back(elt);
  }

  void clear() { m_elements.clear(); }

  goos::Object to_form(const Env& env) const;
  goos::Object to_form_as_condition(const Env& env) const;
  std::string to_string(const Env& env) const;
  void inline_forms(std::vector<goos::Object>& forms, const Env& env) const;
  void apply(const std::function<void(FormElement*)>& f);
  void apply_form(const std::function<void(Form*)>& f);
  void collect_vars(VariableSet& vars) const;

  void update_children_from_stack(const Env& env,
                                  FormPool& pool,
                                  FormStack& stack,
                                  bool allow_side_effects);
  bool has_side_effects();
  void get_modified_regs(RegSet& regs) const;

  FormElement* parent_element = nullptr;

 private:
  std::vector<FormElement*> m_elements;
};

/*!
 * A FormPool is used to allocate forms and form elements.
 * It will clean up everything when it is destroyed.
 * As a result, you don't need to worry about deleting / referencing counting when manipulating
 * a Form graph.
 */
class FormPool {
 public:
  template <typename T, class... Args>
  T* alloc_element(Args&&... args) {
    auto elt = new T(std::forward<Args>(args)...);
    m_elements.emplace_back(elt);
    return elt;
  }

  template <typename T, class... Args>
  Form* alloc_single_element_form(FormElement* parent, Args&&... args) {
    auto elt = new T(std::forward<Args>(args)...);
    m_elements.emplace_back(elt);
    auto form = alloc_single_form(parent, elt);
    return form;
  }

  Form* alloc_single_form(FormElement* parent, FormElement* elt) {
    auto form = new Form(parent, elt);
    m_forms.push_back(form);
    return form;
  }

  Form* alloc_sequence_form(FormElement* parent, const std::vector<FormElement*> sequence) {
    auto form = new Form(parent, sequence);
    m_forms.push_back(form);
    return form;
  }

  Form* acquire(std::unique_ptr<Form> form_ptr) {
    Form* form = form_ptr.release();
    m_forms.push_back(form);
    return form;
  }

  Form* alloc_empty_form() {
    Form* form = new Form;
    m_forms.push_back(form);
    return form;
  }

  ~FormPool();

 private:
  std::vector<Form*> m_forms;
  std::vector<FormElement*> m_elements;
};
}  // namespace decompiler
