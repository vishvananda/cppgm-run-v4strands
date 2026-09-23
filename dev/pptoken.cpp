#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

using namespace std;

#include "preprocess/tokens/IPPTokenStream.h"
#include "preprocess/tokens/DebugPPTokenStream.h"
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
int HexCharToValue(int c)
{
	switch (c)
	{
	case '0': return 0;
	case '1': return 1;
	case '2': return 2;
	case '3': return 3;
	case '4': return 4;
	case '5': return 5;
	case '6': return 6;
	case '7': return 7;
	case '8': return 8;
	case '9': return 9;
	case 'A': return 10;
	case 'a': return 10;
	case 'B': return 11;
	case 'b': return 11;
	case 'C': return 12;
	case 'c': return 12;
	case 'D': return 13;
	case 'd': return 13;
	case 'E': return 14;
	case 'e': return 14;
	case 'F': return 15;
	case 'f': return 15;
	default: throw logic_error("HexCharToValue of nonhex char");
	}
}

// See C++ standard 2.11 Identifiers and Appendix/Annex E.1
const vector<pair<int, int>> AnnexE1_Allowed_RangesSorted =
{
	{0xA8,0xA8},
	{0xAA,0xAA},
	{0xAD,0xAD},
	{0xAF,0xAF},
	{0xB2,0xB5},
	{0xB7,0xBA},
	{0xBC,0xBE},
	{0xC0,0xD6},
	{0xD8,0xF6},
	{0xF8,0xFF},
	{0x100,0x167F},
	{0x1681,0x180D},
	{0x180F,0x1FFF},
	{0x200B,0x200D},
	{0x202A,0x202E},
	{0x203F,0x2040},
	{0x2054,0x2054},
	{0x2060,0x206F},
	{0x2070,0x218F},
	{0x2460,0x24FF},
	{0x2776,0x2793},
	{0x2C00,0x2DFF},
	{0x2E80,0x2FFF},
	{0x3004,0x3007},
	{0x3021,0x302F},
	{0x3031,0x303F},
	{0x3040,0xD7FF},
	{0xF900,0xFD3D},
	{0xFD40,0xFDCF},
	{0xFDF0,0xFE44},
	{0xFE47,0xFFFD},
	{0x10000,0x1FFFD},
	{0x20000,0x2FFFD},
	{0x30000,0x3FFFD},
	{0x40000,0x4FFFD},
	{0x50000,0x5FFFD},
	{0x60000,0x6FFFD},
	{0x70000,0x7FFFD},
	{0x80000,0x8FFFD},
	{0x90000,0x9FFFD},
	{0xA0000,0xAFFFD},
	{0xB0000,0xBFFFD},
	{0xC0000,0xCFFFD},
	{0xD0000,0xDFFFD},
	{0xE0000,0xEFFFD}
};

// See C++ standard 2.11 Identifiers and Appendix/Annex E.2
const vector<pair<int, int>> AnnexE2_DisallowedInitially_RangesSorted =
{
	{0x300,0x36F},
	{0x1DC0,0x1DFF},
	{0x20D0,0x20FF},
	{0xFE20,0xFE2F}
};

// See C++ standard 2.13 Operators and punctuators
const unordered_set<string> Digraph_IdentifierLike_Operators =
{
	"new", "delete", "and", "and_eq", "bitand",
	"bitor", "compl", "not", "not_eq", "or",
	"or_eq", "xor", "xor_eq"
};

// See `simple-escape-sequence` grammar
const unordered_set<int> SimpleEscapeSequence_CodePoints =
{
	'\'', '"', '?', '\\', 'a', 'b', 'f', 'n', 'r', 't', 'v'
};

