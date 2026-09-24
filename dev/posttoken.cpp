// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

using namespace std;

#include "preprocess/pp_tokenizer.h"
#include "preprocess/tokens/IPPTokenStream.h"
#include "support/not_implemented.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <limits>
#include <regex>
#include <stdint.h>
#include <vector>

// See 3.9.1: Fundamental Types
enum EFundamentalType {
  // 3.9.1.2
  FT_SIGNED_CHAR,
  FT_SHORT_INT,
  FT_INT,
  FT_LONG_INT,
  FT_LONG_LONG_INT,

  // 3.9.1.3
  FT_UNSIGNED_CHAR,
  FT_UNSIGNED_SHORT_INT,
  FT_UNSIGNED_INT,
  FT_UNSIGNED_LONG_INT,
  FT_UNSIGNED_LONG_LONG_INT,

  // 3.9.1.1 / 3.9.1.5
  FT_WCHAR_T,
  FT_CHAR,
  FT_CHAR16_T,
  FT_CHAR32_T,

  // 3.9.1.6
  FT_BOOL,

  // 3.9.1.8
  FT_FLOAT,
  FT_DOUBLE,
  FT_LONG_DOUBLE,

  // 3.9.1.9
  FT_VOID,

  // 3.9.1.10
  FT_NULLPTR_T
};

