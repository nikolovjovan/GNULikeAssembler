#ifndef _ASM_H
#define _ASM_H

#include "elf.h"

#include <fstream>
#include <regex>

#define REGEX_CNT 6

/* Regex strings that match specific components */

/* Line START: matches all whitespace BEFORE any valid symbols */
#define REGEX_START "^\\s*"
/* Line END: matches all whitespace AFTER valid symbols and comments */
#define REGEX_END "\\s*(?:#.*)?$"

/* Valid symbol format: starts with '.' or '_' or 'a-z' then can also contain digits */
#define REGEX_SYM "[._a-z][.\\w]*"

/* Valid byte value format: binary ex. 0b[0|1]+; octal ex. 0[0-7]+; decimal ex. [1-9][0-9]*; hexadecimal ex. 0x[0-9a-f]+ */
#define REGEX_VAL_B "[-~]?(?:0b[0-1]{1,8}|0[0-7]{1,3}|[1-9]\\d{0,2}|0x[\\da-f]{1,2})"

/* Valid word value format: binary ex. 0b[0|1]+; octal ex. 0[0-7]+; decimal ex. [1-9][0-9]*; hexadecimal ex. 0x[0-9a-f]+ */
#define REGEX_VAL_W "[-~]?(?:0b[0-1]{1,16}|0[0-7]{1,6}|[1-9]\\d{0,4}|0x[\\da-f]{1,4})"

/* Valid expression format: anything that does not match a comment */
#define REGEX_EXPR "[^#]*?"

/* Immediate addressing modes */
#define REGEX_ADR_IMM_B REGEX_VAL_B "|&" REGEX_SYM // <val> or &<symbol_name> (immediate value embedded inside the instruction)
#define REGEX_ADR_IMM_W REGEX_VAL_W "|&" REGEX_SYM // <val> or &<symbol_name> (immediate value embedded inside the instruction)

/* Register addressing modes */
#define REGEX_ADR_REGDIR_B "r[0-7][hl]" // r<num>h or r<num>l (register high or low addressing)
#define REGEX_ADR_REGDIR_W "r[0-7]|sp|pc" // r<num> or sp or pc (psw can be only addressed in push/pop <=> pushf/popf) (full register addressing)

/* Memory addressing modes */
#define REGEX_ADR_REGIND "\\[\\s*(?:" REGEX_ADR_REGDIR_W ")\\s*\\]|(?:" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(?:" REGEX_VAL_W "|" REGEX_SYM ")\\s*\\]" // [r<num>] or r<num>[<val>] or r<num>[<symbol_name>]
#define REGEX_ADR_ABS "\\*" REGEX_VAL_W // *<val> (absolute data addressing)
#define REGEX_ADR_SYM "\\$?" REGEX_SYM // <symbol_name> or $<symbol_name> (absolute or PC-relative symbol addressing)
#define REGEX_ADR_MEM REGEX_ADR_REGIND "|" REGEX_ADR_ABS "|" REGEX_ADR_SYM // all memory addressing modes for both byte and word sized operands

/* Combinational addressing modes */
#define REGEX_ADR_IMMREG_B REGEX_ADR_IMM_B "|" REGEX_ADR_REGDIR_B // immediate and register direct addressing modes for byte sized operands
#define REGEX_ADR_IMMREG_W REGEX_ADR_IMM_W "|" REGEX_ADR_REGDIR_W // immediate and register direct addressing modes for word sized operands
#define REGEX_ADR_REGMEM_B REGEX_ADR_REGDIR_B "|" REGEX_ADR_MEM // register and memory addressing modes for byte sized operands
#define REGEX_ADR_REGMEM_W REGEX_ADR_REGDIR_W "|" REGEX_ADR_MEM // register and memory addressing modes for word sized operands

typedef enum { SUCCESS, END, ERROR } parse_t; // parse result

class Assembler
{
public:
    Assembler(const std::string &input_file, const std::string &output_file, bool binary = false);
    ~Assembler();

    bool assemble();

private:
    typedef enum { FIRST_PASS, SECOND_PASS } pass_t; // assembler pass
    typedef enum { EMPTY, LABEL, DIRECTIVE, ZEROADDR, ONEADDR, TWOADDR } expr_t; // expression type

    std::string input_file, output_file;
    std::ifstream input;
    std::ofstream output;
    bool binary;

    pass_t pass;

    std::map<std::string, Elf16_Addr> lcMap; // Section name to location counter map

    std::string cur_sect; // Currently active section
    Elf16_Addr cur_lc; // Active section location counter

    bool run_first_pass();
    bool run_second_pass();

    parse_t parse(const std::string &line);
    parse_t parse_directive(const std::smatch &match, unsigned first);
    parse_t parse_zeroaddr(const std::smatch &match, unsigned first);
    parse_t parse_oneaddr(const std::smatch &match, unsigned first);
    parse_t parse_twoaddr(const std::smatch &match, unsigned first);

    bool add_symbol(const std::string &symbol);

    std::regex regex_exprs[REGEX_CNT];
    std::string regex_strings[REGEX_CNT] = {
        REGEX_START REGEX_END, // Empty line (with comment)

        REGEX_START "(" REGEX_SYM "):\\s*(" REGEX_EXPR ")" REGEX_END, // Label

        REGEX_START // Directives
        "\\.(?:"
        "(global|extern|byte|word)\\s+(" REGEX_EXPR ")|"
        "(equ|set)\\s+(" REGEX_SYM "),\\s*(" REGEX_EXPR ")|"
        "(text|data|bss|end)|"
        "(section)\\s+(" REGEX_SYM ")\\s*(?:,\\s*\"(b?d?r?w?x?\\d?)\")?|"
        "(align)\\s+(" REGEX_VAL_B ")\\s*(?:,\\s*(" REGEX_VAL_B "))?\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        "(skip)\\s+(" REGEX_VAL_W ")\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        ")"
        REGEX_END, // Directives

        REGEX_START "(nop|halt|ret|iret)" REGEX_END, // 0-address instructions

        REGEX_START // 1-address instructions
        "(?:"
        "(int)\\s+(" REGEX_ADR_IMM_B ")|"

        "(not)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(not)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(pushf)|" // pushf <=> push psw (push psw is not allowed but it is coded)
        "(popf)|" // popf <=> pop psw (pop psw is not allowed but it is coded)

        "(push)(b)\\s+(" REGEX_ADR_IMMREG_B "|" REGEX_ADR_MEM ")|"
        "(push)(w?)\\s+(" REGEX_ADR_IMMREG_W "|" REGEX_ADR_MEM ")|"
        "(pop)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(pop)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(jmp|jeq|jne|jgt|call)\\s+(" REGEX_ADR_MEM ")"
        ")"
        REGEX_END,

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
        ")"
        REGEX_END // Instructions
    };
};

#endif  /* asm.h */