// PA1 tokenizer. Translation and token emission are kept in one pass-oriented
// component; later front-end phases consume the same IPPTokenStream contract.
namespace {

bool in_ranges(uint32_t c, const vector<pair<int,int> >& ranges)
{
    size_t lo=0, hi=ranges.size();
    while(lo<hi) { size_t m=lo+(hi-lo)/2; if(c<static_cast<uint32_t>(ranges[m].first)) hi=m; else if(c>static_cast<uint32_t>(ranges[m].second)) lo=m+1; else return true; }
    return false;
}
bool digit(uint32_t c) { return c>='0'&&c<='9'; }
bool hex_digit(uint32_t c) { return digit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F'); }
bool ident_start(uint32_t c) { return c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z')|| (in_ranges(c,AnnexE1_Allowed_RangesSorted)&&!in_ranges(c,AnnexE2_DisallowedInitially_RangesSorted)); }
bool ident_cont(uint32_t c) { return ident_start(c)||digit(c)||in_ranges(c,AnnexE2_DisallowedInitially_RangesSorted); }
void append_utf8(string& out, uint32_t c) {
    if(c<=0x7f) out+=char(c);
    else if(c<=0x7ff) {out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
    else if(c<=0xffff) {out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    else {out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
}
vector<uint32_t> decode_utf8(const string& s) {
    vector<uint32_t> v;
    for(size_t i=0;i<s.size();) {
        unsigned char b=s[i++]; uint32_t c; unsigned n;
        if(b<0x80){c=b;n=0;} else if(b>=0xc2&&b<=0xdf){c=b&31;n=1;} else if(b>=0xe0&&b<=0xef){c=b&15;n=2;} else if(b>=0xf0&&b<=0xf4){c=b&7;n=3;} else throw logic_error("invalid UTF-8 leading byte");
        if(i+n>s.size()) throw logic_error("invalid UTF-8 continuation byte");
        for(unsigned k=0;k<n;k++){unsigned char x=s[i++]; if((x&0xc0)!=0x80) throw logic_error("invalid UTF-8 continuation byte"); c=(c<<6)|(x&63);}
        if((n==1&&c<0x80)||(n==2&&c<0x800)||(n==3&&c<0x10000)||c>0x10ffff||(c>=0xd800&&c<=0xdfff)) throw logic_error("invalid UTF-8 code point");
        v.push_back(c);
    }
    return v;
}
string utf8(const vector<uint32_t>& v,size_t a,size_t b) { string s; for(size_t i=a;i<b;i++) append_utf8(s,v[i]); return s; }
uint32_t tri(uint32_t c) {
    switch(c){case '=':return '#';case '/':return '\\';case '\'':return '^';case '(':return '[';case ')':return ']';case '!':return '|';case '<':return '{';case '>':return '}';case '-':return '~';default:return 0;}
}
// Translate trigraphs/splices; preserve raw-string bodies, whose phase-1/2
// transformations are reverted by the C++11 raw-string rule.
vector<uint32_t> translate(const vector<uint32_t>& in) {
    vector<uint32_t> a;
    for(size_t i=0;i<in.size();) {
        // Phase 1/2 transformations inside any C++11 raw-string spelling are
        // reverted. Recognize every encoding prefix, not only the bare R form.
        size_t quote=i;
        if(i+3<in.size()&&in[i]=='u'&&in[i+1]=='8'&&in[i+2]=='R'&&in[i+3]=='"') quote=i+3;
        else if(i+2<in.size()&&(in[i]=='u'||in[i]=='U'||in[i]=='L')&&in[i+1]=='R'&&in[i+2]=='"') quote=i+2;
        else if(i+1<in.size()&&in[i]=='R'&&in[i+1]=='"') quote=i+1;
        if(quote!=i) {
            size_t op=quote+1; while(op<in.size()&&in[op]!='('&&in[op]!='\n'&&op-quote<=17) op++;
            if(op<in.size()&&in[op]=='('&&op-quote-1<=16) {
                string delim=utf8(in,quote+1,op); string close=")"+delim+"\""; size_t e=op+1;
                for(;e+close.size()<=in.size();e++) if(utf8(in,e,e+close.size())==close) {e+=close.size();break;}
                if(e<=in.size()&&e>op+1&&utf8(in,e-close.size(),e)==close) {a.insert(a.end(),in.begin()+i,in.begin()+e);i=e;continue;}
            }
        }
        if(i+2<in.size()&&in[i]=='?'&&in[i+1]=='?'&&tri(in[i+2])) {a.push_back(tri(in[i+2]));i+=3;}
        else a.push_back(in[i++]);
    }
    vector<uint32_t> b;
    for(size_t i=0;i<a.size();i++) { if(a[i]=='\\'&&i+1<a.size()&&a[i+1]=='\n'){i++;continue;} b.push_back(a[i]); }
    return b;
}

struct Scanner {
    IPPTokenStream& out; vector<uint32_t> s; size_t i; bool bol; bool directive; bool directive_name_pending; bool include_next; bool emitted_any; bool line_had;
    Scanner(IPPTokenStream& o,const vector<uint32_t>& x):out(o),s(x),i(0),bol(true),directive(false),directive_name_pending(false),include_next(false),emitted_any(false),line_had(false){}
    void token(void(IPPTokenStream::*fn)(const string&),size_t a,size_t b) { (out.*fn)(utf8(s,a,b)); emitted_any=true; line_had=true; }
    bool starts(size_t p,const string& x) { vector<uint32_t> q=decode_utf8(x); if(p+q.size()>s.size())return false; for(size_t k=0;k<q.size();k++)if(s[p+k]!=q[k])return false;return true; }
    size_t ucn(size_t p, uint32_t& c) {
        if (p + 2 > s.size() || s[p] != '\\' ||
            (s[p + 1] != 'u' && s[p + 1] != 'U')) return p;
        const size_t digits = s[p + 1] == 'u' ? 4 : 8;
        const size_t first = p + 2;
        if (first + digits > s.size()) return p;
        uint32_t value = 0;
        for (size_t k = 0; k < digits; ++k) {
            if (!hex_digit(s[first + k])) return p;
            value = (value << 4) | HexCharToValue(s[first + k]);
        }
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ||
            (value < 0xa0 && value != '$' && value != '@' && value != '`'))
            throw logic_error("invalid universal character value");
        c = value;
        return first + digits;
    }
    string literal_spelling(size_t begin, size_t quote, size_t close, size_t end) {
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
    void scan() {
        // A nonempty source lacking LF receives the phase-2 terminating newline.
        if(!s.empty()&&s.back()!='\n') s.push_back('\n');
        if(s.size()>=3&&s[0]==0xfeff){s[0]=' ';}
        static const char* ops[]={"%:%:","<<=",">>=","->*","...","##","<:",":>","<%","%>","%:","::",".*","->","++","--","<<",">>","<=",">=","==","!=","&&","||","+=","-=","*=","/=","%=","^=","&=","|=","new","delete","and_eq","not_eq","or_eq","xor_eq","bitand","bitor","compl","and","not","or","xor","or_eq",0};
        while(i<s.size()) {
            if(s[i]=='\n'){out.emit_new_line();i++;bol=true;directive=false;directive_name_pending=false;include_next=false;line_had=false;emitted_any=true;continue;}
            if(s[i]==' '||s[i]=='\t'||s[i]=='\v'||s[i]=='\f'||s[i]=='\r'||(s[i]=='/'&&i+1<s.size()&&(s[i+1]=='/'||s[i+1]=='*'))){
                bool had=false;
                for(;;){while(i<s.size()&&s[i]!='\n'&&(s[i]==' '||s[i]=='\t'||s[i]=='\v'||s[i]=='\f'||s[i]=='\r')){i++;had=true;}
                    if(i+1<s.size()&&s[i]=='/'&&s[i+1]=='/') {had=true;i+=2;while(i<s.size()&&s[i]!='\n')i++;break;}
                    if(i+1<s.size()&&s[i]=='/'&&s[i+1]=='*'){had=true;i+=2;bool closed=false;while(i<s.size()){if(s[i]=='\n'){i++;continue;}if(s[i]=='*'&&i+1<s.size()&&s[i+1]=='/'){i+=2;closed=true;break;}i++;}if(!closed)throw logic_error("unterminated block comment");continue;}break;
                }
                if(had)out.emit_whitespace_sequence(); continue;
            }
            size_t a=i;
            // Header-name is enabled only after #include at logical line start.
            if(include_next&&(s[i]=='<'||s[i]=='"')) {uint32_t q=s[i]=='<'?'>':'"';size_t j=i+1;while(j<s.size()&&s[j]!='\n'&&s[j]!=q)j++;if(j<s.size()&&s[j]==q){i=j+1;token(&IPPTokenStream::emit_header_name,a,i);include_next=false;bol=false;continue;}}
            // String/character literals, with standard encoding prefixes.
            size_t quote=i; bool raw=false; string rawprefix;
            if(starts(i,"u8R\"")||starts(i,"uR\"")||starts(i,"UR\"")||starts(i,"LR\"")||starts(i,"R\"")){raw=true;quote=i+(starts(i,"u8R\"")?3:starts(i,"R\"")?1:2);}
            else if(starts(i,"u8\""))quote=i+2;
            else if((s[i]=='u'||s[i]=='U'||s[i]=='L')&&i+1<s.size()&&(s[i+1]=='\''||s[i+1]=='\"'))quote=i+1;
            if(raw){size_t op=quote+1;while(op<s.size()&&s[op]!='('&&s[op]!='\n'){if(s[op]==' '||s[op]=='\\'||s[op]==')'||s[op]=='\t'||s[op]=='\v'||s[op]=='\f'||op-quote>16)throw logic_error("raw string delimiter is too long");op++;}if(op>=s.size()||s[op]!='(')throw logic_error("unterminated raw string literal");string d=utf8(s,quote+1,op), close=")"+d+"\"";size_t j=op+1;while(j+close.size()<=s.size()&&utf8(s,j,j+close.size())!=close)j++;if(j+close.size()>s.size())throw logic_error("unterminated raw string literal");i=j+close.size();size_t after=i;while(i<s.size()&&ident_cont(s[i]))i++;token(i>after?&IPPTokenStream::emit_user_defined_string_literal:&IPPTokenStream::emit_string_literal,a,i);bol=false;continue;}
            if(s[quote]=='\''||s[quote]=='\"'){
                uint32_t q=s[quote]; bool ch=q=='\'';size_t j=quote+1;bool closed=false;
                while(j<s.size()&&s[j]!='\n'){if(s[j]==q){j++;closed=true;break;}if(s[j]=='\\'){if(j+1>=s.size()||s[j+1]=='\n')break;uint32_t e=s[j+1];if(SimpleEscapeSequence_CodePoints.count(e)){j+=2;continue;}if(e>='0'&&e<='7'){size_t k=j+1;while(k<s.size()&&k<j+4&&s[k]>='0'&&s[k]<='7')k++;j=k;continue;}if(e=='x'){size_t k=j+2;while(k<s.size()&&hex_digit(s[k]))k++;if(k==j+2)throw logic_error("hex escape has no digits");j=k;continue;}uint32_t escaped;size_t un=ucn(j,escaped);if(un!=j){j=un;continue;}throw logic_error("invalid escape sequence");}uint32_t cv;size_t uj=ucn(j,cv);j=uj==j?j+1:uj;}
                if(!closed)throw logic_error("unterminated quoted literal");
                i=j;
                size_t after=i;
                while(i<s.size()){
                    uint32_t cv; size_t uj=ucn(i,cv);
                    if(uj!=i){i=uj;continue;}
                    if(!ident_cont(s[i]))break;
                    i++;
                }
                bool ud=i>after;
                string spelling=literal_spelling(a,quote,j-1,i);
                if(ch){ if(ud)out.emit_user_defined_character_literal(spelling);else out.emit_character_literal(spelling); }
                else { if(ud)out.emit_user_defined_string_literal(spelling);else out.emit_string_literal(spelling); }
                line_had=true;
                emitted_any=true;
                bol=false;
                continue;bol=false;continue;
            }
            // Identifier (UCNs are decoded for identifier spelling).
            {uint32_t c=s[i];size_t uj=ucn(i,c);if(uj!=i&&!ident_start(c)){string spelling;append_utf8(spelling,c);i=uj;out.emit_non_whitespace_char(spelling);bol=false;continue;}if(uj!=i||ident_start(c)){string spelling;size_t j=i;bool first=true;while(j<s.size()){uint32_t x=s[j];size_t n=ucn(j,x);if(n==j&&! (first?ident_start(x):ident_cont(x)))break;if(n!=j&&!(first?ident_start(x):ident_cont(x)))break;if(n==j){append_utf8(spelling,x);j++;}else{append_utf8(spelling,x);j=n;}first=false;}if(j>i){i=j;if(Digraph_IdentifierLike_Operators.count(spelling))out.emit_preprocessing_op_or_punc(spelling);else out.emit_identifier(spelling);line_had=true;if(directive_name_pending){include_next=(spelling=="include");directive_name_pending=false;}bol=false;continue;}}
            }
            if(digit(s[i])||(s[i]=='.'&&i+1<s.size()&&digit(s[i+1]))){size_t j=i+1;while(j<s.size()){if(digit(s[j])||ident_cont(s[j])||s[j]=='.'){j++;continue;}if((s[j]=='+'||s[j]=='-')&&j>i&&(s[j-1]=='e'||s[j-1]=='E')){j++;continue;}break;}i=j;token(&IPPTokenStream::emit_pp_number,a,i);bol=false;continue;}
            bool matched=false;for(int k=0;ops[k];k++){string op=ops[k];if(op=="<:"&&starts(i,"<::")&&(i+3>=s.size()||(s[i+3]!=':'&&s[i+3]!='>')))continue;if(starts(i,op)){i+=op.size();if(op=="<:"&&i<s.size()&&s[i]==':'){}token(&IPPTokenStream::emit_preprocessing_op_or_punc,a,i);if(bol&&(op=="#"||op=="%:")){directive=true;directive_name_pending=true;}bol=false;matched=true;break;}}if(matched)continue;
            if(string("{}[]#();:?~!%^&|=<>+-*/.,").find(char(s[i]))!=string::npos){i++;token(&IPPTokenStream::emit_preprocessing_op_or_punc,a,i);if(bol&&s[a]=='#'){directive=true;directive_name_pending=true;}bol=false;continue;}
            if(s[i]=='\''||s[i]=='"')throw logic_error("unterminated literal");i++;token(&IPPTokenStream::emit_non_whitespace_char,a,i);bol=false;
        }
        out.emit_eof();
    }
};

} // namespace

bool HasBatchStdinArg(int argc, char** argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (string(argv[i]) == "--batch-stdin")
			return true;
	}
	return false;
}

int RunBatchMode()
{
    string record;
    while (getline(cin, record)) {
        if(record.empty()) continue;
        vector<string> fields; size_t at=0; for(;;){size_t t=record.find('\t',at);fields.push_back(record.substr(at,t==string::npos?t:t-at));if(t==string::npos)break;at=t+1;}
        if(fields.size()!=3){cout<<"EXIT_FAILURE\n";continue;}
        ifstream in(fields[2].c_str(),ios::binary); if(!in){ofstream e(fields[1].c_str());e<<"ERROR: cannot read input\n";cout<<"EXIT_FAILURE\n";continue;}
        ostringstream data;data<<in.rdbuf(); ofstream outFile(fields[0].c_str(),ios::binary); streambuf* oldOut=cout.rdbuf(outFile.rdbuf());streambuf* oldErr=cerr.rdbuf(outFile.rdbuf()); int status=0;
        try{DebugPPTokenStream sink;vector<uint32_t> cps=decode_utf8(data.str());vector<uint32_t> translated=translate(cps);Scanner sc(sink,translated);sc.scan();}catch(exception& e){cerr<<"ERROR: "<<e.what()<<endl;status=1;}
        cout.rdbuf(oldOut);cerr.rdbuf(oldErr);cout<<(status?"EXIT_FAILURE":"EXIT_SUCCESS")<<endl;
    }return 0;
}

int main(int argc, char** argv)
{
	try
	{
		if (HasBatchStdinArg(argc, argv))
			return RunBatchMode();

		ostringstream oss;
		oss << cin.rdbuf();

		string input = oss.str();

		DebugPPTokenStream output;
        vector<uint32_t> cps=decode_utf8(input);
        vector<uint32_t> translated=translate(cps);
        Scanner scanner(output,translated);
        scanner.scan();

		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
