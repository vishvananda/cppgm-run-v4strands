#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

#include "preprocess/tokens/DebugPPTokenStream.h"
#include "preprocess/tokens/IPPTokenStream.h"
#include "support/not_implemented.h"

// Translation features you need to implement:
// - utf8 decoder
// - utf8 encoder
// - universal-character-name decoder
// - trigraphs
// - line splicing
// - newline at eof
// - comment striping (can be part of whitespace-sequence)

// EndOfFile: synthetic "character" to represent the end of source file
constexpr int EndOfFile = -1;

// given hex digit character c, return its value
int HexCharToValue(int c) {
  switch (c) {
  case '0':
    return 0;
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '4':
    return 4;
  case '5':
    return 5;
  case '6':
    return 6;
  case '7':
    return 7;
  case '8':
    return 8;
  case '9':
    return 9;
  case 'A':
    return 10;
  case 'a':
    return 10;
  case 'B':
    return 11;
  case 'b':
    return 11;
  case 'C':
    return 12;
  case 'c':
    return 12;
  case 'D':
    return 13;
  case 'd':
    return 13;
  case 'E':
    return 14;
  case 'e':
    return 14;
  case 'F':
    return 15;
  case 'f':
    return 15;
  default:
    throw logic_error("HexCharToValue of nonhex char");
  }
}

// See C++ standard 2.11 Identifiers and Appendix/Annex E.1
const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted = {
    {0xA8, 0xA8},       {0xAA, 0xAA},       {0xAD, 0xAD},
    {0xAF, 0xAF},       {0xB2, 0xB5},       {0xB7, 0xBA},
    {0xBC, 0xBE},       {0xC0, 0xD6},       {0xD8, 0xF6},
    {0xF8, 0xFF},       {0x100, 0x167F},    {0x1681, 0x180D},
    {0x180F, 0x1FFF},   {0x200B, 0x200D},   {0x202A, 0x202E},
    {0x203F, 0x2040},   {0x2054, 0x2054},   {0x2060, 0x206F},
    {0x2070, 0x218F},   {0x2460, 0x24FF},   {0x2776, 0x2793},
    {0x2C00, 0x2DFF},   {0x2E80, 0x2FFF},   {0x3004, 0x3007},
    {0x3021, 0x302F},   {0x3031, 0x303F},   {0x3040, 0xD7FF},
    {0xF900, 0xFD3D},   {0xFD40, 0xFDCF},   {0xFDF0, 0xFE44},
    {0xFE47, 0xFFFD},   {0x10000, 0x1FFFD}, {0x20000, 0x2FFFD},
    {0x30000, 0x3FFFD}, {0x40000, 0x4FFFD}, {0x50000, 0x5FFFD},
    {0x60000, 0x6FFFD}, {0x70000, 0x7FFFD}, {0x80000, 0x8FFFD},
    {0x90000, 0x9FFFD}, {0xA0000, 0xAFFFD}, {0xB0000, 0xBFFFD},
    {0xC0000, 0xCFFFD}, {0xD0000, 0xDFFFD}, {0xE0000, 0xEFFFD}};

// See C++ standard 2.11 Identifiers and Appendix/Annex E.2
const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted = {
    {0x300, 0x36F}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF}, {0xFE20, 0xFE2F}};

// See C++ standard 2.13 Operators and punctuators
const unordered_set<string> Digraph_IdentifierLike_Operators = {
    "new", "delete", "and", "and_eq", "bitand", "bitor", "compl",
    "not", "not_eq", "or",  "or_eq",  "xor",    "xor_eq"};

// See `simple-escape-sequence` grammar
const unordered_set<int> SimpleEscapeSequence_CodePoints = {
    '\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'};

