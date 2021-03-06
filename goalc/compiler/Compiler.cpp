#include "Compiler.h"
#include "common/link_types.h"
#include "IR.h"
#include "goalc/regalloc/allocate.h"
#include "third-party/fmt/core.h"
#include "CompilerException.h"
#include <chrono>
#include <thread>

using namespace goos;

Compiler::Compiler() : m_debugger(&m_listener) {
  m_listener.add_debugger(&m_debugger);
  m_ts.add_builtin_types();
  m_global_env = std::make_unique<GlobalEnv>();
  m_none = std::make_unique<None>(m_ts.make_typespec("none"));

  // todo - compile library
  Object library_code = m_goos.reader.read_from_file({"goal_src", "goal-lib.gc"});
  compile_object_file("goal-lib", library_code, false);
}

void Compiler::execute_repl() {
  while (!m_want_exit) {
    try {
      // 1). get a line from the user (READ)
      std::string prompt = "g";
      if (m_listener.is_connected()) {
        prompt += "c";
      } else {
        prompt += " ";
      }

      if (m_debugger.is_halted()) {
        prompt += "s";
      } else if (m_debugger.is_attached()) {
        prompt += "r";
      } else {
        prompt += " ";
      }

      Object code = m_goos.reader.read_from_stdin(prompt);

      // 2). compile
      auto obj_file = compile_object_file("repl", code, m_listener.is_connected());
      if (m_settings.debug_print_ir) {
        obj_file->debug_print_tl();
      }

      if (!obj_file->is_empty()) {
        // 3). color
        color_object_file(obj_file);

        // 4). codegen
        auto data = codegen_object_file(obj_file);

        // 4). send!
        if (m_listener.is_connected()) {
          m_listener.send_code(data);
          if (!m_listener.most_recent_send_was_acked()) {
            print_compiler_warning("Runtime is not responding. Did it crash?\n");
          }
        }
      }

    } catch (std::exception& e) {
      print_compiler_warning("REPL Error: {}\n", e.what());
    }
  }

  m_listener.disconnect();
}

FileEnv* Compiler::compile_object_file(const std::string& name,
                                       goos::Object code,
                                       bool allow_emit) {
  auto file_env = m_global_env->add_file(name);
  Env* compilation_env = file_env;
  if (!allow_emit) {
    compilation_env = file_env->add_no_emit_env();
  }

  file_env->add_top_level_function(
      compile_top_level_function("top-level", std::move(code), compilation_env));

  if (!allow_emit && !file_env->is_empty()) {
    throw std::runtime_error("Compilation generated code, but wasn't supposed to");
  }

  return file_env;
}

std::unique_ptr<FunctionEnv> Compiler::compile_top_level_function(const std::string& name,
                                                                  const goos::Object& code,
                                                                  Env* env) {
  auto fe = std::make_unique<FunctionEnv>(env, name);
  fe->set_segment(TOP_LEVEL_SEGMENT);

  auto result = compile_error_guard(code, fe.get());

  // only move to return register if we actually got a result
  if (!dynamic_cast<const None*>(result)) {
    fe->emit(std::make_unique<IR_Return>(fe->make_gpr(result->type()), result->to_gpr(fe.get())));
  }

  fe->finish();
  return fe;
}

Val* Compiler::compile_error_guard(const goos::Object& code, Env* env) {
  try {
    return compile(code, env);
  } catch (CompilerException& ce) {
    if (ce.print_err_stack) {
      auto obj_print = code.print();
      if (obj_print.length() > 80) {
        obj_print = obj_print.substr(0, 80);
        obj_print += "...";
      }
      bool term;
      fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Location:\n");
      fmt::print(m_goos.reader.db.get_info_for(code, &term));
      fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Code:\n");
      fmt::print("{}\n", obj_print);
      if (term) {
        ce.print_err_stack = false;
      }
      std::string line(80, '-');
      line.push_back('\n');
      fmt::print(line);
    }
    throw ce;
  }

  catch (std::runtime_error& e) {
    auto obj_print = code.print();
    if (obj_print.length() > 80) {
      obj_print = obj_print.substr(0, 80);
      obj_print += "...";
    }
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Location:\n");
    fmt::print(m_goos.reader.db.get_info_for(code));
    fmt::print(fg(fmt::color::yellow) | fmt::emphasis::bold, "Code:\n");
    fmt::print("{}\n", obj_print);
    std::string line(80, '-');
    line.push_back('\n');
    fmt::print(line);
    throw e;
  }
}

void Compiler::color_object_file(FileEnv* env) {
  for (auto& f : env->functions()) {
    AllocationInput input;
    input.is_asm_function = f->is_asm_func;
    for (auto& i : f->code()) {
      input.instructions.push_back(i->to_rai());
      input.debug_instruction_names.push_back(i->print());
    }

    input.max_vars = f->max_vars();
    input.constraints = f->constraints();
    input.stack_slots_for_stack_vars = f->stack_slots_used_for_stack_vars();

    if (m_settings.debug_print_regalloc) {
      input.debug_settings.print_input = true;
      input.debug_settings.print_result = true;
      input.debug_settings.print_analysis = true;
      input.debug_settings.allocate_log_level = 2;
    }

    f->set_allocations(allocate_registers(input));
  }
}

