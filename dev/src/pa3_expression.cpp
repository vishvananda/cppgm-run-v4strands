#include "pa3_expression.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {
typedef unsigned long long U;
typedef long long S;

S as_signed(PA3Value v) { return static_cast<S>(v.bits); }

U arithmetic_shift_right(U bits, unsigned shift) {
  if (shift == 0)
    return bits;
  U shifted = bits >> shift;
  if (bits >> 63)
    shifted |= ~static_cast<U>(0) << (64 - shift);
  return shifted;
}

bool identifier_spelling(const PA3Token &t) {
  if (t.kind == PA3Token::Identifier)
    return true;
  // PA1 exposes alternative operator keywords via its operator callback. They
  // remain identifier spellings in preprocessing-token context.
  static const char *const identifiers[] = {
      "and", "and_eq", "bitand", "bitor", "compl", "delete", "new",
      "not", "not_eq", "or",     "or_eq", "xor",   "xor_eq", 0};
  if (t.kind != PA3Token::Punctuator)
    return false;
  for (const char *const *p = identifiers; *p; ++p)
    if (t.spelling == *p)
      return true;
  return false;
}

bool ident_token(const PA3Token &t) { return identifier_spelling(t); }

int digit_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

struct Parser {
  const std::vector<PA3Token> &ts;
  size_t p;
  explicit Parser(const std::vector<PA3Token> &tokens) : ts(tokens), p(0) {}
  bool at(const char *s) const { return p < ts.size() && ts[p].spelling == s; }
  bool take(const char *s) {
    if (at(s)) {
      ++p;
      return true;
    }
    return false;
  }
  const PA3Token &next() {
    if (p >= ts.size())
      throw std::runtime_error("expected expression");
    return ts[p++];
  }
  static PA3Value boolean(bool b) { return PA3Value(b ? 1 : 0, false); }

  PA3Value primary(bool eval) {
    if (take("(")) {
      PA3Value v = conditional(eval);
      if (!take(")"))
        throw std::runtime_error("missing )");
      return v;
    }
    const PA3Token &t = next();
    if (t.kind == PA3Token::Number)
      return integer(t.spelling);
    if (t.kind == PA3Token::Character)
      return character(t.spelling);
    if (ident_token(t)) {
      if (t.spelling == "defined") {
        bool paren = take("(");
        const PA3Token &id = next();
        if (!ident_token(id))
          throw std::runtime_error("defined operand");
        if (paren && !take(")"))
          throw std::runtime_error("defined paren");
        unsigned char first = id.spelling.empty()
                                  ? 0
                                  : static_cast<unsigned char>(id.spelling[0]);
        return PA3Value(first & 1 ? 1 : 0, false);
      }
      if (t.spelling == "true")
        return PA3Value(1, false);
      if (t.spelling == "false")
        return PA3Value(0, false);
      return PA3Value(0, false);
    }
    throw std::runtime_error("invalid primary");
  }