// PA1 tokenizer. Translation and token emission are kept in one pass-oriented
// component; later front-end phases consume the same IPPTokenStream contract.
namespace {

bool in_ranges(uint32_t c, const vector<pair<int, int>> &ranges) {
  size_t lo = 0, hi = ranges.size();
  while (lo < hi) {
    size_t m = lo + (hi - lo) / 2;
    if (c < static_cast<uint32_t>(ranges[m].first))
      hi = m;
    else if (c > static_cast<uint32_t>(ranges[m].second))
      lo = m + 1;
    else
      return true;
  }
  return false;
}
bool digit(uint32_t c) { return c >= '0' && c <= '9'; }
bool hex_digit(uint32_t c) {
  return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool ident_start(uint32_t c) {
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (in_ranges(c, AnnexE1_Allowed_RangesSorted) &&
          !in_ranges(c, AnnexE2_DisallowedInitially_RangesSorted));
}
bool ident_cont(uint32_t c) {
  return ident_start(c) || digit(c) ||
         in_ranges(c, AnnexE2_DisallowedInitially_RangesSorted);
}
void append_utf8(string &out, uint32_t c) {
  if (c <= 0x7f)
    out += char(c);
  else if (c <= 0x7ff) {
    out += char(0xc0 | (c >> 6));
    out += char(0x80 | (c & 63));
  } else if (c <= 0xffff) {
    out += char(0xe0 | (c >> 12));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  } else {
    out += char(0xf0 | (c >> 18));
    out += char(0x80 | ((c >> 12) & 63));
    out += char(0x80 | ((c >> 6) & 63));
    out += char(0x80 | (c & 63));
  }
}
vector<uint32_t> decode_utf8(const string &s) {
  vector<uint32_t> v;
  for (size_t i = 0; i < s.size();) {
    unsigned char b = s[i++];
    uint32_t c;
    unsigned n;
    if (b < 0x80) {
      c = b;
      n = 0;
    } else if (b >= 0xc2 && b <= 0xdf) {
      c = b & 31;
      n = 1;
    } else if (b >= 0xe0 && b <= 0xef) {
      c = b & 15;
      n = 2;
    } else if (b >= 0xf0 && b <= 0xf4) {
      c = b & 7;
      n = 3;
    } else
      throw logic_error("invalid UTF-8 leading byte");
    if (i + n > s.size())
      throw logic_error("invalid UTF-8 continuation byte");
    for (unsigned k = 0; k < n; k++) {
      unsigned char x = s[i++];
      if ((x & 0xc0) != 0x80)
        throw logic_error("invalid UTF-8 continuation byte");
      c = (c << 6) | (x & 63);
    }
    if ((n == 1 && c < 0x80) || (n == 2 && c < 0x800) ||
        (n == 3 && c < 0x10000) || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      throw logic_error("invalid UTF-8 code point");
    v.push_back(c);
  }
  return v;
}
string utf8(const vector<uint32_t> &v, size_t a, size_t b) {
  string s;
  for (size_t i = a; i < b; i++)
    append_utf8(s, v[i]);
  return s;
}
uint32_t tri(uint32_t c) {
  switch (c) {
  case '=':
    return '#';
  case '/':
    return '\\';
  case '\'':
    return '^';
  case '(':
    return '[';
  case ')':
    return ']';
  case '!':
    return '|';
  case '<':
    return '{';
  case '>':
    return '}';
  case '-':
    return '~';
  default:
    return 0;
  }
}
struct Phase12View {
  vector<uint32_t> chars;
  vector<uint32_t> compact_source_offsets;
  vector<size_t> wide_source_offsets;
  bool wide_offsets;

  explicit Phase12View(bool wide = false) : wide_offsets(wide) {}
  void push(uint32_t c, size_t source_offset) {
    chars.push_back(c);
    if (wide_offsets)
      wide_source_offsets.push_back(source_offset);
    else
      compact_source_offsets.push_back(static_cast<uint32_t>(source_offset));
  }
  size_t source_offset(size_t i) const {
    return wide_offsets ? wide_source_offsets[i] : compact_source_offsets[i];
  }
  void release_offsets() {
    vector<uint32_t>().swap(compact_source_offsets);
    vector<size_t>().swap(wide_source_offsets);
  }
};

Phase12View make_phase12_view(const vector<uint32_t> &source) {
  Phase12View view(source.size() > numeric_limits<uint32_t>::max());
  // Fuse phase-1 trigraph replacement with phase-2 splicing. The source
  // buffer stays immutable; offsets retain the physical origin for raw-text
  // restoration, without materializing separate phase-1 character/offset
  // arrays.
  for (size_t i = 0; i < source.size();) {
    size_t origin = i;
    uint32_t c;
    if (i + 2 < source.size() && source[i] == '?' && source[i + 1] == '?' &&
        tri(source[i + 2])) {
      c = tri(source[i + 2]);
      i += 3;
    } else {
      c = source[i++];
    }
    if (c == '\\' && i < source.size() && source[i] == '\n') {
      ++i;
      continue;
    }
    view.push(c, origin);
  }
  return view;
}

bool raw_prefix_at(const vector<uint32_t> &s, size_t i, size_t &quote) {
  if (i + 3 < s.size() && s[i] == 'u' && s[i + 1] == '8' && s[i + 2] == 'R' &&
      s[i + 3] == '"')
    quote = i + 3;
  else if (i + 2 < s.size() && (s[i] == 'u' || s[i] == 'U' || s[i] == 'L') &&
           s[i + 1] == 'R' && s[i + 2] == '"')
    quote = i + 2;
  else if (i + 1 < s.size() && s[i] == 'R' && s[i + 1] == '"')
    quote = i + 1;
  else
    return false;
  return true;
}

size_t raw_literal_end_after_quote(const vector<uint32_t> &s, size_t quote) {
  size_t open = quote + 1;
  while (open < s.size() && s[open] != '(' && s[open] != '\n' &&
         open - quote <= 17) {
    if (s[open] == ' ' || s[open] == '\\' || s[open] == ')' ||
        s[open] == '\t' || s[open] == '\v' || s[open] == '\f')
      return quote;
    ++open;
  }
  if (open >= s.size() || s[open] != '(' || open - quote - 1 > 16)
    return quote;
  string close = ")" + utf8(s, quote + 1, open) + "\"";
  for (size_t p = open + 1; p + close.size() <= s.size(); ++p) {
    size_t k = 0;
    while (k < close.size() && s[p + k] == static_cast<unsigned char>(close[k]))
      ++k;
    if (k == close.size())
      return p + close.size();
  }
  // A valid raw opener with no close is one partial token through EOF.
  return s.size();
}

bool scan_ucn(const vector<uint32_t> &s, size_t p, uint32_t &value,
              size_t &end) {
  if (p + 2 > s.size() || s[p] != '\\' || (s[p + 1] != 'u' && s[p + 1] != 'U'))
    return false;
  size_t digits = s[p + 1] == 'u' ? 4 : 8;
  if (p + 2 + digits > s.size())
    return false;
  value = 0;
  for (size_t k = 0; k < digits; ++k) {
    if (!hex_digit(s[p + 2 + k]))
      return false;
    value = (value << 4) | HexCharToValue(s[p + 2 + k]);
  }
  end = p + 2 + digits;
  return true;
}

struct RawLiteralRange {
  size_t prefix_end;
  size_t translated_end;
  size_t source_quote;
  size_t source_end;
};

// Identify raw literals in the phase-1/2 view so comment and token boundaries
// are correct. This lexical walk skips comments, ordinary literals,
// identifiers, and pp-numbers: an R" sequence inside one of those
// preprocessing tokens is not a raw-string introducer.
vector<RawLiteralRange> raw_literal_ranges(const Phase12View &view,
                                           const vector<uint32_t> &source) {
  const vector<uint32_t> &s = view.chars;
  vector<RawLiteralRange> ranges;
  size_t i = 0;
  bool beginning_of_line = true;
  bool directive_name_pending = false;
  bool include_header_pending = false;
  while (i < s.size()) {
    uint32_t c = s[i];
    if (c == '\n') {
      ++i;
      beginning_of_line = true;
      directive_name_pending = false;
      include_header_pending = false;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r') {
      ++i;
      continue;
    }
    if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
      i += 2;
      while (i < s.size() && s[i] != '\n')
        ++i;
      continue;
    }
    if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
      i += 2;
      while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
        if (s[i] == '\n') {
          beginning_of_line = true;
          directive_name_pending = false;
          include_header_pending = false;
        }
        ++i;
      }
      i = i + 1 < s.size() ? i + 2 : s.size();
      continue;
    }
    // Header names are context-dependent preprocessing tokens. An R\" or
    // trigraph-looking sequence inside an #include header is not a raw
    // string and must remain subject to phase-1/2 translation.
    if (include_header_pending && (c == '<' || c == '"')) {
      uint32_t close = c == '<' ? '>' : '"';
      ++i;
      while (i < s.size() && s[i] != '\n' && s[i] != close)
        ++i;
      if (i < s.size() && s[i] == close)
        ++i;
      include_header_pending = false;
      directive_name_pending = false;
      beginning_of_line = false;
      continue;
    }
    if (include_header_pending)
      include_header_pending = false;
    // Recognize the directive introducer only at the start of a logical
    // line. The alternative # spelling is %:, while %:%: is ##.
    bool directive_hash =
        beginning_of_line &&
        (c == '#' || (i + 1 < s.size() && c == '%' && s[i + 1] == ':')) &&
        !(i + 3 < s.size() && c == '%' && s[i + 1] == ':' && s[i + 2] == '%' &&
          s[i + 3] == ':');
    if (directive_hash) {
      i += (c == '#' ? 1 : 2);
      directive_name_pending = true;
      include_header_pending = false;
      beginning_of_line = false;
      continue;
    }
    size_t quote = i;
    if (raw_prefix_at(s, i, quote)) {
      size_t source_quote = view.source_offset(quote);
      size_t source_end = raw_literal_end_after_quote(source, source_quote);
      if (source_end != source_quote) {
        size_t translated_end = quote + 1;
        while (translated_end < view.chars.size() &&
               view.source_offset(translated_end) < source_end)
          ++translated_end;
        RawLiteralRange range = {quote + 1, translated_end, source_quote,
                                 source_end};
        ranges.push_back(range);
        i = translated_end;
        beginning_of_line = false;
        directive_name_pending = false;
        continue;
      }
    }
    // Skip ordinary string/character literals, including their encoding
    // prefixes, before looking for any later raw-string token.
    size_t ordinary_quote = i;
    if (i + 2 < s.size() && s[i] == 'u' && s[i + 1] == '8' && s[i + 2] == '"')
      ordinary_quote = i + 2;
    else if (i + 1 < s.size() && (s[i] == 'u' || s[i] == 'U' || s[i] == 'L') &&
             (s[i + 1] == '"' || s[i + 1] == '\''))
      ordinary_quote = i + 1;
    if (s[ordinary_quote] == '"' || s[ordinary_quote] == '\'') {
      uint32_t delimiter = s[ordinary_quote++];
      while (ordinary_quote < s.size() && s[ordinary_quote] != '\n') {
        if (s[ordinary_quote] == '\\' && ordinary_quote + 1 < s.size())
          ordinary_quote += 2;
        else if (s[ordinary_quote++] == delimiter)
          break;
      }
      i = ordinary_quote;
      // A user-defined suffix belongs to the ordinary literal's
      // preprocessing token. Its final R must not begin a raw string.
      uint32_t suffix_char = i < s.size() ? s[i] : 0;
      size_t suffix_end = i;
      bool suffix_ucn = scan_ucn(s, i, suffix_char, suffix_end);
      if ((suffix_ucn && ident_start(suffix_char)) ||
          (!suffix_ucn && ident_start(suffix_char))) {
        i = suffix_ucn ? suffix_end : i + 1;
        while (i < s.size()) {
          uint32_t suffix_body = s[i];
          suffix_end = i;
          suffix_ucn = scan_ucn(s, i, suffix_body, suffix_end);
          if (suffix_ucn && ident_cont(suffix_body))
            i = suffix_end;
          else if (!suffix_ucn && ident_cont(s[i]))
            ++i;
          else
            break;
        }
      }
      beginning_of_line = false;
      directive_name_pending = false;
      continue;
    }
    uint32_t decoded = c;
    size_t after = i;
    bool ucn = scan_ucn(s, i, decoded, after);
    if ((ucn && ident_start(decoded)) || ident_start(c)) {
      size_t token_start = i;
      if (ucn)
        i = after;
      else
        ++i;
      while (i < s.size()) {
        decoded = s[i];
        after = i;
        ucn = scan_ucn(s, i, decoded, after);
        if (ucn && ident_cont(decoded))
          i = after;
        else if (!ucn && ident_cont(s[i]))
          ++i;
        else
          break;
      }
      if (directive_name_pending) {
        include_header_pending = (utf8(s, token_start, i) == "include");
        directive_name_pending = false;
      }
      beginning_of_line = false;
      continue;
    }
    if (digit(c) || (c == '.' && i + 1 < s.size() && digit(s[i + 1]))) {
      ++i;
      while (i < s.size()) {
        decoded = s[i];
        after = i;
        ucn = scan_ucn(s, i, decoded, after);
        if ((ucn && ident_cont(decoded)) ||
            (!ucn && (digit(s[i]) || ident_cont(s[i]) || s[i] == '.'))) {
          if (ucn)
            i = after;
          else
            ++i;
          continue;
        }
        // C++11 pp-number signs follow only e/E, not p/P.
        if ((s[i] == '+' || s[i] == '-') && i > 0 &&
            (s[i - 1] == 'e' || s[i - 1] == 'E')) {
          ++i;
          continue;
        }
        break;
      }
      beginning_of_line = false;
      directive_name_pending = false;
      continue;
    }
    ++i;
    beginning_of_line = false;
    directive_name_pending = false;
  }
  return ranges;
}

