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

    std::vector<Line_Info>  file_vect; // { line number, { loc_cnt, line } }

    std::map<std::string, Elf16_Addr>                   lc_map;
    // std::map<std::string, Symtab_Entry>                 symtab_map;
    // std::map<std::string, Shdrtab_Entry>                shdrtab_map;
    // std::map<std::string, std::vector<Reltab_Entry>>    reltab_map;
    // std::map<std::string, std::vector<Elf16_Half>>      section_map;

    // std::vector<std::string>    strtab_vect;
    // std::vector<std::string>    shstrtab_vect;
    // std::vector<Symtab_Entry>   symtab_vect;

    Section_Info cur_sect;

    bool run_first_pass();
    bool run_second_pass();

    void write_output();
    void print_line(Line_Info &info);

    bool process_line(Line_Info &info);
    bool process_directive(const Directive &dir);
    bool process_instruction(const Instruction &instr);

    uint8_t get_directive_size(const Directive &dir);
    uint8_t get_instruction_size(const Instruction &instr);
    uint8_t get_operand_size(const string &op);

    // bool decode_word(const std::string &value, Elf16_Word &result);
    // bool decode_byte(const std::string &value, Elf16_Half &result);

    // bool add_symbol(const std::string &symbol);
    // bool add_shdr(const std::string &name, Elf16_Word type, Elf16_Word flags, bool reloc = false, Elf16_Word info = 0, Elf16_Word entsize = 0);

    // void global_symbol(const std::string &str);

    // unsigned get_operand_size(const std::string &operand);
    // bool insert_operand(const std::string &operand, const char size, Elf16_Addr next_instr);
    // bool add_reloc(const std::string symbol, Elf16_Half type, Elf16_Addr next_instr);
};

#endif  // assembler.h