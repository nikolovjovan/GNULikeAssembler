#include "parser.h"

using std::string;
using std::vector;

Line::Line() : label(""), content_type(Content_Type::None), dir(nullptr), instr(nullptr) {}


Line::Line(const Line &l)
{
    label = l.label;
    content_type = l.content_type;
    dir = l.dir != nullptr ? new Directive(*(l.dir)) : nullptr;
    instr = l.instr != nullptr ? new Instruction(*(l.instr)) : nullptr;
}

Line::~Line()
{
    freeDir();
    freeInstr();
}

Directive& Line::getDir()
{
    if (dir == nullptr)
        dir = new Directive();
    return *dir;
}

Instruction& Line::getInstr()
{
    if (instr == nullptr)
        instr = new Instruction();
    return *instr;
}

void Line::freeDir()
{
    if (dir != nullptr)
        delete dir;
    dir = nullptr;
}

void Line::freeInstr()
{
    if (instr != nullptr)
        delete instr;
    instr = nullptr;
}

Parser::Parser(Lexer *lexer)
{
    this->lexer = lexer;

    // Initializing directive map
    for (uint8_t i = 0; i < DIR_CNT; ++i)
        dir_map[dir_str[i]] = i;
    // Initializing instructions map
    for (uint8_t i = 0; i < INSTR_CNT; ++i)
        instr_map[instr_str[i]] = i;
    // Initializing pseudo-instructions map
    pseudo_map[pseudo_str[0]] = Instruction::Push; // pushf
    pseudo_map[pseudo_str[1]] = Instruction::Pop; // popf
}

string Parser::get_directive(uint8_t code) const
{
    if (code >= DIR_CNT) return "";
    return dir_str[code];
}

string Parser::get_instruction(uint8_t code) const
{
    if (code >= INSTR_CNT) return "";
    return instr_str[code];
}

bool Parser::parse_line(const string &str, Line &result)
{
    result.label = "";
    result.content_type = Content_Type::None;
    if (lexer->is_empty(str)) return true; // empty line

    tokens_t tokens;
    if (!lexer->tokenize_line(str, tokens)) return false;
    if (tokens.size() < 2) return false; // should never happen!

    result.label = tokens[0];
    if (lexer->is_empty(tokens[1])) return true;
    if (parse_directive(tokens[1], result.getDir()))
    {
        result.content_type = Content_Type::Directive;
        return true;
    }
    result.freeDir();
    if (parse_instruction(tokens[1], result.getInstr()))
    {
        result.content_type = Content_Type::Instruction;
        return true;
    }
    result.freeInstr();
    return false; // invalid content
}

bool Parser::parse_directive(const string &str, Directive &result)
{
    result.code = -1;
    result.p1 = "";
    result.p2 = "";
    result.p3 = "";

    tokens_t tokens;
    if (!lexer->tokenize_directive(str, tokens)) return false;
    if (tokens.size() < 1) return false; // should never happen!

    result.code = dir_map[tokens[0]];
    if (tokens.size() > 1) result.p1 = tokens[1];
    if (tokens.size() > 2) result.p2 = tokens[2];
    if (tokens.size() > 3) result.p3 = tokens[3];

    return true;
}

bool Parser::parse_instruction(const string &str, Instruction &result)
{
    result.code = -1;
    result.op_cnt = 0;
    result.op_size = 0;
    result.op1 = "";
    result.op2 = "";

    tokens_t tokens;
    if (!lexer->tokenize_zeroaddr(str, tokens))
    if (!lexer->tokenize_oneaddr(str, tokens))
    if (!lexer->tokenize_twoaddr(str, tokens)) return false;
    if (tokens.size() < 1) return false; // should never happen!

    string mnem = Lexer::tolower(tokens[0]);

    if (pseudo_map.count(mnem) > 0)
    {
        result.code = pseudo_map[mnem];
        switch (result.code)
        {
        case Instruction::Push:
        case Instruction::Pop:
            result.op_cnt = 1;
            result.op_size = Operand_Size::Word;
            result.op1 = "psw"; // pushf and popf have a single operand - psw
            return true;
        default:
            return false; // should never happen!
        }
    }

    result.code = instr_map[mnem];

    if (tokens.size() == 1) return true; // zero-addr instruction (1 token)
    if (tokens.size() < 3) return false;

    string op_size = Lexer::tolower(tokens[1]);
    if (op_size.empty()) op_size = "w";
    result.op_size = op_size[0] == 'b' ? Operand_Size::Byte : Operand_Size::Word;

    result.op_cnt++; // could be either one-addr or two-addr instruction
    result.op1 = tokens[2];

    if (tokens.size() == 3) return true; // one-addr instruction (3 tokens)
    if (tokens.size() > 4) return false;

    result.op_cnt++;
    result.op2 = tokens[3];

    return true; // two-addr instruction (4 tokens)
}

bool Parser::decode_byte(const string &str, uint8_t &byte)
{
    byte = 0;
    if (str == "") return true;
    string value;
    if (!lexer->match_byte(str, value)) return false;
    bool inv = value[0] == '~', neg = value[0] == '-';
    int first = inv || neg;
    unsigned long long temp;
    if (value[first] != '0') temp = stoul(value.substr(first), 0, 10);
    else if (value.length() == first + 1) temp = 0; // 0 || ~0 || -0
    else if (value[first + 1] == 'b') temp = stoul(value.substr(first + 2), 0, 2);
    else if (value[first + 1] != 'x') temp = stoul(value.substr(first + 1), 0, 8);
    else temp = stoul(value.substr(first + 2), 0, 16);
    if (temp >= 0 && temp <= 0xff)
    {
        byte = temp;
        if (inv) byte = ~byte;
        else if (neg) byte = -byte;
        return true;
    }
    return false;
}

bool Parser::decode_word(const string &str, uint16_t &word)
{
    word = 0;
    if (str == "") return true;
    string value;
    if (!lexer->match_word(str, value)) return false;
    bool inv = value[0] == '~', neg = value[0] == '-';
    int first = inv || neg;
    unsigned long temp;
    if (value[first] != '0') temp = stoul(value.substr(first), 0, 10);
    else if (value.length() == first + 1) temp = 0; // 0 || ~0 || -0
    else if (value[first + 1] == 'b') temp = stoul(value.substr(first + 2), 0, 2);
    else if (value[first + 1] != 'x') temp = stoul(value.substr(first + 1), 0, 8);
    else temp = stoul(value.substr(first + 2), 0, 16);
    if (temp >= 0 && temp <= 0xffff)
    {
        word = temp;
        if (inv) word = ~word;
        else if (neg) word = -word;
        return true;
    }
    return false;
}

bool Parser::decode_register(const string &str, uint8_t &regdesc)
{
    regdesc = 0;
    if (str[0] == 'r') regdesc |= (str[1] - '0') << 1;
    else if (str == "sp") regdesc |= 6 << 1;
    else if (str == "pc") regdesc |= 7 << 1;
    else return false;
    return true;
}

uint8_t Parser::get_operand_size(const string &str)
{
    if (lexer->match_byte_operand(str)) return 1;
    string offset_str;
    if (!lexer->match_word_operand(str, offset_str)) return 3;
    uint8_t offset;
    if (decode_byte(offset_str, offset))
    {
        if (offset == 0) return 1; // if offset is zero assume regind without offset
        return 2;
    }
    return 3;
}