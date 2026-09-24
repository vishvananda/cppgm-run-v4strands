#pragma once

#include <string>
#include <vector>

struct PA3Token {
  enum Kind { Identifier, Number, Character, Punctuator, Invalid } kind;
  std::string spelling;
  PA3Token(Kind k, const std::string &s) : kind(k), spelling(s) {}
};

struct PA3Value {
  unsigned long long bits;
  bool is_unsigned;
  PA3Value(unsigned long long b = 0, bool u = false)
      : bits(b), is_unsigned(u) {}
};

PA3Value EvaluatePA3Expression(const std::vector<PA3Token> &tokens);
std::string FormatPA3Value(const PA3Value &value);
