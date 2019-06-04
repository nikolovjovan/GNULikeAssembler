#ifndef _ASM_H
#define _ASM_H

#include "elf.h"

#include <fstream>
#include <regex>

// Regex strings that match specific tokens

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

// Valid expression format: anything that does not match a comment
#define REGEX_EXPR "[^#]*?"

// Immediate addressing modes
#define REGEX_ADR_IMM_B REGEX_VAL_B // <val> (immediate value embedded inside the instruction)
#define REGEX_ADR_IMM_W REGEX_VAL_W "|&" REGEX_SYM // <val> or &<symbol_name> (immediate value embedded inside the instruction)

// Register addressing modes
#define REGEX_ADR_REGDIR_B "r[0-7][hl]" // r<num>h or r<num>l (register high or low addressing)
#define REGEX_ADR_REGDIR_W "r[0-7]|sp|pc" // r<num> or sp or pc (psw can be only addressed in push/pop <=> pushf/popf) (full register addressing)

// Memory addressing modes
#define REGEX_ADR_REGIND "\\[\\s*(?:" REGEX_ADR_REGDIR_W ")\\s*\\]|(?:" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(?:" REGEX_VAL_W "|" REGEX_SYM ")\\s*\\]" // [r<num>] or r<num>[<val>] or r<num>[<symbol_name>]
#define REGEX_ADR_ABS "\\*" REGEX_VAL_W // *<val> (absolute data addressing)
#define REGEX_ADR_SYM "\\$?" REGEX_SYM // <symbol_name> or $<symbol_name> (absolute or PC-relative symbol addressing)
#define REGEX_ADR_MEM REGEX_ADR_REGIND "|" REGEX_ADR_ABS "|" REGEX_ADR_SYM // all memory addressing modes for both byte and word sized operands

// Combinational addressing modes
#define REGEX_ADR_IMMREG_B REGEX_ADR_IMM_B "|" REGEX_ADR_REGDIR_B // immediate and register direct addressing modes for byte sized operands
#define REGEX_ADR_IMMREG_W REGEX_ADR_IMM_W "|" REGEX_ADR_REGDIR_W // immediate and register direct addressing modes for word sized operands
#define REGEX_ADR_REGMEM_B REGEX_ADR_REGDIR_B "|" REGEX_ADR_MEM // register and memory addressing modes for byte sized operands
#define REGEX_ADR_REGMEM_W REGEX_ADR_REGDIR_W "|" REGEX_ADR_MEM // register and memory addressing modes for word sized operands

typedef struct Symtab_Entry
{
    static Elf16_Addr symtab_index;

    Elf16_Addr index = symtab_index++;
    Elf16_Sym sym;

    Symtab_Entry(Elf16_Word name, Elf16_Addr value, uint8_t info, Elf16_Section shndx)
    {
        sym.st_name     = name;     // String table index
        sym.st_value    = value;    // Symbol value = 1* label - current location counter, 2* constant - .equ value
        sym.st_size     = 0;        // Symbol size
        sym.st_info     = info;     // Symbol type and binding
        sym.st_other    = 0;        // No defined meaning, 0
        sym.st_shndx    = shndx;    // Section header table index
    }
} Symtab_Entry;

typedef struct Shdrtab_Entry
{
    static Elf16_Addr shdrtab_index;

    Elf16_Addr index = shdrtab_index++;
    Elf16_Shdr shdr;

    Shdrtab_Entry(Elf16_Word type, Elf16_Word flags, Elf16_Word info = 0, Elf16_Word entsize = 0, Elf16_Word size = 0)
    {
        shdr.sh_name        = index;    // Section header string table index
        shdr.sh_type        = type;     // Section type
        shdr.sh_flags       = flags;    // Section flags
        shdr.sh_addr        = 0;        // Section virtual address
        shdr.sh_offset      = 0;        // Section file offset
        shdr.sh_size        = size;     // Section size in bytes
        shdr.sh_link        = 0;        // Link to another section
        shdr.sh_info        = info;     // Additional section information
        shdr.sh_addralign   = 0;        // Section alignment (0 | 1 = no alignment, 2^n = alignment)
        shdr.sh_entsize     = entsize;  // Entry size if section holds table
    }
} Shdrtab_Entry;

