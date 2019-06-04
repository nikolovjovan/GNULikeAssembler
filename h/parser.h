#ifndef _PARSER_H
#define _PARSER_H

#include <string>
#include <map>

#define DIRECTIVE_CNT 13
#define INSTRUCTION_CNT 26

struct Content_Type { enum Enum { Directive, Instruction }; };
struct Directive_Code { enum Enum { Global, Extern, Equ, Set, Text, Data, Bss, Section, End, Byte, Word, Align, Skip }; };
struct Instruction_Code { enum Enum { Nop, Halt, Xchg, Int, Mov, Add, Sub, Mul, Div, Cmp, Not, And, Or, Xor, Test, Shl, Shr, Push, Pop, Jmp, Jeq, Jne, Jgt, Call, Ret, Iret }; };
struct Operand_Size { enum Enum { Byte, Word }; };

struct Directive
{
    Directive_Code::Enum code;
    std::string p1, p2, p3, expr;
};

struct Instruction
{
    Instruction_Code::Enum code;
    Operand_Size::Enum op_size;
    std::string op1, op2;
};

typedef union { Directive dir; Instruction instr; } Content;

struct Line
{
    std::string label;
    Content_Type::Enum type;
    Content content;
};

class Parser
{
public:
    Parser();
    ~Parser() {};

    bool parse_line(const std::string &, Line &);
    bool parse_content(const std::string &, Content &);
private:
    const std::string directive_strings[DIRECTIVE_CNT] = {
        "global", "extern", "equ", "set", "text", "data", "bss",
        "section", "end", "byte", "word", "align", "skip"
    };

    const std::string instruction_strings[INSTRUCTION_CNT] = {
        "nop", "halt", "xchg", "int", "mov", "add", "sub", "mul", "div",
        "cmp", "not", "and", "or", "xor", "test", "shl", "shr", "push",
        "pop", "jmp", "jeq", "jne", "jgt", "call", "ret", "iret"
    };

    std::map<std::string, uint8_t>  directive_map, instruction_map;
};

#endif // parser.h