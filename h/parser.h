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
    enum Token_Type { Operator, Number, Symbol };
    const Token_Type type;
    Expression_Token(const Expression_Token &t);
    Expression_Token(Token_Type type);
    virtual ~Expression_Token() {}
};

class Operator_Token : public Expression_Token
{
public:
    enum Operator_Type { Open, Close, Add, Sub, Mul, Div, Mod, And, Or, Xor };
    const Operator_Type op_type;

    Operator_Token(const Operator_Token &t);
    Operator_Token(Operator_Type op_type);

    char get_symbol();
    int priority();
    int calculate(unsigned a, unsigned b);
    int get_st_shndx(int shndx_a, int shndx_b);
    int get_clidx(int clidx_a, int clidx_b);
};

class Number_Token : public Expression_Token
{
public:
    const int value;
    Number_Token(const Number_Token &t);
    Number_Token(int value);
};

class Symbol_Token : public Expression_Token
{
public:
    std::string name;
    Symbol_Token(const Symbol_Token &t);
    Symbol_Token(const std::string &name);
};

typedef std::vector<std::unique_ptr<Expression_Token>> Expression;

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

    int decode_number(const std::string &str);
    bool decode_byte(const std::string &str, uint8_t &result);
    bool decode_word(const std::string &str, uint16_t &result);
    bool decode_register(const std::string &str, uint8_t &regdesc);
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