std::vector<u8> Compiler::codegen_object_file(FileEnv* env) {
  try {
    auto debug_info = &m_debugger.get_debug_info_for_object(env->name());
    debug_info->clear();
    CodeGenerator gen(env, debug_info);
    bool ok = true;
    auto result = gen.run(&m_ts);
    for (auto& f : env->functions()) {
      if (f->settings.print_asm) {
        fmt::print("{}\n", debug_info->disassemble_function_by_name(f->name(), &ok));
      }
    }
    return result;
  } catch (std::exception& e) {
    throw_compiler_error_no_code("Error during codegen: {}", e.what());
  }
  return {};
}

bool Compiler::codegen_and_disassemble_object_file(FileEnv* env,
                                                   std::vector<u8>* data_out,
                                                   std::string* asm_out) {
  auto debug_info = &m_debugger.get_debug_info_for_object(env->name());
  debug_info->clear();
  CodeGenerator gen(env, debug_info);
  *data_out = gen.run(&m_ts);
  bool ok = true;
  *asm_out = debug_info->disassemble_all_functions(&ok);
  return ok;
}

void Compiler::compile_and_send_from_string(const std::string& source_code) {
  if (!connect_to_target()) {
    throw std::runtime_error(
        "Compiler failed to connect to target for compile_and_send_from_string.");
  }

  auto code = m_goos.reader.read_from_string(source_code);
  auto compiled = compile_object_file("test-code", code, true);
  assert(!compiled->is_empty());
  color_object_file(compiled);
  auto data = codegen_object_file(compiled);
  m_listener.send_code(data);
  if (!m_listener.most_recent_send_was_acked()) {
    print_compiler_warning("Runtime is not responding after sending test code. Did it crash?\n");
  }
}

std::vector<std::string> Compiler::run_test_from_file(const std::string& source_code) {
  try {
    if (!connect_to_target()) {
      throw std::runtime_error("Compiler::run_test_from_file couldn't connect!");
    }

    auto code = m_goos.reader.read_from_file({source_code});
    auto compiled = compile_object_file("test-code", code, true);
    if (compiled->is_empty()) {
      return {};
    }
    color_object_file(compiled);
    auto data = codegen_object_file(compiled);
    m_listener.record_messages(ListenerMessageKind::MSG_PRINT);
    m_listener.send_code(data);
    if (!m_listener.most_recent_send_was_acked()) {
      print_compiler_warning("Runtime is not responding after sending test code. Did it crash?\n");
    }
    return m_listener.stop_recording_messages();
  } catch (std::exception& e) {
    fmt::print("[Compiler] Failed to compile test program {}: {}\n", source_code, e.what());
    throw e;
  }
}

std::vector<std::string> Compiler::run_test_from_string(const std::string& src,
                                                        const std::string& obj_name) {
  try {
    if (!connect_to_target()) {
      throw std::runtime_error("Compiler::run_test_from_file couldn't connect!");
    }

    auto code = m_goos.reader.read_from_string({src});
    auto compiled = compile_object_file(obj_name, code, true);
    if (compiled->is_empty()) {
      return {};
    }
    color_object_file(compiled);
    auto data = codegen_object_file(compiled);
    m_listener.record_messages(ListenerMessageKind::MSG_PRINT);
    m_listener.send_code(data);
    if (!m_listener.most_recent_send_was_acked()) {
      print_compiler_warning("Runtime is not responding after sending test code. Did it crash?\n");
    }
    return m_listener.stop_recording_messages();
  } catch (std::exception& e) {
    fmt::print("[Compiler] Failed to compile test program from string {}: {}\n", src, e.what());
    throw e;
  }
}

bool Compiler::connect_to_target() {
  if (!m_listener.is_connected()) {
    for (int i = 0; i < 1000; i++) {
      m_listener.connect_to_target();
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
      if (m_listener.is_connected()) {
        break;
      }
    }
    if (!m_listener.is_connected()) {
      return false;
    }
  }
  return true;
}

void Compiler::run_front_end_on_string(const std::string& src) {
  auto code = m_goos.reader.read_from_string({src});
  compile_object_file("run-on-string", code, true);
}

std::vector<std::string> Compiler::run_test_no_load(const std::string& source_code) {
  auto code = m_goos.reader.read_from_file({source_code});
  compile_object_file("test-code", code, true);
  return {};
}

void Compiler::shutdown_target() {
  if (m_debugger.is_attached()) {
    m_debugger.detach();
  }

  if (m_listener.is_connected()) {
    m_listener.send_reset(true);
  }
}

void Compiler::typecheck(const goos::Object& form,
                         const TypeSpec& expected,
                         const TypeSpec& actual,
                         const std::string& error_message) {
  (void)form;
  if (!m_ts.typecheck(expected, actual, error_message, true, false)) {
    throw_compiler_error(form, "Typecheck failed");
  }
}

/*!
 * Like typecheck, but will allow Val* to be #f if the destination isn't a number.
 * Also will convert to register types for the type checking.
 */
void Compiler::typecheck_reg_type_allow_false(const goos::Object& form,
                                              const TypeSpec& expected,
                                              const Val* actual,
                                              const std::string& error_message) {
  if (!m_ts.typecheck(m_ts.make_typespec("number"), expected, "", false, false)) {
    auto as_sym_val = dynamic_cast<const SymbolVal*>(actual);
    if (as_sym_val && as_sym_val->name() == "#f") {
      return;
    }
  }
  typecheck(form, expected, coerce_to_reg_type(actual->type()), error_message);
}