  static PA3Value integer(const std::string &s) {
    int base = 10;
    size_t i = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      base = 16;
      i = 2;
    } else if (s.size() > 1 && s[0] == '0') {
      base = 8;
      i = 0;
    }
    const size_t digits_begin = i;
    U value = 0;
    for (; i < s.size(); ++i) {
      int d = digit_value(s[i]);
      if (d < 0 || d >= base)
        break;
      if (value > (std::numeric_limits<U>::max() - static_cast<U>(d)) /
                      static_cast<U>(base))
        throw std::runtime_error("integer overflow");
      value = value * static_cast<U>(base) + static_cast<U>(d);
    }
    if (i == digits_begin)
      throw std::runtime_error("missing integer digits");
    bool u = false;
    unsigned long_count = 0;
    if (i < s.size() && (s[i] == 'u' || s[i] == 'U')) {
      u = true;
      ++i;
    }
    if (i < s.size() && (s[i] == 'l' || s[i] == 'L')) {
      const char long_case = s[i++];
      long_count = 1;
      if (i < s.size() && (s[i] == 'l' || s[i] == 'L')) {
        if (s[i] != long_case)
          throw std::runtime_error("mixed-case long-long suffix");
        ++i;
        long_count = 2;
      }
    }
    if (!u && i < s.size() && (s[i] == 'u' || s[i] == 'U')) {
      u = true;
      ++i;
    }
    if (i != s.size())
      throw std::runtime_error("invalid integer suffix");
    // Apply C++11's candidate type ordering on the Linux x86-64 data model.
    // PA3 then promotes that phase-7 type to intmax_t or uintmax_t without
    // changing signedness.
    const U int_signed_max = static_cast<U>(std::numeric_limits<int>::max());
    const U int_unsigned_max =
        static_cast<U>(std::numeric_limits<unsigned int>::max());
    const U long_signed_max = static_cast<U>(std::numeric_limits<long>::max());
    const U long_unsigned_max =
        static_cast<U>(std::numeric_limits<unsigned long>::max());
    const U signed_max = static_cast<U>(std::numeric_limits<S>::max());
    const U unsigned_max = static_cast<U>(std::numeric_limits<U>::max());
    bool uns = false;
    if (u) {
      if (long_count == 2) {
        if (value > unsigned_max)
          throw std::runtime_error("integer out of range");
      } else if (long_count == 1) {
        if (value > long_unsigned_max)
          throw std::runtime_error("integer out of range");
      } else if (value > int_unsigned_max && value > long_unsigned_max) {
        throw std::runtime_error("integer out of range");
      }
      uns = true;
    } else if (base == 10) {
      // Unsuffixed decimal literals only consider signed types, including when
      // an l/ll suffix is present. Unlike non-decimal literals, there is no
      // unsigned candidate after a signed candidate fails.
      if (value > signed_max)
        throw std::runtime_error("decimal literal out of range");
    } else if (long_count == 2) {
      if (value > unsigned_max)
        throw std::runtime_error("integer out of range");
      uns = value > signed_max;
    } else if (long_count == 1) {
      if (value <= long_signed_max)
        uns = false;
      else if (value <= long_unsigned_max)
        uns = true;
      else if (value <= signed_max)
        uns = false;
      else if (value <= unsigned_max)
        uns = true;
      else
        throw std::runtime_error("integer out of range");
    } else if (value <= int_signed_max) {
      uns = false;
    } else if (value <= int_unsigned_max) {
      uns = true;
    } else if (value <= long_signed_max) {
      uns = false;
    } else if (value <= long_unsigned_max) {
      uns = true;
    } else {
      throw std::runtime_error("integer out of range");
    }
    return PA3Value(value, uns);
  }

  static U utf8_codepoint(const std::string &s, size_t &i) {
    unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80)
      return c;
    unsigned n = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    U cp = c & (n == 1 ? 0x1f : n == 2 ? 0x0f : 0x07);
    if (i + n > s.size())
      throw std::runtime_error("bad utf8");
    while (n--)
      cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3f);
    return cp;
  }
  static PA3Value character(const std::string &s) {
    size_t q = s.find('\'');
    if (q == std::string::npos || s.size() < q + 3 || s[s.size() - 1] != '\'')
      throw std::runtime_error("bad character");
    const std::string prefix = s.substr(0, q);
    if (!prefix.empty() && prefix != "u" && prefix != "U" && prefix != "L")
      throw std::runtime_error("invalid character encoding prefix");
    size_t i = q + 1, stop = s.size() - 1;
    U value = 0;
    unsigned count = 0;
    while (i < stop) {
      U cp;
      if (s[i] != '\\')
        cp = utf8_codepoint(s, i);
      else {
        ++i;
        if (i >= stop)
          throw std::runtime_error("bad escape");
        char e = s[i++];
        switch (e) {
        case '\\':
          cp = '\\';
          break;
        case '\'':
          cp = '\'';
          break;
        case '"':
          cp = '"';
          break;
        case '?':
          cp = '?';
          break;
        case 'a':
          cp = 7;
          break;
        case 'b':
          cp = 8;
          break;
        case 'f':
          cp = 12;
          break;
        case 'n':
          cp = 10;
          break;
        case 'r':
          cp = 13;
          break;
        case 't':
          cp = 9;
          break;
        case 'v':
          cp = 11;
          break;
        case 'x': {
          cp = 0;
          unsigned d = 0;
          while (i < stop && digit_value(s[i]) >= 0) {
            const unsigned digit = static_cast<unsigned>(digit_value(s[i]));
            if (cp > (0xffffffffULL - digit) / 16)
              throw std::runtime_error("hex escape overflow");
            cp = cp * 16 + digit;
            ++i;
            ++d;
          }
          if (!d)
            throw std::runtime_error("bad hex escape");
          break;
        }
        case 'u':
        case 'U': {
          unsigned n = e == 'u' ? 4 : 8;
          cp = 0;
          if (i + n > stop)
            throw std::runtime_error("bad ucn");
          while (n--) {
            int d = digit_value(s[i++]);
            if (d < 0)
              throw std::runtime_error("bad ucn");
            cp = cp * 16 + d;
          }
          break;
        }
        default:
          if (e >= '0' && e <= '7') {
            cp = e - '0';
            unsigned n = 1;
            while (n < 3 && i < stop && s[i] >= '0' && s[i] <= '7') {
              cp = cp * 8 + s[i++] - '0';
              ++n;
            }
          } else
            throw std::runtime_error("bad escape");
        }
      }
      // PA2 defines character literals as exactly one Unicode scalar value,
      // and PA3 reuses that decoded literal semantics. Apply the range check
      // after decoding escapes as well as direct source characters.
      if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        throw std::runtime_error("character literal code point out of range");
      if (count == 0)
        value = cp;
      ++count;
    }
    // Do not silently implement an arbitrary multicharacter-literal value.
    if (count != 1)
      throw std::runtime_error("character literal must contain one code point");
    // A UTF-16 character literal must denote one code unit; unlike an
    // unprefixed/wide/UTF-32 literal it cannot represent supplementary code
    // points.
    if (q && s[0] == 'u' && value > 0xffff)
      throw std::runtime_error("UTF-16 character out of range");
    if (q && s[0] == 'u')
      return PA3Value(value, true);
    if (q && s[0] == 'U')
      return PA3Value(value, true);
    return PA3Value(value, false);
  }

  PA3Value unary(bool eval) {
    if (take("+")) {
      PA3Value a = unary(eval);
      return a;
    }
    if (take("-")) {
      PA3Value a = unary(eval);
      return PA3Value(0 - a.bits, a.is_unsigned);
    }
    if (take("!") || take("not")) {
      PA3Value a = unary(eval);
      return boolean(a.bits == 0);
    }
    if (take("~") || take("compl")) {
      PA3Value a = unary(eval);
      return PA3Value(~a.bits, a.is_unsigned);
    }
    return primary(eval);
  }
  PA3Value expression(int min_precedence, bool eval);
  PA3Value conditional(bool eval) { return expression(0, eval); }
};

