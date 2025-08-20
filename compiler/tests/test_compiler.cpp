// ==============================================================================================
// ╦  ┬┌─┐┌─┐┌┐┌┌─┐┌─┐    Yaksha Programming Language
// ║  ││  ├┤ │││└─┐├┤     is Licensed with GPLv3 + extra terms. Please see below.
// ╩═╝┴└─┘└─┘┘└┘└─┘└─┘
// Note: libs - MIT license, runtime/3rd - various
// ==============================================================================================
// GPLv3:
//
// Yaksha - Programming Language.
// Copyright (C) 2020 - 2024 Bhathiya Perera
//
// This program is free software: you can redistribute it and/or modify it under the terms
// of the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see https://www.gnu.org/licenses/.
//
// ==============================================================================================
// Additional Terms:
//
// Please note that any commercial use of the programming language's compiler source code
// (everything except compiler/runtime, compiler/libs and compiler/3rd) require a written agreement
// with author of the language (Bhathiya Perera).
//
// If you are using it for an open source project, please give credits.
// Your own project must use GPLv3 license with these additional terms.
//
// You may use programs written in Yaksha/YakshaLisp for any legal purpose
// (commercial, open-source, closed-source, etc) as long as it agrees
// to the licenses of linked runtime libraries (see compiler/runtime/README.md).
//
// ==============================================================================================
#include "catch2/catch.hpp"
#include "compiler/codegen_c.h"
#include "compiler/multifile_compiler.h"
#include "tokenizer/tokenizer.h"
#include <string>
#include <sstream>
using namespace yaksha;
static void test_compile_yaka_file(const std::string &yaka_code_file) {
  std::string exe_path = get_my_exe_path();
  auto libs_path =
      std::filesystem::path(exe_path).parent_path().parent_path() / "libs";
  multifile_compiler mc{};
  codegen_c cg{};
  auto result = mc.compile(yaka_code_file, libs_path.string(), &cg);
  REQUIRE(result.failed_ == false);
  gc_pool<token> token_pool{};
  // --------------------------------------------
  // This is the snapshot file
  std::string c_code_file = yaka_code_file + ".c";
  // load snapshot --> myfile.yaka.c
  std::string snapshot_code = read_file(c_code_file);
#ifdef YAKSHA_OS_WINDOWS
  // fix windows line endings
  // (git might covert line endings to CRLF)
  replace_all(snapshot_code, "\r\n", "\n");
#endif
  std::vector<token *> token_snapshot;
  tokenizer c_code_snapshot{"output.c", snapshot_code, &token_pool};
  c_code_snapshot.tokenize();
  token_snapshot = c_code_snapshot.tokens_;
  // tokenize the generated code --> in memory
  tokenizer c_code{"output.c", result.code_, &token_pool};
  c_code.tokenize();
  // write snapshot .c file
  write_file(result.code_, c_code_file);
  // compare current tokens with snapshot
  auto tokens_equal_ignoring_gensym = [](const token* a, const token* b) -> bool {
    if (!a || !b) return a == b;
    if (a->file_ != b->file_) return false;
    if (a->line_ != b->line_) return false;
    if (a->pos_ != b->pos_) return false;
    // Ignore gensym token text if both start with "g_"
    if (!(a->token_.rfind("g_", 0) == 0 && b->token_.rfind("g_", 0) == 0)) {
      if (a->token_ != b->token_) return false;
    }
    if (a->type_ != b->type_) return false;
    return true;
  };
auto dump_token = [](const token* t) -> std::string {
  if (!t) return "<null>";
  std::ostringstream oss;
  oss << "{file:'" << t->file_
      << "', line:" << t->line_
      << ", pos:" << t->pos_
      << ", token:'" << t->token_
      << "', type:" << static_cast<int>(t->type_)
      << "}";
  return oss.str();
};

// Size check with explicit diff message
{
  const size_t parsed_sz = c_code.tokens_.size();
  const size_t expect_sz = token_snapshot.size();
  if (parsed_sz != expect_sz) {
    const size_t min_sz = std::min(parsed_sz, expect_sz);

    size_t first_diff = min_sz;
    for (size_t i = 0; i < min_sz; ++i) {
      if (!tokens_equal_ignoring_gensym(c_code.tokens_[i], token_snapshot[i])) {
        first_diff = i;
        break;
      }
    }

    std::ostringstream msg;
    msg << "Token count mismatch. Parsed=" << parsed_sz
        << " Expected=" << expect_sz << "\n";

    if (first_diff < min_sz) {
      msg << "First difference at index " << first_diff << "\n"
          << "Parsed : " << dump_token(c_code.tokens_[first_diff]) << "\n"
          << "Expected: " << dump_token(token_snapshot[first_diff]) << "\n";
    } else {
      msg << "All first " << min_sz
          << " tokens equal; difference due to extra/missing tokens.\n";
    }

    const size_t preview = 8;
    if (parsed_sz > expect_sz) {
      msg << "Extra tokens in parsed output starting at index " << expect_sz << ":\n";
      for (size_t i = expect_sz; i < std::min(parsed_sz, expect_sz + preview); ++i) {
        msg << "  + " << i << ": " << dump_token(c_code.tokens_[i]) << "\n";
      }
    } else {
      msg << "Missing tokens (present in expected) starting at index " << parsed_sz << ":\n";
      for (size_t i = parsed_sz; i < std::min(expect_sz, parsed_sz + preview); ++i) {
        msg << "  - " << i << ": " << dump_token(token_snapshot[i]) << "\n";
      }
    }

    REQUIRE_MESSAGE(false, msg.str());
  }
}

// Per-token comparison with explicit message on mismatch
for (size_t i = 0; i < token_snapshot.size(); i++) {
  auto parsed = c_code.tokens_[i];
  auto expected = token_snapshot[i];

  auto mk_msg = [&](const char* what) {
    std::ostringstream oss;
    oss << what << " mismatch at index " << i << "\n"
        << "Parsed : " << dump_token(parsed) << "\n"
        << "Expected: " << dump_token(expected) << "\n";
    return oss.str();
  };

  REQUIRE_MESSAGE(parsed->file_ == expected->file_, mk_msg("file"));
  REQUIRE_MESSAGE(parsed->line_ == expected->line_, mk_msg("line"));
  REQUIRE_MESSAGE(parsed->pos_ == expected->pos_, mk_msg("pos"));
  if (!(parsed->token_.rfind("g_", 0) == 0 && expected->token_.rfind("g_", 0) == 0)) {
    REQUIRE_MESSAGE(parsed->token_ == expected->token_, mk_msg("token"));
  }
  REQUIRE_MESSAGE(parsed->type_ == expected->type_, mk_msg("type"));
}
TEST_CASE("compiler: Hello World") {
  test_compile_yaka_file("../test_data/compiler_tests/test1.yaka");
}
TEST_CASE("compiler: Defer") {
  test_compile_yaka_file("../test_data/compiler_tests/test2.yaka");
}
TEST_CASE("compiler: Class support") {
  test_compile_yaka_file("../test_data/compiler_tests/test3.yaka");
}
TEST_CASE("compiler: Create object from class") {
  test_compile_yaka_file("../test_data/compiler_tests/test4.yaka");
}
TEST_CASE("compiler: Object members") {
  test_compile_yaka_file("../test_data/compiler_tests/test5.yaka");
}
TEST_CASE("compiler: Array access") {
  test_compile_yaka_file("../test_data/compiler_tests/test6.yaka");
}
TEST_CASE("compiler: Nested array access") {
  test_compile_yaka_file("../test_data/compiler_tests/test7.yaka");
}
TEST_CASE("compiler: Void function") {
  test_compile_yaka_file("../test_data/compiler_tests/voidfunc.yaka");
}
TEST_CASE("compiler: Native functions") {
  test_compile_yaka_file("../test_data/compiler_tests/nativefunc.yaka");
}
TEST_CASE("compiler: Imports") {
  test_compile_yaka_file("../test_data/import_tests/main.yaka");
}
TEST_CASE("compiler: Native functions in imports") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/io_module_test/main.yaka");
}
TEST_CASE("compiler: Array methods") {
  test_compile_yaka_file("../test_data/compiler_tests/arrstack.yaka");
}
TEST_CASE("compiler: Native defines") {
  test_compile_yaka_file("../test_data/byol/lisp.yaka");
}
TEST_CASE("compiler: Do not copy str for getref") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/do_not_copy_str_getref.yaka");
}
TEST_CASE("compiler: Str unref and getref hacks!") {
  test_compile_yaka_file("../test_data/compiler_tests/str_getref_unref.yaka");
}
TEST_CASE("compiler: Test automatic generation for string hashes!") {
  test_compile_yaka_file("../test_data/compiler_tests/string_hash.yaka");
}
TEST_CASE("compiler: Test return calls a function with defer deleted stuff !") {
  test_compile_yaka_file("../test_data/compiler_tests/defer_return.yaka");
}
TEST_CASE("compiler: All @native stuff !") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/native_function_type_tests.yaka");
}
TEST_CASE("compiler: Test elif") {
  test_compile_yaka_file("../test_data/compiler_tests/elif_testing.yaka");
}
TEST_CASE("compiler: Casting") {
  test_compile_yaka_file("../test_data/compiler_tests/casting_test.yaka");
}
TEST_CASE("compiler: Basic function pointer") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/function_datatype_test.yaka");
}
TEST_CASE("compiler: Function pointer passing & calling") {
  test_compile_yaka_file("../test_data/compiler_tests/"
                         "function_datatype_passing_calling_test.yaka");
}
TEST_CASE("compiler: Test automatic generation for normal hashes!") {
  test_compile_yaka_file("../test_data/compiler_tests/normal_hash_map.yaka");
}
TEST_CASE("compiler: Test sorting functionality using qsort() !") {
  test_compile_yaka_file("../test_data/compiler_tests/sort_test.yaka");
}
TEST_CASE("compiler: Test arrnew() !") {
  test_compile_yaka_file("../test_data/compiler_tests/arrnew_test.yaka");
}
TEST_CASE("compiler: Test array() !") {
  test_compile_yaka_file("../test_data/compiler_tests/array_test.yaka");
}
TEST_CASE("compiler: Test sorting with @native functions") {
  test_compile_yaka_file("../test_data/compiler_tests/native_func_sort.yaka");
}
TEST_CASE("compiler: Test iif() builtin") {
  test_compile_yaka_file("../test_data/compiler_tests/iif_test.yaka");
}
TEST_CASE("compiler: Test foreach() builtin") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/functional_test_foreach.yaka");
}
TEST_CASE("compiler: Test countif() builtin") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/functional_test_countif.yaka");
}
TEST_CASE("compiler: Test map() builtin") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/functional_test_map.yaka");
}
TEST_CASE("compiler: Test filter() builtin") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/functional_test_filter.yaka");
}
TEST_CASE("compiler: Tuple data type") {
  test_compile_yaka_file("../test_data/compiler_tests/tuple_test.yaka");
}
TEST_CASE("compiler: Number literals") {
  test_compile_yaka_file("../test_data/compiler_tests/all_integers.yaka");
}
TEST_CASE("compiler: Operator test augment assign and bitwise") {
  test_compile_yaka_file("../test_data/compiler_tests/operator_test.yaka");
}
TEST_CASE("compiler: Global constants") {
  test_compile_yaka_file("../test_data/compiler_tests/global_constants.yaka");
}
TEST_CASE("compiler: None comparison") {
  test_compile_yaka_file("../test_data/compiler_tests/none_comparison.yaka");
}
TEST_CASE("compiler: Loops and logical operators") {
  test_compile_yaka_file("../test_data/compiler_tests/loops_and_logic.yaka");
}
TEST_CASE("compiler: Class stuff") {
  test_compile_yaka_file("../test_data/compiler_tests/class_stuff.yaka");
}
TEST_CASE("compiler: Test binarydata() builtin") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/binarydata_builtin_test.yaka");
}
TEST_CASE("compiler: Test native constants") {
  test_compile_yaka_file("../test_data/compiler_tests/native_constants.yaka");
}
TEST_CASE("compiler: Test @onstack class") {
  test_compile_yaka_file("../test_data/compiler_tests/on_stack_test.yaka");
}
TEST_CASE("compiler: Test endless for") {
  test_compile_yaka_file("../test_data/compiler_tests/endless_for.yaka");
}
TEST_CASE("compiler: Test println each element in for") {
  test_compile_yaka_file("../test_data/compiler_tests/eachelem_for.yaka");
}
TEST_CASE("compiler: bug-fix comment break indent") {
  test_compile_yaka_file("../test_data/bug_fixes/comment_break_indent.yaka");
}
TEST_CASE("compiler: macros - compile with macros!{} and dsl!{} macro usage") {
  test_compile_yaka_file(
      "../test_data/macro_tests/eachelem_for_with_macros.yaka");
}
TEST_CASE("compiler: macros - compile time fizzbuzz!{}") {
  test_compile_yaka_file("../test_data/macro_tests/comptime_fizz_buzz.yaka");
}
TEST_CASE("compiler: macros - load a file as a string") {
  test_compile_yaka_file("../test_data/macro_tests/grab_file.yaka");
}
TEST_CASE("compiler: macros - different kind of arguments") {
  test_compile_yaka_file("../test_data/macro_tests/m_args.yaka");
}
TEST_CASE("compiler: macros - use macros in other files in my macros") {
  test_compile_yaka_file("../test_data/macro_tests/i_import_stuff.yaka");
}
TEST_CASE("compiler: macros - use macro from another file as module.dsl!{}") {
  test_compile_yaka_file(
      "../test_data/macro_tests/imported_dsl_macro_use.yaka");
}
TEST_CASE("compiler: macros - gensym usage") {
  test_compile_yaka_file("../test_data/macro_tests/g1.yaka");
}
TEST_CASE("compiler: inlinec and make") {
  test_compile_yaka_file("../test_data/compiler_tests/inlinec_and_make.yaka");
}
TEST_CASE("compiler: inlinec & macros") {
  test_compile_yaka_file("../test_data/macro_tests/memory_writer.yaka");
}
TEST_CASE("compiler: strings - pass literal to sr") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/pass_literal_to_sr.yaka");
}
TEST_CASE("compiler: strings - pass str to sr and create str with literal") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/str_to_sr_create_str_lit.yaka");
}
TEST_CASE("compiler: strings - pass literal / sr to str") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/pass_lit_sr_to_str.yaka");
}
TEST_CASE("compiler: strings - variables using sr") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/variables_using_sr.yaka");
}
TEST_CASE("compiler: strings - concat str") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/concat_str.yaka");
}
TEST_CASE("compiler: strings - concat lit") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/concat_lit.yaka");
}
TEST_CASE("compiler: strings - concat sr") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/concat_sr.yaka");
}
TEST_CASE("compiler: strings - concat mixed") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/concat_mixed.yaka");
}
TEST_CASE("compiler: strings - literal comparison") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/literal_comparison.yaka");
}
TEST_CASE("compiler: strings - comparison of mixed str, sr, literals") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/compare_mixed.yaka");
}
TEST_CASE("compiler: strings - comparison with none") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/compare_with_none.yaka");
}
TEST_CASE("compiler: strings - sr functional countif") {
  test_compile_yaka_file("../test_data/compiler_tests/string_tests/"
                         "sr_functional_test_countif.yaka");
}
TEST_CASE("compiler: strings - sr functional filter") {
  test_compile_yaka_file("../test_data/compiler_tests/string_tests/"
                         "sr_functional_test_filter.yaka");
}
TEST_CASE("compiler: strings - sr functional foreach") {
  test_compile_yaka_file("../test_data/compiler_tests/string_tests/"
                         "sr_functional_test_foreach.yaka");
}
TEST_CASE("compiler: strings - sr functional map") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/sr_functional_test_map.yaka");
}
TEST_CASE("compiler: strings - returning strings") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/returning_strings.yaka");
}
TEST_CASE("compiler: carpntr code base") {
  test_compile_yaka_file("../carpntr/main.yaka");
}
TEST_CASE("compiler: autocasting") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/auto_casting/autocast.yaka");
}
TEST_CASE("compiler: doc sample - wind_tree") {
  test_compile_yaka_file("../test_data/document_samples/wind_tree.yaka");
}
TEST_CASE("compiler: doc sample - factorial") {
  test_compile_yaka_file("../test_data/document_samples/factorial.yaka");
}
TEST_CASE("compiler: doc sample - notes") {
  test_compile_yaka_file("../test_data/document_samples/notes.yaka");
}
TEST_CASE("compiler: doc sample - snake") {
  test_compile_yaka_file("../test_data/document_samples/snake_w4.yaka");
}
TEST_CASE("compiler: doc sample - threading test") {
  test_compile_yaka_file("../test_data/document_samples/threading_test.yaka");
}
TEST_CASE("compiler: c like for loop") {
  test_compile_yaka_file("../test_data/compiler_tests/for_loop_test.yaka");
}
TEST_CASE("compiler: auto cast bool count") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/auto_casting/autocastboolcount.yaka");
}
TEST_CASE("compiler: sr with const") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/string_tests/sr_with_const.yaka");
}
TEST_CASE("compiler: various string operations with const") {
  test_compile_yaka_file("../test_data/compiler_tests/string_tests/"
                         "strings_const_mixed_tests.yaka");
}
TEST_CASE("compiler: fixed arrays - simple test") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/arrays/simple_fixed_arr.yaka");
}
TEST_CASE("compiler: fixed arrays - foreach loop / sr") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/arrays/fixed_arr_loop.yaka");
}
TEST_CASE("compiler: fixed arrays - copying when assigned") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/arrays/fixed_array_copying.yaka");
}
TEST_CASE("compiler: fixed arrays - structure data types") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/arrays/fixed_arr_structure.yaka");
}
TEST_CASE("compiler: bug-fix - access struct str member") {
  test_compile_yaka_file("../test_data/bug_fixes/struct_str_return.yaka");
}
TEST_CASE("compiler: bug-fix - cast string literal should work as expected") {
  test_compile_yaka_file("../test_data/bug_fixes/easy_cstr.yaka");
}
TEST_CASE("compiler: directive - ccode") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/directives/directive_ccode.yaka");
}
TEST_CASE("compiler: directive - no_main/no_stdlib") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/directives/minimal_mode.yaka");
}
TEST_CASE("compiler: structures - depends on other structures") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/structs_arrays/cat_game.yaka");
}
TEST_CASE("compiler: enums - import and use enum") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/integer_enums/sample.yaka");
}
TEST_CASE("compiler: decl macro - decl s get_global_state()") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/directives/directives_and_decl_macors.yaka");
}
TEST_CASE("compiler: setref usage") {
  test_compile_yaka_file(
      "../test_data/compiler_tests/setref_test.yaka");
}
