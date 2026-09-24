// PA3 controlling-expression evaluator over the shared PA1 phase 1--3 scanner.
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "pa3_expression.h"
#include "preprocess/pp_tokenizer.h"
#include "preprocess/tokens/IPPTokenStream.h"

namespace {

struct ExpressionLines : IPPTokenStream {
  std::vector<PA3Token> tokens;

  void evaluate_line() {
    if (tokens.empty())
      return;
    try {
      std::cout << FormatPA3Value(EvaluatePA3Expression(tokens)) << '\n';
    } catch (const std::exception &) {
      std::cout << "error\n";
    } catch (...) {
      std::cout << "error\n";
    }
    tokens.clear();
  }
  void emit_whitespace_sequence() {}
  void emit_new_line() { evaluate_line(); }
  void emit_header_name(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Invalid, s));
  }
  void emit_identifier(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Identifier, s));
  }
  void emit_pp_number(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Number, s));
  }
  void emit_character_literal(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Character, s));
  }
  void emit_user_defined_character_literal(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Invalid, s));
  }
  void emit_string_literal(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Invalid, s));
  }
  void emit_user_defined_string_literal(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Invalid, s));
  }
  void emit_preprocessing_op_or_punc(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Punctuator, s));
  }
  void emit_non_whitespace_char(const std::string &s) {
    tokens.push_back(PA3Token(PA3Token::Invalid, s));
  }
  void emit_eof() { evaluate_line(); }
};

bool has_batch_stdin_arg(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (std::string(argv[i]) == "--batch-stdin")
      return true;
  return false;
}

int run_batch_stdin() {
  // Match the text-test batch worker protocol: output<TAB>stderr<TAB>input.
  std::string record;
  while (std::getline(std::cin, record)) {
    if (record.empty())
      continue;
    std::size_t a = record.find('\t'),
                b = a == std::string::npos ? a : record.find('\t', a + 1);
    if (a == std::string::npos || b == std::string::npos ||
        record.find('\t', b + 1) != std::string::npos) {
      std::cout << "EXIT_FAILURE\n";
      continue;
    }
    const std::string outpath = record.substr(0, a),
                      errpath = record.substr(a + 1, b - a - 1),
                      inpath = record.substr(b + 1);
    std::ifstream input(inpath.c_str(), std::ios::binary);
    std::ofstream output(outpath.c_str(), std::ios::binary);
    if (!input || !output) {
      std::ofstream err(errpath.c_str());
      err << "ERROR: cannot read input\n";
      std::cout << "EXIT_FAILURE\n";
      continue;
    }
    std::ostringstream source;
    source << input.rdbuf();
    std::streambuf *old = std::cout.rdbuf(output.rdbuf());
    int status = EXIT_SUCCESS;
    try {
      ExpressionLines lines;
      ScanPreprocessingTokens(source.str(), lines);
      std::cout << "eof\n";
    } catch (const std::exception &e) {
      std::ofstream err(errpath.c_str());
      err << "ERROR: " << e.what() << '\n';
      status = EXIT_FAILURE;
    }
    std::cout.rdbuf(old);
    std::cout << (status ? "EXIT_FAILURE\n" : "EXIT_SUCCESS\n");
  }
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char **argv) {
  if (has_batch_stdin_arg(argc, argv))
    return run_batch_stdin();
  std::ostringstream source;
  source << std::cin.rdbuf();
  try {
    ExpressionLines lines;
    ScanPreprocessingTokens(source.str(), lines);
    std::cout << "eof\n";
    return EXIT_SUCCESS;
  } catch (const std::exception &e) {
    std::cerr << "ERROR: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