// Precedence values follow the PA3 controlling-expression grammar.
static int precedence(const std::string &op) {
  if (op == "*" || op == "/" || op == "%")
    return 10;
  if (op == "+" || op == "-")
    return 9;
  if (op == "<<" || op == ">>")
    return 8;
  if (op == "<" || op == ">" || op == "<=" || op == ">=")
    return 7;
  if (op == "==" || op == "!=" || op == "not_eq")
    return 6;
  if (op == "&" || op == "bitand")
    return 5;
  if (op == "^" || op == "xor")
    return 4;
  if (op == "|" || op == "bitor")
    return 3;
  if (op == "&&" || op == "and")
    return 2;
  if (op == "||" || op == "or")
    return 1;
  return -1;
}
PA3Value Parser::expression(int min_precedence, bool eval) {
  PA3Value left = unary(eval);
  for (;;) {
    if (min_precedence == 0 && at("?")) {
      ++p;
      const bool choose_yes = left.bits != 0;
      PA3Value yes = expression(0, eval && choose_yes);
      if (!take(":"))
        throw std::runtime_error("missing colon");
      PA3Value no = expression(0, eval && !choose_yes);
      const bool common_unsigned = yes.is_unsigned || no.is_unsigned;
      left = choose_yes ? yes : no;
      left.is_unsigned = common_unsigned;
      continue;
    }
    if (p >= ts.size())
      break;
    const std::string op = ts[p].spelling;
    const int prec = precedence(op);
    if (prec < min_precedence)
      break;
    ++p;
    bool rhs_eval = eval;
    if ((op == "&&" || op == "and") && left.bits == 0)
      rhs_eval = false;
    if ((op == "||" || op == "or") && left.bits != 0)
      rhs_eval = false;
    PA3Value right = expression(prec + 1, rhs_eval);
    const bool uns = left.is_unsigned || right.is_unsigned;
    if (!eval) {
      const bool is_boolean = op == "<" || op == ">" || op == "<=" ||
                              op == ">=" || op == "==" || op == "!=" ||
                              op == "not_eq" || op == "&&" || op == "and" ||
                              op == "||" || op == "or";
      if (is_boolean)
        left = PA3Value(0, false);
      else if (op == "<<" || op == ">>")
        left = PA3Value(0, left.is_unsigned);
      else
        left = PA3Value(0, uns);
      continue;
    }
    const U x = left.bits, y = right.bits;
    U z = 0;
    bool cmp = false;
    if (op == "*")
      z = x * y;
    else if (op == "/") {
      if (!y)
        throw std::runtime_error("divide by zero");
      if (!uns && as_signed(left) == std::numeric_limits<S>::min() &&
          as_signed(right) == -1)
        throw std::runtime_error("signed divide overflow");
      z = uns ? x / y : static_cast<U>(as_signed(left) / as_signed(right));
    } else if (op == "%") {
      if (!y)
        throw std::runtime_error("mod by zero");
      if (!uns && as_signed(left) == std::numeric_limits<S>::min() &&
          as_signed(right) == -1)
        throw std::runtime_error("signed remainder overflow");
      z = uns ? x % y : static_cast<U>(as_signed(left) % as_signed(right));
    } else if (op == "+")
      z = x + y;
    else if (op == "-")
      z = x - y;
    else if (op == "<<" || op == ">>") {
      const S shift =
          right.is_unsigned
              ? (right.bits > static_cast<U>(std::numeric_limits<S>::max())
                     ? 64
                     : static_cast<S>(right.bits))
              : as_signed(right);
      if (shift < 0 || shift >= 64)
        throw std::runtime_error("bad shift");
      // Use unsigned bit operations for the course-defined fixed-width
      // controlling-expression arithmetic, then preserve the signed result's
      // two's-complement representation when formatting or comparing it.
      z = op == "<<"
              ? (x << shift)
              : (left.is_unsigned ? (x >> shift)
                                  : arithmetic_shift_right(x, shift));
      left = PA3Value(z, left.is_unsigned);
      continue;
    } else if (op == "<")
      cmp = uns ? x < y : as_signed(left) < as_signed(right);
    else if (op == ">")
      cmp = uns ? x > y : as_signed(left) > as_signed(right);
    else if (op == "<=")
      cmp = uns ? x <= y : as_signed(left) <= as_signed(right);
    else if (op == ">=")
      cmp = uns ? x >= y : as_signed(left) >= as_signed(right);
    else if (op == "==")
      cmp = x == y;
    else if (op == "!=" || op == "not_eq")
      cmp = x != y;
    else if (op == "&" || op == "bitand")
      z = x & y;
    else if (op == "^" || op == "xor")
      z = x ^ y;
    else if (op == "|" || op == "bitor")
      z = x | y;
    else if (op == "&&" || op == "and")
      cmp = x != 0 && y != 0;
    else if (op == "||" || op == "or")
      cmp = x != 0 || y != 0;
    if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" ||
        op == "!=" || op == "not_eq" || op == "&&" || op == "and" ||
        op == "||" || op == "or")
      left = boolean(cmp);
    else
      left = PA3Value(z, uns);
  }
  return left;
}

} // namespace

PA3Value EvaluatePA3Expression(const std::vector<PA3Token> &tokens) {
  Parser p(tokens);
  PA3Value v = p.conditional(true);
  if (p.p != tokens.size())
    throw std::runtime_error("trailing tokens");
  return v;
}
std::string FormatPA3Value(const PA3Value &v) {
  if (v.is_unsigned)
    return std::to_string(v.bits) + "u";
  S n = static_cast<S>(v.bits);
  return std::to_string(n);
}