typedef struct Reltab_Entry
{
    Elf16_Rel   rel;

    Reltab_Entry(Elf16_Word info, Elf16_Addr offset = 0)
    {
        rel.r_offset = offset;
        rel.r_info = info;
    }
} Reltab_Entry;

typedef std::pair<const std::string, Symtab_Entry>  Symtab_Pair;
typedef std::pair<const std::string, Shdrtab_Entry> Shdrtab_Pair;

enum class Pass { First, Second };
enum class Parse_Result { Success, Error, End };

class Addressing_Mode
{
public:
    enum
    {
        Imm         = 0x0 << 5, // 0 0 0 R3 R2 R1 R0 L/H
        RegDir      = 0x1 << 5, // 0 0 1 R3 R2 R1 R0 L/H
        RegInd      = 0x2 << 5, // 0 1 0 R3 R2 R1 R0 L/H
        RegIndOff8  = 0x3 << 5, // 0 1 1 R3 R2 R1 R0 L/H
        RegIndOff16 = 0x4 << 5, // 1 0 0 R3 R2 R1 R0 L/H
        Mem         = 0x5 << 5  // 1 0 1 R3 R2 R1 R0 L/H
    };
};

#define REGEX_CNT           6
#define OPCODE_CNT          26
#define DIRECTIVE_CNT       13
#define ZEROADDRINSTR_CNT   4
#define ONEADDRINSTR_CNT    11
#define TWOADDRINSTR_CNT    11

class Regex_Match { public: enum { Empty = 0, Label, Directive, ZeroAddr, OneAddr, TwoAddr }; };
class Directive { public: enum { Section = 0, Text, Data, Bss, End, Global, Extern, Byte, Word, Equ, Set, Align, Skip }; };
class ZeroAddrInstr { public: enum { Nop = 0, Halt, Ret, Iret }; };
class OneAddrInstr { public: enum { Pushf = 0, Popf, Int, Not, Jmp, Jeq, Jne, Jgt, Call, Push, Pop }; };
class TwoAddrInstr { public: enum { Xchg = 0, Mov, Add, Sub, Mul, Div, Cmp, And, Or, Xor, Test }; };

typedef struct section_info
{
    std::string name;          // Section name
    Elf16_Word  type;          // Section type
    Elf16_Word  flags;         // Section flags
    Elf16_Addr  loc_cnt;       // Section location counter
    Elf16_Addr  shdrtab_index; // Section header table index
} Section_Info;

class Assembler
{
public:
    Assembler(const std::string &input_file, const std::string &output_file, bool binary = false);
    ~Assembler();

    bool assemble();

private:
    std::string     input_file, output_file;
    std::ifstream   input;
    std::ofstream   output;
    bool            binary;

    Pass pass;

    std::map<std::string, Elf16_Addr>                   lc_map;
    std::map<std::string, Symtab_Entry>                 symtab_map;
    std::map<std::string, Shdrtab_Entry>                shdrtab_map;
    std::map<std::string, std::vector<Reltab_Entry>>    reltab_map;
    std::map<std::string, std::vector<Elf16_Half>>      section_map;

    std::vector<std::string>    strtab_vect;
    std::vector<std::string>    shstrtab_vect;
    std::vector<Symtab_Entry>   symtab_vect;

    Section_Info cur_sect;

    bool run_first_pass();
    bool run_second_pass();
    void write_output();

    Parse_Result parse(const std::string &line);
    Parse_Result parse_label(const std::smatch &match, unsigned index);
    Parse_Result parse_directive(const std::smatch &match, unsigned index);
    Parse_Result parse_zeroaddr(const std::smatch &match, unsigned index);
    Parse_Result parse_oneaddr(const std::smatch &match, unsigned index);
    Parse_Result parse_twoaddr(const std::smatch &match, unsigned index);

    bool decode_word(const std::string &value, Elf16_Word &result);
    bool decode_byte(const std::string &value, Elf16_Half &result);

    bool add_symbol(const std::string &symbol);
    bool add_shdr(const std::string &name, Elf16_Word type, Elf16_Word flags, bool reloc = false, Elf16_Word info = 0, Elf16_Word entsize = 0);

    void global_symbol(const std::string &str);

    unsigned get_operand_size(const std::string &operand);
    bool insert_operand(const std::string &operand, const char size, Elf16_Addr next_instr);
    bool add_reloc(const std::string symbol, Elf16_Half type, Elf16_Addr next_instr);

