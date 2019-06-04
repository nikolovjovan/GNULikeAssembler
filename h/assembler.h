#ifndef _ASSEMBLER_H
#define _ASSEMBLER_H

#include "elf.h"
#include "lexer.h"
#include "parser.h"

#include <fstream>
#include <string>
#include <vector>
#include <map>

enum class Pass { First, Second };
enum class Result { Success, Error, End };

typedef struct Line_Info
{
    unsigned    line_num;
    Elf16_Addr  loc_cnt;
    Line        line;
    Line_Info() {}
    Line_Info(unsigned line_num, Elf16_Addr loc_cnt, Line line) : line_num(line_num), loc_cnt(loc_cnt), line(line) {}
} Line_Info;

typedef struct Section_Info
{
    std::string name;          // Section name
    Elf16_Word  type;          // Section type
    Elf16_Word  flags;         // Section flags
    Elf16_Addr  loc_cnt;       // Section location counter
    Elf16_Addr  shdrtab_index; // Section header table index
} Section_Info;

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

    Lexer   *lexer;
    Parser  *parser;
    Pass    pass;

    Section_Info cur_sect;

    std::map<std::string, Elf16_Addr>                   lc_map;
    std::map<std::string, Symtab_Entry>                 symtab_map;
    std::map<std::string, Shdrtab_Entry>                shdrtab_map;
    std::map<std::string, std::vector<Reltab_Entry>>    reltab_map;
    std::map<std::string, std::vector<Elf16_Half>>      section_map;

    std::vector<Line_Info>      file_vect;
    std::vector<std::string>    strtab_vect;
    std::vector<std::string>    shstrtab_vect;
    std::vector<Symtab_Entry>   symtab_vect;

    bool run_first_pass();
    bool run_second_pass();

    void write_output();
    void print_line(Line_Info &info);

    Result process_line(Line_Info &info);
    Result process_directive(const Directive &dir);
    Result process_instruction(const Instruction &instr);

    bool add_symbol(const std::string &symbol);
    bool add_shdr(const std::string &name, Elf16_Word type, Elf16_Word flags, bool reloc = false, Elf16_Word info = 0, Elf16_Word entsize = 0);

    bool insert_operand(const std::string &str, uint8_t size, Elf16_Addr next_instr);
    bool add_reloc(const std::string &symbol, Elf16_Half type, Elf16_Addr next_instr);
};

#endif  // assembler.h