// FundamentalTypeOf: convert fundamental type T to EFundamentalType
// for example: `FundamentalTypeOf<long int>()` will return `FT_LONG_INT`
template <typename T> constexpr EFundamentalType FundamentalTypeOf();
template <> constexpr EFundamentalType FundamentalTypeOf<signed char>() {
  return FT_SIGNED_CHAR;
}
template <> constexpr EFundamentalType FundamentalTypeOf<short int>() {
  return FT_SHORT_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<int>() {
  return FT_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<long int>() {
  return FT_LONG_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<long long int>() {
  return FT_LONG_LONG_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<unsigned char>() {
  return FT_UNSIGNED_CHAR;
}
template <> constexpr EFundamentalType FundamentalTypeOf<unsigned short int>() {
  return FT_UNSIGNED_SHORT_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<unsigned int>() {
  return FT_UNSIGNED_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<unsigned long int>() {
  return FT_UNSIGNED_LONG_INT;
}
template <>
constexpr EFundamentalType FundamentalTypeOf<unsigned long long int>() {
  return FT_UNSIGNED_LONG_LONG_INT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<wchar_t>() {
  return FT_WCHAR_T;
}
template <> constexpr EFundamentalType FundamentalTypeOf<char>() {
  return FT_CHAR;
}
template <> constexpr EFundamentalType FundamentalTypeOf<char16_t>() {
  return FT_CHAR16_T;
}
template <> constexpr EFundamentalType FundamentalTypeOf<char32_t>() {
  return FT_CHAR32_T;
}
template <> constexpr EFundamentalType FundamentalTypeOf<bool>() {
  return FT_BOOL;
}
template <> constexpr EFundamentalType FundamentalTypeOf<float>() {
  return FT_FLOAT;
}
template <> constexpr EFundamentalType FundamentalTypeOf<double>() {
  return FT_DOUBLE;
}
template <> constexpr EFundamentalType FundamentalTypeOf<long double>() {
  return FT_LONG_DOUBLE;
}
template <> constexpr EFundamentalType FundamentalTypeOf<void>() {
  return FT_VOID;
}
template <> constexpr EFundamentalType FundamentalTypeOf<nullptr_t>() {
  return FT_NULLPTR_T;
}

// convert EFundamentalType to a source code
const map<EFundamentalType, string> FundamentalTypeToStringMap{
    {FT_SIGNED_CHAR, "signed char"},
    {FT_SHORT_INT, "short int"},
    {FT_INT, "int"},
    {FT_LONG_INT, "long int"},
    {FT_LONG_LONG_INT, "long long int"},
    {FT_UNSIGNED_CHAR, "unsigned char"},
    {FT_UNSIGNED_SHORT_INT, "unsigned short int"},
    {FT_UNSIGNED_INT, "unsigned int"},
    {FT_UNSIGNED_LONG_INT, "unsigned long int"},
    {FT_UNSIGNED_LONG_LONG_INT, "unsigned long long int"},
    {FT_WCHAR_T, "wchar_t"},
    {FT_CHAR, "char"},
    {FT_CHAR16_T, "char16_t"},
    {FT_CHAR32_T, "char32_t"},
    {FT_BOOL, "bool"},
    {FT_FLOAT, "float"},
    {FT_DOUBLE, "double"},
    {FT_LONG_DOUBLE, "long double"},
    {FT_VOID, "void"},
    {FT_NULLPTR_T, "nullptr_t"}};

// token type enum for `simples`
enum ETokenType {
  // keywords
  KW_ALIGNAS,
  KW_ALIGNOF,
  KW_ASM,
  KW_AUTO,
  KW_BOOL,
  KW_BREAK,
  KW_CASE,
  KW_CATCH,
  KW_CHAR,
  KW_CHAR16_T,
  KW_CHAR32_T,
  KW_CLASS,
  KW_CONST,
  KW_CONSTEXPR,
  KW_CONST_CAST,
  KW_CONTINUE,
  KW_DECLTYPE,
  KW_DEFAULT,
  KW_DELETE,
  KW_DO,
  KW_DOUBLE,
  KW_DYNAMIC_CAST,
  KW_ELSE,
  KW_ENUM,
  KW_EXPLICIT,
  KW_EXPORT,
  KW_EXTERN,
  KW_FALSE,
  KW_FLOAT,
  KW_FOR,
  KW_FRIEND,
  KW_GOTO,
  KW_IF,
  KW_INLINE,
  KW_INT,
  KW_LONG,
  KW_MUTABLE,
  KW_NAMESPACE,
  KW_NEW,
  KW_NOEXCEPT,
  KW_NULLPTR,
  KW_OPERATOR,
  KW_PRIVATE,
  KW_PROTECTED,
  KW_PUBLIC,
  KW_REGISTER,
  KW_REINTERPET_CAST,
  KW_RETURN,
  KW_SHORT,
  KW_SIGNED,
  KW_SIZEOF,
  KW_STATIC,
  KW_STATIC_ASSERT,
  KW_STATIC_CAST,
  KW_STRUCT,
  KW_SWITCH,
  KW_TEMPLATE,
  KW_THIS,
  KW_THREAD_LOCAL,
  KW_THROW,
  KW_TRUE,
  KW_TRY,
  KW_TYPEDEF,
  KW_TYPEID,
  KW_TYPENAME,
  KW_UNION,
  KW_UNSIGNED,
  KW_USING,
  KW_VIRTUAL,
  KW_VOID,
  KW_VOLATILE,
  KW_WCHAR_T,
  KW_WHILE,

  // operators/punctuation
  OP_LBRACE,
  OP_RBRACE,
  OP_LSQUARE,
  OP_RSQUARE,
  OP_LPAREN,
  OP_RPAREN,
  OP_BOR,
  OP_XOR,
  OP_COMPL,
  OP_AMP,
  OP_LNOT,
  OP_SEMICOLON,
  OP_COLON,
  OP_DOTS,
  OP_QMARK,
  OP_COLON2,
  OP_DOT,
  OP_DOTSTAR,
  OP_PLUS,
  OP_MINUS,
  OP_STAR,
  OP_DIV,
  OP_MOD,
  OP_ASS,
  OP_LT,
  OP_GT,
  OP_PLUSASS,
  OP_MINUSASS,
  OP_STARASS,
  OP_DIVASS,
  OP_MODASS,
  OP_XORASS,
  OP_BANDASS,
  OP_BORASS,
  OP_LSHIFT,
  OP_RSHIFT,
  OP_RSHIFTASS,
  OP_LSHIFTASS,
  OP_EQ,
  OP_NE,
  OP_LE,
  OP_GE,
  OP_LAND,
  OP_LOR,
  OP_INC,
  OP_DEC,
  OP_COMMA,
  OP_ARROWSTAR,
  OP_ARROW,
};

// StringToETokenTypeMap map of `simple` `preprocessing-tokens` to ETokenType
const unordered_map<string, ETokenType> StringToTokenTypeMap = {
    // keywords
    {"alignas", KW_ALIGNAS},
    {"alignof", KW_ALIGNOF},
    {"asm", KW_ASM},
    {"auto", KW_AUTO},
    {"bool", KW_BOOL},
    {"break", KW_BREAK},
    {"case", KW_CASE},
    {"catch", KW_CATCH},
    {"char", KW_CHAR},
    {"char16_t", KW_CHAR16_T},
    {"char32_t", KW_CHAR32_T},
    {"class", KW_CLASS},
    {"const", KW_CONST},
    {"constexpr", KW_CONSTEXPR},
    {"const_cast", KW_CONST_CAST},
    {"continue", KW_CONTINUE},
    {"decltype", KW_DECLTYPE},
    {"default", KW_DEFAULT},
    {"delete", KW_DELETE},
    {"do", KW_DO},
    {"double", KW_DOUBLE},
    {"dynamic_cast", KW_DYNAMIC_CAST},
    {"else", KW_ELSE},
    {"enum", KW_ENUM},
    {"explicit", KW_EXPLICIT},
    {"export", KW_EXPORT},
    {"extern", KW_EXTERN},
    {"false", KW_FALSE},
    {"float", KW_FLOAT},
    {"for", KW_FOR},
    {"friend", KW_FRIEND},
    {"goto", KW_GOTO},
    {"if", KW_IF},
    {"inline", KW_INLINE},
    {"int", KW_INT},
    {"long", KW_LONG},
    {"mutable", KW_MUTABLE},
    {"namespace", KW_NAMESPACE},
    {"new", KW_NEW},
    {"noexcept", KW_NOEXCEPT},
    {"nullptr", KW_NULLPTR},
    {"operator", KW_OPERATOR},
    {"private", KW_PRIVATE},
    {"protected", KW_PROTECTED},
    {"public", KW_PUBLIC},
    {"register", KW_REGISTER},
    {"reinterpret_cast", KW_REINTERPET_CAST},
    {"return", KW_RETURN},
    {"short", KW_SHORT},
    {"signed", KW_SIGNED},
    {"sizeof", KW_SIZEOF},
    {"static", KW_STATIC},
    {"static_assert", KW_STATIC_ASSERT},
    {"static_cast", KW_STATIC_CAST},
    {"struct", KW_STRUCT},
    {"switch", KW_SWITCH},
    {"template", KW_TEMPLATE},
    {"this", KW_THIS},
    {"thread_local", KW_THREAD_LOCAL},
    {"throw", KW_THROW},
    {"true", KW_TRUE},
    {"try", KW_TRY},
    {"typedef", KW_TYPEDEF},
    {"typeid", KW_TYPEID},
    {"typename", KW_TYPENAME},
    {"union", KW_UNION},
    {"unsigned", KW_UNSIGNED},
    {"using", KW_USING},
    {"virtual", KW_VIRTUAL},
    {"void", KW_VOID},
    {"volatile", KW_VOLATILE},
    {"wchar_t", KW_WCHAR_T},
    {"while", KW_WHILE},

    // operators/punctuation
    {"{", OP_LBRACE},
    {"<%", OP_LBRACE},
    {"}", OP_RBRACE},
    {"%>", OP_RBRACE},
    {"[", OP_LSQUARE},
    {"<:", OP_LSQUARE},
    {"]", OP_RSQUARE},
    {":>", OP_RSQUARE},
    {"(", OP_LPAREN},
    {")", OP_RPAREN},
    {"|", OP_BOR},
    {"bitor", OP_BOR},
    {"^", OP_XOR},
    {"xor", OP_XOR},
    {"~", OP_COMPL},
    {"compl", OP_COMPL},
    {"&", OP_AMP},
    {"bitand", OP_AMP},
    {"!", OP_LNOT},
    {"not", OP_LNOT},
    {";", OP_SEMICOLON},
    {":", OP_COLON},
    {"...", OP_DOTS},
    {"?", OP_QMARK},
    {"::", OP_COLON2},
    {".", OP_DOT},
    {".*", OP_DOTSTAR},
    {"+", OP_PLUS},
    {"-", OP_MINUS},
    {"*", OP_STAR},
    {"/", OP_DIV},
    {"%", OP_MOD},
    {"=", OP_ASS},
    {"<", OP_LT},
    {">", OP_GT},
    {"+=", OP_PLUSASS},
    {"-=", OP_MINUSASS},
    {"*=", OP_STARASS},
    {"/=", OP_DIVASS},
    {"%=", OP_MODASS},
    {"^=", OP_XORASS},
    {"xor_eq", OP_XORASS},
    {"&=", OP_BANDASS},
    {"and_eq", OP_BANDASS},
    {"|=", OP_BORASS},
    {"or_eq", OP_BORASS},
    {"<<", OP_LSHIFT},
    {">>", OP_RSHIFT},
    {">>=", OP_RSHIFTASS},
    {"<<=", OP_LSHIFTASS},
    {"==", OP_EQ},
    {"!=", OP_NE},
    {"not_eq", OP_NE},
    {"<=", OP_LE},
    {">=", OP_GE},
    {"&&", OP_LAND},
    {"and", OP_LAND},
    {"||", OP_LOR},
    {"or", OP_LOR},
    {"++", OP_INC},
    {"--", OP_DEC},
    {",", OP_COMMA},
    {"->*", OP_ARROWSTAR},
    {"->", OP_ARROW}};

// map of enum to string
const map<ETokenType, string> TokenTypeToStringMap = {
    {KW_ALIGNAS, "KW_ALIGNAS"},
    {KW_ALIGNOF, "KW_ALIGNOF"},
    {KW_ASM, "KW_ASM"},
    {KW_AUTO, "KW_AUTO"},
    {KW_BOOL, "KW_BOOL"},
    {KW_BREAK, "KW_BREAK"},
    {KW_CASE, "KW_CASE"},
    {KW_CATCH, "KW_CATCH"},
    {KW_CHAR, "KW_CHAR"},
    {KW_CHAR16_T, "KW_CHAR16_T"},
    {KW_CHAR32_T, "KW_CHAR32_T"},
    {KW_CLASS, "KW_CLASS"},
    {KW_CONST, "KW_CONST"},
    {KW_CONSTEXPR, "KW_CONSTEXPR"},
    {KW_CONST_CAST, "KW_CONST_CAST"},
    {KW_CONTINUE, "KW_CONTINUE"},
    {KW_DECLTYPE, "KW_DECLTYPE"},
    {KW_DEFAULT, "KW_DEFAULT"},
    {KW_DELETE, "KW_DELETE"},
    {KW_DO, "KW_DO"},
    {KW_DOUBLE, "KW_DOUBLE"},
    {KW_DYNAMIC_CAST, "KW_DYNAMIC_CAST"},
    {KW_ELSE, "KW_ELSE"},
    {KW_ENUM, "KW_ENUM"},
    {KW_EXPLICIT, "KW_EXPLICIT"},
    {KW_EXPORT, "KW_EXPORT"},
    {KW_EXTERN, "KW_EXTERN"},
    {KW_FALSE, "KW_FALSE"},
    {KW_FLOAT, "KW_FLOAT"},
    {KW_FOR, "KW_FOR"},
    {KW_FRIEND, "KW_FRIEND"},
    {KW_GOTO, "KW_GOTO"},
    {KW_IF, "KW_IF"},
    {KW_INLINE, "KW_INLINE"},
    {KW_INT, "KW_INT"},
    {KW_LONG, "KW_LONG"},
    {KW_MUTABLE, "KW_MUTABLE"},
    {KW_NAMESPACE, "KW_NAMESPACE"},
    {KW_NEW, "KW_NEW"},
    {KW_NOEXCEPT, "KW_NOEXCEPT"},
    {KW_NULLPTR, "KW_NULLPTR"},
    {KW_OPERATOR, "KW_OPERATOR"},
    {KW_PRIVATE, "KW_PRIVATE"},
    {KW_PROTECTED, "KW_PROTECTED"},
    {KW_PUBLIC, "KW_PUBLIC"},
    {KW_REGISTER, "KW_REGISTER"},
    {KW_REINTERPET_CAST, "KW_REINTERPET_CAST"},
    {KW_RETURN, "KW_RETURN"},
    {KW_SHORT, "KW_SHORT"},
    {KW_SIGNED, "KW_SIGNED"},
    {KW_SIZEOF, "KW_SIZEOF"},
    {KW_STATIC, "KW_STATIC"},
    {KW_STATIC_ASSERT, "KW_STATIC_ASSERT"},
    {KW_STATIC_CAST, "KW_STATIC_CAST"},
    {KW_STRUCT, "KW_STRUCT"},
    {KW_SWITCH, "KW_SWITCH"},
    {KW_TEMPLATE, "KW_TEMPLATE"},
    {KW_THIS, "KW_THIS"},
    {KW_THREAD_LOCAL, "KW_THREAD_LOCAL"},
    {KW_THROW, "KW_THROW"},
    {KW_TRUE, "KW_TRUE"},
    {KW_TRY, "KW_TRY"},
    {KW_TYPEDEF, "KW_TYPEDEF"},
    {KW_TYPEID, "KW_TYPEID"},
    {KW_TYPENAME, "KW_TYPENAME"},
    {KW_UNION, "KW_UNION"},
    {KW_UNSIGNED, "KW_UNSIGNED"},
    {KW_USING, "KW_USING"},
    {KW_VIRTUAL, "KW_VIRTUAL"},
    {KW_VOID, "KW_VOID"},
    {KW_VOLATILE, "KW_VOLATILE"},
    {KW_WCHAR_T, "KW_WCHAR_T"},
    {KW_WHILE, "KW_WHILE"},
    {OP_LBRACE, "OP_LBRACE"},
    {OP_RBRACE, "OP_RBRACE"},
    {OP_LSQUARE, "OP_LSQUARE"},
    {OP_RSQUARE, "OP_RSQUARE"},
    {OP_LPAREN, "OP_LPAREN"},
    {OP_RPAREN, "OP_RPAREN"},
    {OP_BOR, "OP_BOR"},
    {OP_XOR, "OP_XOR"},
    {OP_COMPL, "OP_COMPL"},
    {OP_AMP, "OP_AMP"},
    {OP_LNOT, "OP_LNOT"},
    {OP_SEMICOLON, "OP_SEMICOLON"},
    {OP_COLON, "OP_COLON"},
    {OP_DOTS, "OP_DOTS"},
    {OP_QMARK, "OP_QMARK"},
    {OP_COLON2, "OP_COLON2"},
    {OP_DOT, "OP_DOT"},
    {OP_DOTSTAR, "OP_DOTSTAR"},
    {OP_PLUS, "OP_PLUS"},
    {OP_MINUS, "OP_MINUS"},
    {OP_STAR, "OP_STAR"},
    {OP_DIV, "OP_DIV"},
    {OP_MOD, "OP_MOD"},
    {OP_ASS, "OP_ASS"},
    {OP_LT, "OP_LT"},
    {OP_GT, "OP_GT"},
    {OP_PLUSASS, "OP_PLUSASS"},
    {OP_MINUSASS, "OP_MINUSASS"},
    {OP_STARASS, "OP_STARASS"},
    {OP_DIVASS, "OP_DIVASS"},
    {OP_MODASS, "OP_MODASS"},
    {OP_XORASS, "OP_XORASS"},
    {OP_BANDASS, "OP_BANDASS"},
    {OP_BORASS, "OP_BORASS"},
    {OP_LSHIFT, "OP_LSHIFT"},
    {OP_RSHIFT, "OP_RSHIFT"},
    {OP_RSHIFTASS, "OP_RSHIFTASS"},
    {OP_LSHIFTASS, "OP_LSHIFTASS"},
    {OP_EQ, "OP_EQ"},
    {OP_NE, "OP_NE"},
    {OP_LE, "OP_LE"},
    {OP_GE, "OP_GE"},
    {OP_LAND, "OP_LAND"},
    {OP_LOR, "OP_LOR"},
    {OP_INC, "OP_INC"},
    {OP_DEC, "OP_DEC"},
    {OP_COMMA, "OP_COMMA"},
    {OP_ARROWSTAR, "OP_ARROWSTAR"},
    {OP_ARROW, "OP_ARROW"}};

// convert integer [0,15] to hexadecimal digit
char ValueToHexChar(int c) {
  switch (c) {
  case 0:
    return '0';
  case 1:
    return '1';
  case 2:
    return '2';
  case 3:
    return '3';
  case 4:
    return '4';
  case 5:
    return '5';
  case 6:
    return '6';
  case 7:
    return '7';
  case 8:
    return '8';
  case 9:
    return '9';
  case 10:
    return 'A';
  case 11:
    return 'B';
  case 12:
    return 'C';
  case 13:
    return 'D';
  case 14:
    return 'E';
  case 15:
    return 'F';
  default:
    throw logic_error("ValueToHexChar of nonhex value");
  }
}

// hex dump memory range
string HexDump(const void *pdata, size_t nbytes) {
  unsigned char *p = (unsigned char *)pdata;

  string s(nbytes * 2, '?');

  for (size_t i = 0; i < nbytes; i++) {
    s[2 * i + 0] = ValueToHexChar((p[i] & 0xF0) >> 4);
    s[2 * i + 1] = ValueToHexChar((p[i] & 0x0F) >> 0);
  }

  return s;
}

// DebugPostTokenOutputStream: helper class to produce PA2 output format
struct DebugPostTokenOutputStream {
  // output: invalid <source>
  void emit_invalid(const string &source) {
    cout << "invalid " << source << endl;
  }

  // output: simple <source> <token_type>
  void emit_simple(const string &source, ETokenType token_type) {
    cout << "simple " << source << " " << TokenTypeToStringMap.at(token_type)
         << endl;
  }

  // output: identifier <source>
  void emit_identifier(const string &source) {
    cout << "identifier " << source << endl;
  }

  // output: literal <source> <type> <hexdump(data,nbytes)>
  void emit_literal(const string &source, EFundamentalType type,
                    const void *data, size_t nbytes) {
    cout << "literal " << source << " " << FundamentalTypeToStringMap.at(type)
         << " " << HexDump(data, nbytes) << endl;
  }

  // output: literal <source> array of <num_elements> <type>
  // <hexdump(data,nbytes)>
  void emit_literal_array(const string &source, size_t num_elements,
                          EFundamentalType type, const void *data,
                          size_t nbytes) {
    cout << "literal " << source << " array of " << num_elements << " "
         << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes)
         << endl;
  }

  // output: user-defined-literal <source> <ud_suffix> character <type>
  // <hexdump(data,nbytes)>
  void emit_user_defined_literal_character(const string &source,
                                           const string &ud_suffix,
                                           EFundamentalType type,
                                           const void *data, size_t nbytes) {
    cout << "user-defined-literal " << source << " " << ud_suffix
         << " character " << FundamentalTypeToStringMap.at(type) << " "
         << HexDump(data, nbytes) << endl;
  }

  // output: user-defined-literal <source> <ud_suffix> string array of
  // <num_elements> <type> <hexdump(data, nbytes)>
  void emit_user_defined_literal_string_array(const string &source,
                                              const string &ud_suffix,
                                              size_t num_elements,
                                              EFundamentalType type,
                                              const void *data, size_t nbytes) {
    cout << "user-defined-literal " << source << " " << ud_suffix
         << " string array of " << num_elements << " "
         << FundamentalTypeToStringMap.at(type) << " " << HexDump(data, nbytes)
         << endl;
  }

  // output: user-defined-literal <source> <ud_suffix> <prefix>
  void emit_user_defined_literal_integer(const string &source,
                                         const string &ud_suffix,
                                         const string &prefix) {
    cout << "user-defined-literal " << source << " " << ud_suffix << " integer "
         << prefix << endl;
  }

  // output: user-defined-literal <source> <ud_suffix> <prefix>
  void emit_user_defined_literal_floating(const string &source,
                                          const string &ud_suffix,
                                          const string &prefix) {
    cout << "user-defined-literal " << source << " " << ud_suffix
         << " floating " << prefix << endl;
  }

  // output : eof
  void emit_eof() { cout << "eof" << endl; }
};

// use these 3 functions to scan `floating-literals` (see PA2)
// for example PA2Decode_float("12.34") returns "12.34" as a `float` type
float PA2Decode_float(const string &s) {
  istringstream iss(s);
  float x;
  iss >> x;
  return x;
}

double PA2Decode_double(const string &s) {
  istringstream iss(s);
  double x;
  iss >> x;
  return x;
}

long double PA2Decode_long_double(const string &s) {
  istringstream iss(s);
  long double x;
  iss >> x;
  return x;
}

namespace {
struct LiteralUnit {
  uint32_t value;
  bool numeric_escape;
};
struct PostTokenProcessor : IPPTokenStream {
  explicit PostTokenProcessor(DebugPostTokenOutputStream &output)
      : output(output), string_run_open(false), string_run_invalid(false) {}

  void emit_whitespace_sequence() {}
  void emit_new_line() {}
  void emit_header_name(const string &s) {
    process_nonstring("invalid", s);
  }
  void emit_identifier(const string &s) {
    process_nonstring("identifier", s);
  }
  void emit_pp_number(const string &s) {
    process_nonstring("number", s);
  }
  void emit_character_literal(const string &s) {
    process_nonstring("character", s);
  }
  void emit_user_defined_character_literal(const string &s) {
    process_nonstring("ud-character", s);
  }
  void emit_string_literal(const string &s) { process_string(s); }
  void emit_user_defined_string_literal(const string &s) { process_string(s); }
  void emit_preprocessing_op_or_punc(const string &s) {
    process_nonstring("operator", s);
  }
  void emit_non_whitespace_char(const string &s) {
    process_nonstring("invalid", s);
  }
  void emit_eof() {
    flush_strings();
    output.emit_eof();
  }

private:
  DebugPostTokenOutputStream &output;
  bool string_run_open;
  bool string_run_invalid;
  string string_source, string_suffix, string_encoding;
  vector<LiteralUnit> string_values;

  void process_string(const string &source);
  void flush_strings();
  void process_nonstring(const string &kind, const string &source);
};
int digit_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
bool ident_tail(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
}
vector<uint32_t> decode_utf8_local(const string &s) {
  vector<uint32_t> r;
  for (size_t i = 0; i < s.size();) {
    unsigned char c = s[i++];
    uint32_t v;
    int n;
    if (c < 0x80) {
      v = c;
      n = 0;
    } else if ((c & 0xe0) == 0xc0) {
      v = c & 31;
      n = 1;
      if (v < 2)
        throw runtime_error("invalid UTF-8");
    } else if ((c & 0xf0) == 0xe0) {
      v = c & 15;
      n = 2;
    } else if ((c & 0xf8) == 0xf0) {
      v = c & 7;
      n = 3;
      if (v > 4)
        throw runtime_error("invalid UTF-8");
    } else
      throw runtime_error("invalid UTF-8");
    for (int j = 0; j < n; j++) {
      if (i >= s.size() || ((unsigned char)s[i] & 0xc0) != 0x80)
        throw runtime_error("invalid UTF-8");
      v = (v << 6) | ((unsigned char)s[i++] & 63);
    }
    if ((n == 2 && v < 0x800) || (n == 3 && v < 0x10000) || v > 0x10ffff ||
        (v >= 0xd800 && v <= 0xdfff))
      throw runtime_error("invalid UTF-8");
    r.push_back(v);
  }
  return r;
}
void append_utf8_local(string &s, uint32_t c) {
  if (c <= 0x7f)
    s.push_back(char(c));
  else if (c <= 0x7ff) {
    s.push_back(char(0xc0 | (c >> 6)));
    s.push_back(char(0x80 | (c & 63)));
  } else if (c <= 0xffff) {
    s.push_back(char(0xe0 | (c >> 12)));
    s.push_back(char(0x80 | ((c >> 6) & 63)));
    s.push_back(char(0x80 | (c & 63)));
  } else {
    s.push_back(char(0xf0 | (c >> 18)));
    s.push_back(char(0x80 | ((c >> 12) & 63)));
    s.push_back(char(0x80 | ((c >> 6) & 63)));
    s.push_back(char(0x80 | (c & 63)));
  }
}
bool valid_cp(uint32_t c) {
  return c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff);
}
bool scan_digits(const string &s, size_t &p, int base, bool require = true) {
  size_t start = p;
  bool prev = false;
  while (p < s.size()) {
    if (s[p] == '\'') {
      if (!prev || p + 1 >= s.size() || digit_value(s[p + 1]) < 0 ||
          digit_value(s[p + 1]) >= base)
        return false;
      prev = false;
      ++p;
      continue;
    }
    int d = digit_value(s[p]);
    if (d < 0 || d >= base)
      break;
    prev = true;
    ++p;
  }
  return !require || p > start;
}
struct NumberInfo {
  bool valid = false, floating = false, ud = false;
  string prefix, suffix;
  EFundamentalType type = FT_INT;
  uint64_t value = 0;
};
bool valid_int_suffix(const string &s, bool &uns, int &longs) {
  uns = false;
  longs = 0;
  if (s.empty())
    return true;
  if (s == "u" || s == "U") {
    uns = true;
    return true;
  }
  if (s == "l" || s == "L") {
    longs = 1;
    return true;
  }
  if (s == "ll" || s == "LL") {
    longs = 2;
    return true;
  }
  if (s == "ul" || s == "uL" || s == "Ul" || s == "UL" || s == "lu" ||
      s == "Lu" || s == "lU" || s == "LU") {
    uns = true;
    longs = 1;
    return true;
  }
  if (s == "ull" || s == "uLL" || s == "Ull" || s == "ULL" || s == "llu" ||
      s == "llU" || s == "LLu" || s == "LLU") {
    uns = true;
    longs = 2;
    return true;
  }
  return false;
}
bool scan_uint(const string &digits, int base, uint64_t &value) {
  value = 0;
  for (char c : digits) {
    if (c == '\'')
      continue;
    int d = digit_value(c);
    if (d < 0 || d >= base ||
        value > (UINT64_MAX - (unsigned)d) / (unsigned)base)
      return false;
    value = value * base + (unsigned)d;
  }
  return true;
}
NumberInfo parse_number(const string &s) {
  NumberInfo z;
  string body = s;
  size_t ud_at = s.find('_');
  if (ud_at != string::npos) {
    if (ud_at == 0 || ud_at + 1 == s.size())
      return z;
    for (size_t i = ud_at + 1; i < s.size(); ++i)
      if (!ident_tail(static_cast<unsigned char>(s[i])))
        return z;
    body = s.substr(0, ud_at);
    z.suffix = s.substr(ud_at);
    z.ud = true;
  }
  if (body.empty())
    return z;

  size_t p = 0;
  int base = 10;
  bool floating = false;
  size_t digits_begin = 0, digits_end = 0;
  auto digits = [&](int radix, bool required) {
    size_t start = p;
    while (p < body.size()) {
      int d = digit_value(body[p]);
      if (d < 0 || d >= radix)
        break;
      ++p;
    }
    return !required || p != start;
  };

  if (body.size() >= 2 && body[0] == '0' &&
      (body[1] == 'x' || body[1] == 'X')) {
    base = 16;
    p = 2;
    digits_begin = p;
    size_t before_begin = p;
    digits(16, false);
    bool before = p != before_begin;
    bool dot = p < body.size() && body[p] == '.';
    bool after = false;
    if (dot) {
      ++p;
      size_t after_begin = p;
      digits(16, false);
      after = p != after_begin;
      if (!before && !after)
        return z;
    } else if (!before) {
      return z;
    }
    digits_end = p;
    if (p < body.size() && (body[p] == 'p' || body[p] == 'P')) {
      floating = true;
      ++p;
      if (p < body.size() && (body[p] == '+' || body[p] == '-'))
        ++p;
      if (!digits(10, true))
        return z;
    } else if (dot) {
      return z; // a hexadecimal floating literal requires a binary exponent
    }
  } else {
    bool leading_dot = body[0] == '.';
    if (leading_dot) {
      floating = true;
      ++p;
      digits_begin = p;
      if (!digits(10, true))
        return z;
      digits_end = p;
    } else {
      digits_begin = p;
      if (body[p] == '0') {
        base = 8;
        ++p;
        while (p < body.size() && body[p] >= '0' && body[p] <= '9') {
          if (body[p] > '7')
            return z;
          ++p;
        }
      } else {
        if (body[p] < '1' || body[p] > '9')
          return z;
        while (p < body.size() && body[p] >= '0' && body[p] <= '9')
          ++p;
      }
      digits_end = p;
      if (p < body.size() && body[p] == '.') {
        floating = true;
        ++p;
        if (!digits(10, false))
          return z;
      }
    }
    if (p < body.size() && (body[p] == 'e' || body[p] == 'E')) {
      floating = true;
      ++p;
      if (p < body.size() && (body[p] == '+' || body[p] == '-'))
        ++p;
      if (!digits(10, true))
        return z;
    }
  }

  if (floating) {
    if (p < body.size() && string("fFlL").find(body[p]) != string::npos)
      ++p;
    if (p != body.size())
      return z;
    z.valid = true;
    z.floating = true;
    z.prefix = body;
    return z;
  }

  size_t number_end = p;
  string suffix = body.substr(number_end);
  bool uns = false;
  int longs = 0;
  if (z.ud) {
    // The ud-suffix follows the literal-number, not an integer-suffix.
    if (!suffix.empty())
      return z;
  } else if (!valid_int_suffix(suffix, uns, longs)) {
    return z;
  }
  uint64_t val = 0;
  if (z.ud) {
    // UDL classification depends on the literal grammar and suffix, not whether
    // the unsuffixed spelling fits any built-in integer type.
    z.valid = true;
    z.prefix = body;
    return z;
  }
  for (size_t i = digits_begin; i < digits_end; ++i) {
    int d = digit_value(body[i]);
    if (d < 0 || d >= base || val > (UINT64_MAX - (unsigned)d) / (unsigned)base)
      return z;
    val = val * (unsigned)base + (unsigned)d;
  }

  const uint64_t imax = INT32_MAX, umax = UINT32_MAX, lmax = INT64_MAX;
  if (longs == 2) {
    if (uns) {
      z.type = FT_UNSIGNED_LONG_LONG_INT;
    } else if (val <= lmax) {
      z.type = FT_LONG_LONG_INT;
    } else if (base != 10) {
      // For non-decimal constants the C++11 candidate list includes the
      // corresponding unsigned type before advancing to the next rank.
      z.type = FT_UNSIGNED_LONG_LONG_INT;
    } else {
      return z;
    }
  } else if (longs == 1) {
    if (uns) {
      z.type = FT_UNSIGNED_LONG_INT;
    } else if (val <= lmax) {
      z.type = FT_LONG_INT;
    } else if (base != 10) {
      z.type = FT_UNSIGNED_LONG_INT;
    } else {
      return z;
    }
  } else if (uns) {
    z.type = val <= umax ? FT_UNSIGNED_INT : FT_UNSIGNED_LONG_INT;
  } else if (base == 10) {
    if (val <= imax)
      z.type = FT_INT;
    else if (val <= lmax)
      z.type = FT_LONG_INT;
    else
      return z;
  } else {
    if (val <= imax)
      z.type = FT_INT;
    else if (val <= umax)
      z.type = FT_UNSIGNED_INT;
    else if (val <= lmax)
      z.type = FT_LONG_INT;
    else
      z.type = FT_UNSIGNED_LONG_INT;
  }
  z.valid = true;
  z.value = val;
  z.prefix = body.substr(0, number_end);
  return z;
}

template <class T>
void emit_scalar(DebugPostTokenOutputStream &o, const string &s,
                 EFundamentalType t, T v) {
  o.emit_literal(s, t, &v, sizeof(v));
}
void emit_integer(DebugPostTokenOutputStream &o, const string &s,
                  const NumberInfo &n) {
  switch (n.type) {
  case FT_INT:
    emit_scalar(o, s, n.type, (int)n.value);
    break;
  case FT_UNSIGNED_INT:
    emit_scalar(o, s, n.type, (unsigned int)n.value);
    break;
  case FT_LONG_INT:
    emit_scalar(o, s, n.type, (long)n.value);
    break;
  case FT_UNSIGNED_LONG_INT:
    emit_scalar(o, s, n.type, (unsigned long)n.value);
    break;
  case FT_LONG_LONG_INT:
    emit_scalar(o, s, n.type, (long long)n.value);
    break;
  case FT_UNSIGNED_LONG_LONG_INT:
    emit_scalar(o, s, n.type, (unsigned long long)n.value);
    break;
  default:
    o.emit_invalid(s);
  }
}
uint32_t decode_escape(const vector<uint32_t> &v, size_t &p,
                       bool &numeric_escape) {
  if (p >= v.size())
    throw runtime_error("bad escape");
  uint32_t c = v[p++];
  switch (c) {
  case '\'': return '\'';
  case '"': return '"';
  case '?': return '?';
  case '\\': return '\\';
  case 'a': return 7;
  case 'b': return 8;
  case 'f': return 12;
  case 'n': return 10;
  case 'r': return 13;
  case 't': return 9;
  case 'v': return 11;
  default: break;
  }
  if (c >= '0' && c <= '7') {
    numeric_escape = true;
    uint32_t x = c - '0';
    for (int k = 1; k < 3 && p < v.size() && v[p] >= '0' && v[p] <= '7'; ++k)
      x = x * 8 + v[p++] - '0';
    return x;
  }
  if (c == 'x') {
    numeric_escape = true;
    if (p == v.size() || digit_value(static_cast<char>(v[p])) < 0 ||
        digit_value(static_cast<char>(v[p])) > 15)
      throw runtime_error("empty hex escape");
    uint32_t x = 0;
    while (p < v.size() && digit_value(static_cast<char>(v[p])) >= 0 &&
           digit_value(static_cast<char>(v[p])) < 16) {
      unsigned d = static_cast<unsigned>(digit_value(static_cast<char>(v[p])));
      if (x > (UINT32_MAX - d) / 16)
        throw runtime_error("escape overflow");
      x = x * 16 + d;
      ++p;
    }
    return x;
  }
  return c;
}

struct ParsedLiteral {
  bool ok = false;
  string prefix, suffix;
  vector<LiteralUnit> values;
};
ParsedLiteral parse_quoted(const string &s, bool character) {
  ParsedLiteral r;
  size_t q = s.find_first_of("\"'");
  if (q == string::npos)
    return r;
  string pre = s.substr(0, q);
  if (pre != "" && pre != "u8" && pre != "u" && pre != "U" && pre != "L" &&
      pre != "R" && pre != "u8R" && pre != "uR" && pre != "UR" && pre != "LR")
    return r;
  r.prefix = pre;
  bool raw = !pre.empty() && pre.back() == 'R';
  size_t body = q + 1, close = string::npos;
  size_t after_quote = 0;
  if (raw) {
    size_t par = s.find('(', body);
    if (par == string::npos)
      return r;
    string delim = s.substr(body, par - body);
    string end = ")" + delim + "\"";
    close = s.find(end, par + 1);
    if (close == string::npos)
      return r;
    vector<uint32_t> raw_values = decode_utf8_local(
        s.substr(par + 1, close - par - 1));
    for (uint32_t c : raw_values)
      r.values.push_back({c, false});
    after_quote = close + end.size();
  } else {
    uint32_t quote = s[q];
    size_t p = body;
    bool escaped = false;
    while (p < s.size()) {
      if (!escaped && static_cast<unsigned char>(s[p]) == quote) {
        close = p;
        break;
      }
      if (!escaped && s[p] == '\\')
        escaped = true;
      else
        escaped = false;
      ++p;
    }
    if (close == string::npos)
      return r;
    vector<uint32_t> input = decode_utf8_local(s.substr(body, close - body));
    try {
      for (size_t p = 0; p < input.size();) {
        uint32_t c = input[p++];
        bool numeric = false;
        if (c == '\\')
          c = decode_escape(input, p, numeric);
        if (!numeric && !valid_cp(c))
          return ParsedLiteral();
        r.values.push_back({c, numeric});
      }
    } catch (const exception &) {
      return ParsedLiteral();
    }
    after_quote = close + 1;
  }
  if (after_quote < s.size())
    r.suffix = s.substr(after_quote);
  if (!r.suffix.empty() && r.suffix[0] != '_')
    return ParsedLiteral();
  if (character && r.values.size() != 1)
    return ParsedLiteral();
  r.ok = true;
  return r;
}

void append_units(vector<uint32_t> &out, const vector<LiteralUnit> &values,
                  const string &encoding) {
  for (const LiteralUnit &unit : values) {
    uint32_t c = unit.value;
    if (unit.numeric_escape) {
      out.push_back(c);
    } else if (encoding == "u8" || encoding.empty()) {
      if (c <= 0x7f)
        out.push_back(c);
      else if (c <= 0x7ff) {
        out.push_back(0xc0 | (c >> 6));
        out.push_back(0x80 | (c & 63));
      } else if (c <= 0xffff) {
        out.push_back(0xe0 | (c >> 12));
        out.push_back(0x80 | ((c >> 6) & 63));
        out.push_back(0x80 | (c & 63));
      } else {
        out.push_back(0xf0 | (c >> 18));
        out.push_back(0x80 | ((c >> 12) & 63));
        out.push_back(0x80 | ((c >> 6) & 63));
        out.push_back(0x80 | (c & 63));
      }
    } else if (encoding == "u" && c > 0xffff) {
      c -= 0x10000;
      out.push_back(0xd800 + (c >> 10));
      out.push_back(0xdc00 + (c & 1023));
    } else {
      out.push_back(c);
    }
  }
}

void emit_units(DebugPostTokenOutputStream &o, const string &src,
                const string &suffix, const string &encoding,
                vector<uint32_t> units, bool ud) {
  units.push_back(0);
  EFundamentalType t =
      encoding == "u"
          ? FT_CHAR16_T
          : (encoding == "U" ? FT_CHAR32_T
                             : (encoding == "L" ? FT_WCHAR_T : FT_CHAR));
  if (t == FT_CHAR) {
    vector<char> b;
    for (uint32_t x : units)
      b.push_back((char)x);
    if (ud)
      o.emit_user_defined_literal_string_array(src, suffix, units.size(), t,
                                               b.data(), b.size());
    else
      o.emit_literal_array(src, units.size(), t, b.data(), b.size());
  } else if (t == FT_CHAR16_T) {
    vector<char16_t> b;
    for (uint32_t x : units)
      b.push_back((char16_t)x);
    if (ud)
      o.emit_user_defined_literal_string_array(src, suffix, units.size(), t,
                                               b.data(), b.size() * 2);
    else
      o.emit_literal_array(src, units.size(), t, b.data(), b.size() * 2);
  } else {
    vector<uint32_t> b = units;
    if (ud)
      o.emit_user_defined_literal_string_array(src, suffix, units.size(), t,
                                               b.data(), b.size() * 4);
    else
      o.emit_literal_array(src, units.size(), t, b.data(), b.size() * 4);
  }
}
void emit_character(DebugPostTokenOutputStream &o, const string &s, bool ud) {
  ParsedLiteral p = parse_quoted(s, true);
  if (!p.ok) {
    o.emit_invalid(s);
    return;
  }
  uint32_t c = p.values[0].value;
  EFundamentalType t = FT_CHAR;
  if (p.prefix == "u")
    t = FT_CHAR16_T;
  else if (p.prefix == "U")
    t = FT_CHAR32_T;
  else if (p.prefix == "L")
    t = FT_WCHAR_T;
  else if (c > 127)
    t = FT_INT;
  if (t == FT_CHAR16_T && c > 0xffff) {
    o.emit_invalid(s);
    return;
  }
  if (t == FT_CHAR) {
    char x = (char)c;
    if (ud)
      o.emit_user_defined_literal_character(s, p.suffix, t, &x, sizeof x);
    else
      o.emit_literal(s, t, &x, sizeof x);
  } else if (t == FT_INT) {
    int x = (int)c;
    o.emit_literal(s, t, &x, sizeof x);
  } else if (t == FT_CHAR16_T) {
    char16_t x = (char16_t)c;
    if (ud)
      o.emit_user_defined_literal_character(s, p.suffix, t, &x, sizeof x);
    else
      o.emit_literal(s, t, &x, sizeof x);
  } else {
    uint32_t x = c;
    if (ud)
      o.emit_user_defined_literal_character(s, p.suffix, t, &x, sizeof x);
    else
      o.emit_literal(s, t, &x, sizeof x);
  }
}
void PostTokenProcessor::process_string(const string &source) {
  if (!string_run_open) {
    string_run_open = true;
    string_run_invalid = false;
    string_source.clear();
    string_suffix.clear();
    string_encoding.clear();
    string_values.clear();
  } else {
    string_source += ' ';
  }
  string_source += source;

  ParsedLiteral parsed = parse_quoted(source, false);
  if (!parsed.ok) {
    string_run_invalid = true;
    return;
  }
  string encoding = parsed.prefix;
  if (!encoding.empty() && encoding.back() == 'R')
    encoding.erase(encoding.size() - 1);
  if (encoding == "R")
    encoding.clear();
  if (!encoding.empty()) {
    if (!string_encoding.empty() && string_encoding != encoding)
      string_run_invalid = true;
    string_encoding = encoding;
  }
  if (!parsed.suffix.empty()) {
    if (!string_suffix.empty() && string_suffix != parsed.suffix)
      string_run_invalid = true;
    string_suffix = parsed.suffix;
  }
  string_values.insert(string_values.end(), parsed.values.begin(),
                       parsed.values.end());
}

void PostTokenProcessor::flush_strings() {
  if (!string_run_open)
    return;
  const uint64_t max_unit =
      (string_encoding == "u" ? 0xffffULL
                               : (string_encoding == "U" ||
                                          string_encoding == "L"
                                      ? UINT32_MAX
                                      : 0xffULL));
  for (const LiteralUnit &unit : string_values) {
    if (unit.numeric_escape && unit.value > max_unit)
      string_run_invalid = true;
  }
  if (string_run_invalid) {
    output.emit_invalid(string_source);
  } else {
    vector<uint32_t> units;
    append_units(units, string_values, string_encoding);
    emit_units(output, string_source, string_suffix, string_encoding, units,
               !string_suffix.empty());
  }
  string_run_open = false;
  string_run_invalid = false;
  string_source.clear();
  string_suffix.clear();
  string_encoding.clear();
  string_values.clear();
}

void PostTokenProcessor::process_nonstring(const string &kind,
                                           const string &source) {
  flush_strings();
  if (kind == "invalid") {
    output.emit_invalid(source);
  } else if (kind == "identifier") {
    auto it = StringToTokenTypeMap.find(source);
    if (it == StringToTokenTypeMap.end())
      output.emit_identifier(source);
    else
      output.emit_simple(source, it->second);
  } else if (kind == "operator") {
    auto it = StringToTokenTypeMap.find(source);
    if (source == "#" || source == "##" || source == "%:" ||
        source == "%:%:" || it == StringToTokenTypeMap.end())
      output.emit_invalid(source);
    else
      output.emit_simple(source, it->second);
  } else if (kind == "number") {
    NumberInfo n = parse_number(source);
    if (!n.valid)
      output.emit_invalid(source);
    else if (n.ud) {
      if (n.floating)
        output.emit_user_defined_literal_floating(source, n.suffix, n.prefix);
      else
        output.emit_user_defined_literal_integer(source, n.suffix, n.prefix);
    } else if (n.floating) {
      size_t e = source.size() - 1;
      char last = source[e];
      string f = (last == 'f' || last == 'F') ? source.substr(0, e) : source;
      string d = (last == 'l' || last == 'L') ? source.substr(0, e) : source;
      if (last == 'f' || last == 'F') {
        float v = PA2Decode_float(f);
        emit_scalar(output, source, FT_FLOAT, v);
      } else if (last == 'l' || last == 'L') {
        long double v = PA2Decode_long_double(d);
        emit_scalar(output, source, FT_LONG_DOUBLE, v);
      } else {
        double v = PA2Decode_double(source);
        emit_scalar(output, source, FT_DOUBLE, v);
      }
    } else {
      emit_integer(output, source, n);
    }
  } else if (kind == "character" || kind == "ud-character") {
    emit_character(output, source, kind == "ud-character");
  } else {
    output.emit_invalid(source);
  }
}
} // namespace

bool HasBatchStdinArg(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (string(argv[i]) == "--batch-stdin")
      return true;
  }
  return false;
}

int RunBatchMode() {
  string record;
  while (getline(cin, record)) {
    if (record.empty())
      continue;
    vector<string> fields;
    size_t at = 0;
    for (;;) {
      size_t tab = record.find('\t', at);
      fields.push_back(record.substr(at, tab == string::npos ? tab : tab - at));
      if (tab == string::npos)
        break;
      at = tab + 1;
    }
    if (fields.size() != 3) {
      cout << "EXIT_FAILURE\n";
      continue;
    }
    ifstream input(fields[2].c_str(), ios::binary);
    if (!input) {
      ofstream err(fields[1].c_str());
      err << "ERROR: cannot read input\n";
      cout << "EXIT_FAILURE\n";
      continue;
    }
    ostringstream data;
    data << input.rdbuf();
    ofstream outFile(fields[0].c_str(), ios::binary);
    if (!outFile) {
      cout << "EXIT_FAILURE\n";
      continue;
    }
    streambuf *oldOut = cout.rdbuf(outFile.rdbuf());
    int status = 0;
    try {
      DebugPostTokenOutputStream output;
      PostTokenProcessor stream(output);
      ScanPreprocessingTokens(data.str(), stream);
    } catch (exception &e) {
      cerr << "ERROR: " << e.what() << endl;
      status = 1;
    }
    cout.rdbuf(oldOut);
    cout << (status ? "EXIT_FAILURE" : "EXIT_SUCCESS") << endl;
  }
  return 0;
}

int main(int argc, char **argv) {
  try {
    if (HasBatchStdinArg(argc, argv))
      return RunBatchMode();
    ostringstream data;
    data << cin.rdbuf();
    DebugPostTokenOutputStream output;
    PostTokenProcessor stream(output);
    ScanPreprocessingTokens(data.str(), stream);
    return EXIT_SUCCESS;
  } catch (exception &e) {
    cerr << "ERROR: " << e.what() << endl;
    return EXIT_FAILURE;
  }
}
