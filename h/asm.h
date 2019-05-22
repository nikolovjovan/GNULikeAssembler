#ifndef _ASM_H
#define _ASM_H

#include "elf.h"

#include <fstream>
#include <regex>

#define REGEX_CNT 3

#define REGEX_BEGIN "^\\s*"
#define REGEX_END "\\s*(?:#.*)?$"

#define REGEX_SYM "([._a-z][.\\w]*)"
#define REGEX_NUM "(0b[0-1]+|0[0-7]+|[1-9]\\d*|0x[\\da-f]+)"
#define REGEX_EXPR "([^#]*?)"

typedef enum { SUCCESS, END, ERROR } parse_t; // parse result

class Assembler
{
public:
    Assembler(const std::string &input_file, const std::string &output_file, bool binary = false);
    ~Assembler();

    bool assemble();

private:
    typedef enum { FIRST_PASS, SECOND_PASS } pass_t; // assembler pass
    typedef enum { EMPTY, LABEL, DIRECTIVE } expr_t; // expression type

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
    parse_t parse_directive(const std::smatch &match);

    bool add_symbol(const std::string &symbol);

    std::regex regex_exprs[REGEX_CNT];
    std::string regex_strings[REGEX_CNT] = {
        REGEX_BEGIN REGEX_END, // Empty line (with comment)

        REGEX_BEGIN REGEX_SYM ":\\s*" REGEX_EXPR REGEX_END, // Label

        REGEX_BEGIN // Directives
        "\\.(?:"
        "(global|extern|byte|word)\\s+" REGEX_EXPR "|"
        "(equ|set)\\s+" REGEX_SYM ",\\s*" REGEX_EXPR "|"
        "(text|data|bss|end)|"
        "(section)\\s+" REGEX_SYM "\\s*(?:,\\s*\"(b?d?r?w?x?\\d?)\")?|"
        "(align)\\s+" REGEX_NUM "\\s*(?:,\\s*" REGEX_NUM ")?\\s*(?:,\\s*" REGEX_NUM ")?|"
        "(skip)\\s+" REGEX_NUM "\\s*(?:,\\s*" REGEX_NUM ")?|"
        ")"
        REGEX_END
    };
};

#endif  /* asm.h */