    std::regex regex_split, regex_symbol, regex_byte, regex_word, regex_op1b, regex_op2b,
               regex_imm_b, regex_imm_w, regex_regdir_b, regex_regdir_w, regex_regind,
               regex_regindoff, regex_regindsym, regex_memsym, regex_memabs;

    const std::string regex_split_string    = "\\s*,",
                      regex_symbol_string   = "\\s*(" REGEX_SYM ")\\s*",
                      regex_byte_string     = "\\s*(" REGEX_VAL_B ")\\s*",
                      regex_word_string     = "\\s*(" REGEX_VAL_W ")\\s*",
                      regex_op1b_string     = "\\s*(" REGEX_ADR_REGDIR_B "|" REGEX_ADR_REGDIR_W "|\\[\\s*(?:" REGEX_ADR_REGDIR_W ")\\s*\\])\\s*",
                      regex_op2b_string     = "\\s*(?:(" REGEX_ADR_IMM_B ")|(?:" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(" REGEX_ADR_IMM_B ")\\s*\\])\\s*";

    std::regex regex_exprs[REGEX_CNT];

    const std::string regex_strings[REGEX_CNT] = {
        REGEX_START REGEX_END, // Empty line (with comment)

        REGEX_START "(" REGEX_SYM "):\\s*(" REGEX_EXPR ")" REGEX_END, // Label

        REGEX_START // Directives
        "\\.(?:"
        "(section)\\s+(" REGEX_SYM ")\\s*(?:,\\s*\"(a?e?w?x?)\")?|" // flags: a-allocatable, e-excluded from executable and shared library (bss), w-writable, x-executable
        "(text|data|bss|end)|"
        "(global|extern|byte|word)\\s+(" REGEX_EXPR ")|"
        "(equ|set)\\s+(" REGEX_SYM "),\\s*(" REGEX_EXPR ")|"
        "(align)\\s+(" REGEX_VAL_B ")\\s*(?:,\\s*(" REGEX_VAL_B "))?\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        "(skip)\\s+(" REGEX_VAL_W ")\\s*(?:,\\s*(" REGEX_VAL_B "))?|"
        ")"
        REGEX_END, // Directives

        REGEX_START "(nop|halt|ret|iret)" REGEX_END, // 0-address instructions

        REGEX_START // 1-address instructions
        "(?:"
        "(int)()\\s+(" REGEX_ADR_IMM_B ")|"

        "(not)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(not)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(pushf)()|" // pushf <=> push psw (push psw is not allowed but it is coded)
        "(popf)()|" // popf <=> pop psw (pop psw is not allowed but it is coded)

        "(push)(b)\\s+(" REGEX_ADR_IMMREG_B "|" REGEX_ADR_MEM ")|"
        "(push)(w?)\\s+(" REGEX_ADR_IMMREG_W "|" REGEX_ADR_MEM ")|"
        "(pop)(b)\\s+(" REGEX_ADR_REGMEM_B ")|"
        "(pop)(w?)\\s+(" REGEX_ADR_REGMEM_W ")|"

        "(jmp|jeq|jne|jgt|call)()\\s+(" REGEX_ADR_MEM ")"
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

    const std::string directive[DIRECTIVE_CNT]              = { "section", "text", "data", "bss", "end", "global", "extern", "byte", "word", "equ", "set", "align", "skip" };
    const std::string zero_addr_instr[ZEROADDRINSTR_CNT]    = { "nop", "halt", "ret", "iret" };
    const std::string one_addr_instr[ONEADDRINSTR_CNT]      = { "pushf", "popf", "int", "not", "jmp", "jeq", "jne", "jgt", "call", "push", "pop" };
    const std::string two_addr_instr[TWOADDRINSTR_CNT]      = { "xchg", "mov", "add", "sub", "mul", "div", "cmp", "and", "or", "xor", "test" };

    std::map<std::string, unsigned> directive_map, zero_addr_instr_map, one_addr_instr_map, two_addr_instr_map;

    const std::string opcodeMnems[OPCODE_CNT] = {
        "nop", "halt", "xchg", "int", "mov", "add", "sub", "mul", "div", "cmp",
        "not", "and", "or", "xor", "test", "shl", "shr", "push", "pop",
        "jmp", "jeq", "jne", "jgt", "call", "ret", "iret"
    };

    std::map<std::string, Elf16_Half>   opcode_map;
};

#endif  // asm.h