#ifndef _PARSER_H
#define _PARSER_H

#include "lexer.h"

#include <map>
#include <string>
#include <vector>

#define DIR_CNT 13
#define INSTR_CNT 26
#define PSEUDO_CNT 2

struct Content_Type { enum { None = 0, Directive, Instruction }; };
struct Operand_Size { enum { None = 0, Byte, Word }; };

typedef struct Directive
{
    enum { Global = 0, Extern, Equ, Set, Text, Data, Bss, Section, End, Byte, Word, Align, Skip };
    uint8_t code;
    std::string p1, p2, p3;
} Directive;

typedef struct Instruction
{
    enum { Nop = 0, Halt, Xchg, Int, Mov, Add, Sub, Mul, Div, Cmp, Not, And, Or, Xor, Test, Shl, Shr, Push, Pop, Jmp, Jeq, Jne, Jgt, Call, Ret, Iret };
    uint8_t code;
    uint8_t op_size;
    uint8_t op_cnt;
    std::string op1, op2;
} Instruction;

class Expression_Token
{
public:
    enum Token_Type { OpenBracket, CloseBracket, Plus, Minus, Multiply, Divide, Number, Symbol };
    Token_Type type;
};

class Operator_Token : Expression_Token
{
public:
    unsigned calculate(unsigned a, unsigned b)
    {
        switch (type)
        {
        case Plus:
            return a + b;
        case Minus:
            return a - b;
        case Multiply:
            return a * b;
        case Divide:
            return a / b;
        default:
            return 0;
        };
    }
};

class Number_Token : Expression_Token
{
public:
    unsigned value;
};

class Symbol_Token : Expression_Token
{
public:
    std::string name;
};

typedef std::vector<Expression_Token> Expression;

class Parser;

class Line
{
public:
    std::string label;
    uint8_t content_type;

    Line();
    Line(const Line &);
    ~Line();

    Directive& getDir();
    Instruction& getInstr();
protected:
    friend class Parser;

    void freeDir();
    void freeInstr();
private:
    Directive *dir;
    Instruction *instr;
};

class Parser
{
public:
    Parser(Lexer *lexer);
    ~Parser() {};

    std::string get_directive(uint8_t code) const;
    std::string get_instruction(uint8_t code) const;

    bool parse_line(const std::string &str, Line &result);
    bool parse_directive(const std::string &str, Directive &result);
    bool parse_instruction(const std::string &str, Instruction &result);
    bool parse_expression(const std::string &str, Expression &result);

    bool decode_byte(const std::string &str, uint8_t &result);
    bool decode_word(const std::string &str, uint16_t &result);
    bool decode_register(const std::string &str, uint8_t &regdesc);

    uint8_t get_operand_size(const std::string &str);
private:
    Lexer *lexer;

    const std::string dir_str[DIR_CNT] = {
        "global", "extern", "equ", "set", "text", "data", "bss",
        "section", "end", "byte", "word", "align", "skip"
    };

    const std::string instr_str[INSTR_CNT] = {
        "nop", "halt", "xchg", "int", "mov", "add", "sub", "mul", "div",
        "cmp", "not", "and", "or", "xor", "test", "shl", "shr", "push",
        "pop", "jmp", "jeq", "jne", "jgt", "call", "ret", "iret"
    };

    const std::string pseudo_str[PSEUDO_CNT] = {
        "pushf", "popf"
    };

    std::map<std::string, uint8_t>  dir_map, instr_map, pseudo_map;
};

#endif // parser.h