// Translate trigraphs/splices; preserve raw-string bodies, whose phase-1/2
// transformations are reverted by the C++11 raw-string rule.
vector<uint32_t> translate(const vector<uint32_t> &source) {
  // N3485 [lex.phases] 2.2/2, clarified by CWG 1698/2747: perform physical
  // splicing first, then append LF if a nonempty source has no LF left.
  // An appended LF does not create a new splice; an EOF backslash remains.
  Phase12View view = make_phase12_view(source);
  if (!source.empty() && (view.chars.empty() || view.chars.back() != '\n'))
    view.push('\n', source.size());
  vector<RawLiteralRange> ranges = raw_literal_ranges(view, source);
  // Once raw ranges own all restoration coordinates, the per-code-point
  // source map is dead. Release it before allocating the translated result.
  view.release_offsets();
  size_t final_size = view.chars.size();
  for (size_t r = 0; r < ranges.size(); ++r) {
    const RawLiteralRange &range = ranges[r];
    const size_t restored = range.source_end - range.source_quote - 1;
    const size_t replaced = range.translated_end - range.prefix_end;
    if (replaced > final_size ||
        restored > numeric_limits<size_t>::max() - (final_size - replaced))
      throw length_error("translated source is too large");
    final_size = final_size - replaced + restored;
  }

  vector<uint32_t> result;
  if (final_size > result.max_size())
    throw length_error("translated source is too large");
  result.reserve(final_size);
  size_t cursor = 0;
  for (size_t r = 0; r < ranges.size(); ++r) {
    const RawLiteralRange &range = ranges[r];
    result.insert(result.end(), view.chars.begin() + cursor,
                  view.chars.begin() + range.prefix_end);
    result.insert(result.end(), source.begin() + range.source_quote + 1,
                  source.begin() + range.source_end);
    cursor = range.translated_end;
  }
  result.insert(result.end(), view.chars.begin() + cursor, view.chars.end());
  return result;
}

