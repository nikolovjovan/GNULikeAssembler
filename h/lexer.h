#ifndef _LEXER_H
#define _LEXER_H

#include <vector>
#include <string>
#include <regex>

// *** Regex strings that match specific tokens ***

// Line START: matches all whitespace BEFORE any valid symbols
#define REGEX_START "^\\s*"
// Line END: matches all whitespace AFTER valid symbols and comments
#define REGEX_END "\\s*(?:#.*)?$"

// Valid symbol format: starts with '.' or '_' or 'a-z' then can also contain digits
#define REGEX_SYM "[._a-z][.\\w]*"

// Valid byte value format: binary ex. 0b[0|1]+; octal ex. 0[0-7]+; decimal ex. 0|[1-9][0-9]*; hexadecimal ex. 0x[0-9a-f]+
#define REGEX_VAL_B "[-~]?(?:0b[0-1]{1,8}|0[0-7]{1,3}|0|[1-9]\\d{0,2}|0x[\\da-f]{1,2})"

// Valid word value format: binary ex. 0b[0|1]+; octal ex. 0[0-7]+; decimal ex. [1-9][0-9]*; hexadecimal ex. 0x[0-9a-f]+
#define REGEX_VAL_W "[-~]?(?:0b[0-1]{1,16}|0[0-7]{1,6}|0|[1-9]\\d{0,4}|0x[\\da-f]{1,4})"

// Valid content format: anything until a comment
#define REGEX_CONTENT "[^#]*?"

// *** Immediate addressing modes ***
// <val> (immediate value embedded inside the instruction)
#define REGEX_ADR_IMM_B REGEX_VAL_B
// <val> or &<symbol_name> (immediate value embedded inside the instruction)
#define REGEX_ADR_IMM_W REGEX_VAL_W "|&" REGEX_SYM

// *** Register addressing modes ***
// r<num>h or r<num>l (register high or low addressing)
#define REGEX_ADR_REGDIR_B "r[0-7][hl]"
// r<num> or sp or pc (psw can be only addressed in push/pop <=> pushf/popf) (full register addressing)
#define REGEX_ADR_REGDIR_W "r[0-7]|sp|pc"

// *** Memory addressing modes ***
// [r<num>] or r<num>[<val>] or r<num>[<symbol_name>]
#define REGEX_ADR_REGIND "\\[\\s*(?:" REGEX_ADR_REGDIR_W ")\\s*\\]|(?:" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(?:" REGEX_VAL_W "|" REGEX_SYM ")\\s*\\]"
// *<val> (absolute data addressing)
#define REGEX_ADR_ABS "\\*" REGEX_VAL_W
// <symbol_name> or $<symbol_name> (absolute or PC-relative symbol addressing)
#define REGEX_ADR_SYM "\\$?" REGEX_SYM
// all memory addressing modes for both byte and word sized operands
#define REGEX_ADR_MEM REGEX_ADR_REGIND "|" REGEX_ADR_ABS "|" REGEX_ADR_SYM

// *** Combinational addressing modes ***
// immediate and register direct addressing modes for byte sized operands
#define REGEX_ADR_IMMREG_B REGEX_ADR_IMM_B "|" REGEX_ADR_REGDIR_B
// immediate and register direct addressing modes for word sized operands
#define REGEX_ADR_IMMREG_W REGEX_ADR_IMM_W "|" REGEX_ADR_REGDIR_W
// register and memory addressing modes for byte sized operands
#define REGEX_ADR_REGMEM_B REGEX_ADR_REGDIR_B "|" REGEX_ADR_MEM
// register and memory addressing modes for word sized operands
#define REGEX_ADR_REGMEM_W REGEX_ADR_REGDIR_W "|" REGEX_ADR_MEM

#define CONTENT_CNT 4

struct Content_Match { enum { Directive, ZeroAddr, OneAddr, TwoAddr }; };

class Lexer
{
public:
    Lexer();
    ~Lexer() {};

    bool tokenize_line(const std::string &, std::vector<std::string> &);
    bool tokenize_content(const std::string &, std::vector<std::string> &);

private:
    std::regex line_regex;
    const std::string line_string = REGEX_START "(?:(" REGEX_SYM "):)?\\s*(?:(" REGEX_CONTENT "))?" REGEX_END;

    std::regex regex_split, regex_symbol, regex_byte, regex_word, regex_op1b, regex_op2b,
        regex_imm_b, regex_imm_w, regex_regdir_b, regex_regdir_w, regex_regind,
        regex_regindoff, regex_regindsym, regex_memsym, regex_memabs;