struct Scanner {
  IPPTokenStream &out;
  vector<uint32_t> s;
  size_t i;
  bool bol;
  bool directive;
  bool directive_name_pending;
  bool include_next;
  bool emitted_any;
  bool line_had;
  Scanner(IPPTokenStream &o, vector<uint32_t> x)
      : out(o), s(std::move(x)), i(0), bol(true), directive(false),
        directive_name_pending(false), include_next(false), emitted_any(false),
        line_had(false) {}
  void token(void (IPPTokenStream::*fn)(const string &), size_t a, size_t b) {
    (out.*fn)(utf8(s, a, b));
    emitted_any = true;
    line_had = true;
  }
  bool starts(size_t p, const char *x) {
    size_t k = 0;
    while (x[k]) {
      if (p + k >= s.size() || s[p + k] != static_cast<unsigned char>(x[k]))
        return false;
      ++k;
    }
    return true;
  }
  bool starts(size_t p, const string &x) {
    size_t k = 0;
    while (k < x.size()) {
      if (p + k >= s.size() || s[p + k] != static_cast<unsigned char>(x[k]))
        return false;
      ++k;
    }
    return true;
  }
  size_t ucn(size_t p, uint32_t &c) {
    if (p + 2 > s.size() || s[p] != '\\' ||
        (s[p + 1] != 'u' && s[p + 1] != 'U'))
      return p;
    const size_t digits = s[p + 1] == 'u' ? 4 : 8;
    const size_t first = p + 2;
    if (first + digits > s.size())
      return p;
    uint32_t value = 0;
    for (size_t k = 0; k < digits; ++k) {
      if (!hex_digit(s[first + k]))
        return p;
      value = (value << 4) | HexCharToValue(s[first + k]);
    }
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
        (value < 0xa0 && value != '$' && value != '@' && value != '`'))
      throw logic_error("invalid universal character value");
    c = value;
    return first + digits;
  }
  string literal_spelling(size_t begin, size_t quote, size_t close,
                          size_t end) {
    string result = utf8(s, begin, quote + 1);
    size_t p = quote + 1;
    while (p < close) {
      uint32_t value;
      size_t next = ucn(p, value);
      if (next != p && next <= close) {
        append_utf8(result, value);
        p = next;
      } else if (s[p] == '\\' && p + 1 < close) {
        result += utf8(s, p, p + 2);
        p += 2;
      } else {
        result += utf8(s, p, p + 1);
        ++p;
      }
    }
    result += utf8(s, close, close + 1);
    p = close + 1;
    while (p < end) {
      uint32_t value;
      size_t next = ucn(p, value);
      if (next != p) {
        append_utf8(result, value);
        p = next;
      } else {
        result += utf8(s, p, p + 1);
        ++p;
      }
    }
    return result;
  }
  bool scan_literal_or_header(size_t a) {
    // Header-name is enabled only after #include at logical line start.
    if (include_next && (s[i] == '<' || s[i] == '"')) {
      uint32_t q = s[i] == '<' ? '>' : '"';
      size_t j = i + 1;
      while (j < s.size() && s[j] != '\n' && s[j] != q)
        j++;
      if (j < s.size() && s[j] == q) {
        i = j + 1;
        token(&IPPTokenStream::emit_header_name, a, i);
        include_next = false;
        bol = false;
        return true;
      }
    }
    // String/character literals, with standard encoding prefixes.
    size_t quote = i;
    bool raw = false;
    string rawprefix;
    if (starts(i, "u8R\"") || starts(i, "uR\"") || starts(i, "UR\"") ||
        starts(i, "LR\"") || starts(i, "R\"")) {
      raw = true;
      quote = i + (starts(i, "u8R\"") ? 3 : starts(i, "R\"") ? 1 : 2);
    } else if (starts(i, "u8\""))
      quote = i + 2;
    else if ((s[i] == 'u' || s[i] == 'U' || s[i] == 'L') && i + 1 < s.size() &&
             (s[i + 1] == '\'' || s[i + 1] == '\"'))
      quote = i + 1;
    if (raw) {
      size_t op = quote + 1;
      while (op < s.size() && s[op] != '(' && s[op] != '\n') {
        if (s[op] == ' ' || s[op] == '\\' || s[op] == ')' || s[op] == '\t' ||
            s[op] == '\v' || s[op] == '\f' || op - quote > 16)
          throw logic_error("raw string delimiter is too long");
        op++;
      }
      if (op >= s.size() || s[op] != '(')
        throw logic_error("unterminated raw string literal");
      string d = utf8(s, quote + 1, op), close = ")" + d + "\"";
      size_t j = op + 1;
      while (j + close.size() <= s.size() &&
             utf8(s, j, j + close.size()) != close)
        j++;
      if (j + close.size() > s.size())
        throw logic_error("unterminated raw string literal");
      i = j + close.size();
      size_t after = i;
      // PA2 distinguishes the ud-suffix spelling beginning with underscore;
      // a following ordinary identifier remains a separate preprocessing token.
      if (i < s.size() && s[i] == '_') {
        ++i;
        while (i < s.size() && ident_cont(s[i]))
          ++i;
      }
      token(i > after ? &IPPTokenStream::emit_user_defined_string_literal
                      : &IPPTokenStream::emit_string_literal,
            a, i);
      bol = false;
      return true;
    }
    if (s[quote] == '\'' || s[quote] == '\"') {
      uint32_t q = s[quote];
      bool ch = q == '\'';
      size_t j = quote + 1;
      bool closed = false;
      while (j < s.size() && s[j] != '\n') {
        if (s[j] == q) {
          j++;
          closed = true;
          break;
        }
        if (s[j] == '\\') {
          if (j + 1 >= s.size() || s[j + 1] == '\n')
            break;
          uint32_t e = s[j + 1];
          if (SimpleEscapeSequence_CodePoints.count(e)) {
            j += 2;
            continue;
          }
          if (e >= '0' && e <= '7') {
            size_t k = j + 1;
            while (k < s.size() && k < j + 4 && s[k] >= '0' && s[k] <= '7')
              k++;
            j = k;
            continue;
          }
          if (e == 'x') {
            size_t k = j + 2;
            while (k < s.size() && hex_digit(s[k]))
              k++;
            if (k == j + 2)
              throw logic_error("hex escape has no digits");
            j = k;
            continue;
          }
          uint32_t escaped;
          size_t un = ucn(j, escaped);
          if (un != j) {
            j = un;
            continue;
          }
          throw logic_error("invalid escape sequence");
        }
        uint32_t cv;
        size_t uj = ucn(j, cv);
        j = uj == j ? j + 1 : uj;
      }
      if (!closed)
        throw logic_error("unterminated quoted literal");
      i = j;
      size_t after = i;
      // In an operator-function-id, operator""sv is tokenized as operator,
      // the empty string literal, and the identifier suffix. Elsewhere an
      // adjacent identifier is part of the literal preprocessing-token and
      // posttokenization validates whether it is an allowed ud-suffix.
      size_t before = a;
      while (before > 0 &&
             (s[before - 1] == ' ' || s[before - 1] == '\t' ||
              s[before - 1] == '\n' || s[before - 1] == '\r' ||
              s[before - 1] == '\v' || s[before - 1] == '\f'))
        --before;
      size_t word_end = before;
      while (before > 0 && ident_cont(s[before - 1]))
        --before;
      bool literal_operator_name =
          utf8(s, before, word_end) == "operator" &&
          (before == 0 || !ident_cont(s[before - 1]));
      if (!literal_operator_name) {
        while (i < s.size()) {
          uint32_t cv = 0;
          size_t uj = ucn(i, cv);
          if (uj != i && ident_cont(cv)) {
            i = uj;
            continue;
          }
          if (uj == i && ident_cont(s[i])) {
            ++i;
            continue;
          }
          break;
        }
      }
      bool ud = i > after;
      string spelling = literal_spelling(a, quote, j - 1, i);
      if (ch) {
        if (ud)
          out.emit_user_defined_character_literal(spelling);
        else
          out.emit_character_literal(spelling);
      } else {
        if (ud)
          out.emit_user_defined_string_literal(spelling);
        else
          out.emit_string_literal(spelling);
      }
      line_had = true;
      emitted_any = true;
      bol = false;
      return true;
      bol = false;
      return true;
    }
    return false;
  }

  void scan() {
    if (s.size() >= 3 && s[0] == 0xfeff) {
      s[0] = ' ';
    }
    static const char *ops[] = {
        "%:%:",   "<<=",   ">>=",    "->*",    "...",   "##",     "<:",
        ":>",     "<%",    "%>",     "%:",     "::",    ".*",     "->",
        "++",     "--",    "<<",     ">>",     "<=",    ">=",     "==",
        "!=",     "&&",    "||",     "+=",     "-=",    "*=",     "/=",
        "%=",     "^=",    "&=",     "|=",     "new",   "delete", "and_eq",
        "not_eq", "or_eq", "xor_eq", "bitand", "bitor", "compl",  "and",
        "not",    "or",    "xor",    "or_eq",  0};
    while (i < s.size()) {
      if (s[i] == '\n') {
        out.emit_new_line();
        i++;
        bol = true;
        directive = false;
        directive_name_pending = false;
        include_next = false;
        line_had = false;
        emitted_any = true;
        continue;
      }
      if (s[i] == ' ' || s[i] == '\t' || s[i] == '\v' || s[i] == '\f' ||
          s[i] == '\r' ||
          (s[i] == '/' && i + 1 < s.size() &&
           (s[i + 1] == '/' || s[i + 1] == '*'))) {
        bool had = false;
        for (;;) {
          while (i < s.size() && s[i] != '\n' &&
                 (s[i] == ' ' || s[i] == '\t' || s[i] == '\v' || s[i] == '\f' ||
                  s[i] == '\r')) {
            i++;
            had = true;
          }
          if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
            had = true;
            i += 2;
            while (i < s.size() && s[i] != '\n')
              i++;
            break;
          }
          if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
            had = true;
            i += 2;
            bool closed = false;
            while (i < s.size()) {
              if (s[i] == '\n') {
                // Phase 3 replaces the comment with one space
                // but retains every newline inside it.
                if (had) {
                  out.emit_whitespace_sequence();
                  had = false;
                }
                out.emit_new_line();
                ++i;
                bol = true;
                directive = false;
                directive_name_pending = false;
                include_next = false;
                line_had = false;
                emitted_any = true;
                continue;
              }
              if (s[i] == '*' && i + 1 < s.size() && s[i + 1] == '/') {
                i += 2;
                closed = true;
                break;
              }
              ++i;
            }
            if (!closed)
              throw logic_error("unterminated block comment");
            continue;
          }
          break;
        }
        if (had)
          out.emit_whitespace_sequence();
        continue;
      }
      size_t a = i;
      if (scan_literal_or_header(a))
        continue;
      // Identifier (UCNs are decoded for identifier spelling).
      {
        uint32_t c = s[i];
        size_t uj = ucn(i, c);
        if (uj != i && !ident_start(c)) {
          string spelling;
          append_utf8(spelling, c);
          i = uj;
          out.emit_non_whitespace_char(spelling);
          bol = false;
          continue;
        }
        if (uj != i || ident_start(c)) {
          string spelling;
          size_t j = i;
          bool first = true;
          while (j < s.size()) {
            uint32_t x = s[j];
            size_t n = ucn(j, x);
            if (n == j && !(first ? ident_start(x) : ident_cont(x)))
              break;
            if (n != j && !(first ? ident_start(x) : ident_cont(x)))
              break;
            if (n == j) {
              append_utf8(spelling, x);
              j++;
            } else {
              append_utf8(spelling, x);
              j = n;
            }
            first = false;
          }
          if (j > i) {
            i = j;
            if (Digraph_IdentifierLike_Operators.count(spelling))
              out.emit_preprocessing_op_or_punc(spelling);
            else
              out.emit_identifier(spelling);
            line_had = true;
            if (directive_name_pending) {
              include_next = (spelling == "include");
              directive_name_pending = false;
            }
            bol = false;
            continue;
          }
        }
      }
      if (digit(s[i]) || (s[i] == '.' && i + 1 < s.size() && digit(s[i + 1]))) {
        size_t j = i + 1;
        while (j < s.size()) {
          if (digit(s[j]) || ident_cont(s[j]) || s[j] == '.') {
            j++;
            continue;
          }
          if ((s[j] == '+' || s[j] == '-') && j > i &&
              (s[j - 1] == 'e' || s[j - 1] == 'E')) {
            j++;
            continue;
          }
          break;
        }
        i = j;
        token(&IPPTokenStream::emit_pp_number, a, i);
        bol = false;
        continue;
      }
      bool matched = false;
      for (int k = 0; ops[k]; k++) {
        string op = ops[k];
        if (op == "<:" && starts(i, "<::") &&
            (i + 3 >= s.size() || (s[i + 3] != ':' && s[i + 3] != '>')))
          continue;
        if (starts(i, op)) {
          i += op.size();
          if (op == "<:" && i < s.size() && s[i] == ':') {
          }
          token(&IPPTokenStream::emit_preprocessing_op_or_punc, a, i);
          if (bol && (op == "#" || op == "%:")) {
            directive = true;
            directive_name_pending = true;
          }
          bol = false;
          matched = true;
          break;
        }
      }
      if (matched)
        continue;
      if (string("{}[]#();:?~!%^&|=<>+-*/.,").find(char(s[i])) !=
          string::npos) {
        i++;
        token(&IPPTokenStream::emit_preprocessing_op_or_punc, a, i);
        if (bol && s[a] == '#') {
          directive = true;
          directive_name_pending = true;
        }
        bol = false;
        continue;
      }
      if (s[i] == '\'' || s[i] == '"')
        throw logic_error("unterminated literal");
      ++i;
      token(&IPPTokenStream::emit_non_whitespace_char, a, i);
      bol = false;
    }
    out.emit_eof();
  }
};

} // namespace

// Shared phase 1--3 tokenization entrypoint for posttoken.
void ScanPreprocessingTokens(const string &input, IPPTokenStream &output) {
  vector<uint32_t> cps = decode_utf8(input);
  vector<uint32_t> translated = translate(cps);
  Scanner scanner(output, std::move(translated));
  scanner.scan();
}