    const std::string regex_split_string = "\\s*,",
                      regex_symbol_string = "\\s*(" REGEX_SYM ")\\s*",
                      regex_byte_string = "\\s*(" REGEX_VAL_B ")\\s*",
                      regex_word_string = "\\s*(" REGEX_VAL_W ")\\s*",
                      regex_op1b_string = "\\s*(" REGEX_ADR_REGDIR_B "|" REGEX_ADR_REGDIR_W "|\\[\\s*(?:" REGEX_ADR_REGDIR_W ")\\s*\\])\\s*",
                      regex_op2b_string = "\\s*(?:(" REGEX_ADR_IMM_B ")|(?:" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(" REGEX_ADR_IMM_B ")\\s*\\])\\s*";

    const std::string content_strings[CONTENT_CNT] = {
        REGEX_START // Directives
        "\\.(?:"
        "(section)\\s+(" REGEX_SYM ")\\s*(?:,\\s*\"(a?e?w?x?)\")?|" // flags: a-allocatable, e-excluded from executable and shared library (bss), w-writable, x-executable
        "(text|data|bss|end)|"
        "(global|extern|byte|word)\\s+(" REGEX_CONTENT ")|"
        "(equ|set)\\s+(" REGEX_SYM "),\\s*(" REGEX_CONTENT ")|"
        "(align)\\s+(" REGEX_VAL_B ")\\s*(?:,\\s*(" REGEX_VAL_B "))?\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        "(skip)\\s+(" REGEX_VAL_W ")\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        ")" REGEX_END, // Directives

        REGEX_START "(nop|halt|ret|iret)" REGEX_END, // 0-address instructions

        REGEX_START // 1-address instructions
        "(?:"
        "(int)()\\s+(" REGEX_ADR_IMM_B ")|"

        "(not)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(not)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(pushf)()|" // pushf <=> push psw (push psw is not allowed but it is coded)
        "(popf)()|"  // popf <=> pop psw (pop psw is not allowed but it is coded)

        "(push)(b)\\s+(" REGEX_ADR_IMMREG_B "|" REGEX_ADR_MEM ")|"
        "(push)(w?)\\s+(" REGEX_ADR_IMMREG_W "|" REGEX_ADR_MEM ")|"
        "(pop)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(pop)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(jmp|jeq|jne|jgt|call)()\\s+(" REGEX_ADR_MEM ")"
        ")" REGEX_END,

        REGEX_START // 2-address instructions
        "(?:"
        "(xchg)(b)\\s+(" REGEX_ADR_REGMEM_B ")\\s*,\\s*(" REGEX_ADR_REGDIR_B ")|"
        "(xchg)(b)\\s+(" REGEX_ADR_REGDIR_B ")\\s*,\\s*(" REGEX_ADR_REGMEM_B ")|"
        "(xchg)(w?)\\s+(" REGEX_ADR_REGMEM_W ")\\s*,\\s*(" REGEX_ADR_REGDIR_W ")|"
        "(xchg)(w?)\\s+(" REGEX_ADR_REGDIR_W ")\\s*,\\s*(" REGEX_ADR_REGMEM_W ")|"

        "(mov|add|sub|mul|div|cmp|and|or|xor|test)(b)\\s+(" REGEX_ADR_REGMEM_B ")\\s*,\\s*(" REGEX_ADR_IMMREG_B ")|"
        "(mov|add|sub|mul|div|cmp|and|or|xor|test)(b)\\s+(" REGEX_ADR_REGDIR_B ")\\s*,\\s*(" REGEX_ADR_REGMEM_B ")|"
        "(mov|add|sub|mul|div|cmp|and|or|xor|test)(w?)\\s+(" REGEX_ADR_REGMEM_W ")\\s*,\\s*(" REGEX_ADR_IMMREG_W ")|"
        "(mov|add|sub|mul|div|cmp|and|or|xor|test)(w?)\\s+(" REGEX_ADR_REGDIR_W ")\\s*,\\s*(" REGEX_ADR_REGMEM_W ")|"

        "(shl|shr)(b)\\s+(" REGEX_ADR_REGMEM_B ")\\s*,\\s*(" REGEX_ADR_IMMREG_B ")|"
        "(shl|shr)(w?)\\s+(" REGEX_ADR_REGMEM_W ")\\s*,\\s*(" REGEX_ADR_IMMREG_W ")"
        ")" REGEX_END // Instructions
    };
};

#endif // lexer.h