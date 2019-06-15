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
enum class Result { Success, Error, End, Uneval, Reloc };

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

typedef struct Line_Info
{
    unsigned    line_num;
    Elf16_Addr  loc_cnt;
    Line        line;
    Line_Info();
    Line_Info(unsigned line_num, Elf16_Addr loc_cnt, Line line);
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
    Elf16_Addr index;
    Elf16_Sym sym;
    bool is_equ;        // Specifies whether the symbol is defined by .equ directive
                        // If this is true and the sym.st_shndx is SHN_UNDEF, the expression
                        // must be evaluated on each use since the value is relocatable
    Symtab_Entry();
    Symtab_Entry(Elf16_Word name, Elf16_Addr value, uint8_t info, Elf16_Section shndx, bool is_equ = false);
} Symtab_Entry;

typedef struct Shdrtab_Entry
{
    static Elf16_Addr shdrtab_index;
    Elf16_Addr index;
    Elf16_Shdr shdr;
    Shdrtab_Entry();
    Shdrtab_Entry(Elf16_Word type, Elf16_Word flags, Elf16_Word info = 0, Elf16_Word entsize = 0, Elf16_Word size = 0);
} Shdrtab_Entry;

typedef struct Reltab_Entry
{
    Elf16_Rel   rel;
    Reltab_Entry(Elf16_Word info, Elf16_Addr offset = 0);
} Reltab_Entry;

typedef std::pair<const std::string, Symtab_Entry>                  symtab_pair_t;
typedef std::pair<const std::string, Shdrtab_Entry>                 shdrtab_pair_t;
typedef std::pair<const std::string, std::unique_ptr<Expression>>   equ_uneval_pair_t;
typedef std::pair<int, std::vector<Reltab_Entry>>                   reloc_pair_t;
typedef std::pair<const std::string, reloc_pair_t>                  equ_relocs_pair_t;

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

    Elf16_Ehdr elf_header;

    Section_Info cur_sect;

    std::map<std::string, Elf16_Addr>                   lc_map;
    std::map<std::string, Symtab_Entry>                 symtab_map;
    std::map<std::string, Shdrtab_Entry>                shdrtab_map;
    std::map<std::string, std::vector<Reltab_Entry>>    reltab_map;
    std::map<std::string, std::vector<Elf16_Half>>      section_map;

    std::map<std::string, std::unique_ptr<Expression>>  equ_uneval_map;
    std::map<std::string, reloc_pair_t>                 equ_reloc_map;

    std::vector<std::string>    strtab_vect;
    std::vector<std::string>    shstrtab_vect;
    std::vector<Elf16_Sym*>     symtab_vect;
    std::vector<Elf16_Shdr*>    shdrtab_vect;

    std::vector<Line_Info>      file_vect;
    unsigned                    file_idx;

    bool run_first_pass();
    bool run_second_pass();

    bool evaluate_expressions();

    void print_line(Line_Info &info);
    void print_file(std::ostream &out);

    void finalize();
    void write_output();

    Result process_line(Line_Info &info);
    Result process_directive(const Directive &dir);
    Result process_instruction(const Instruction &instr);
    Result process_expression(const Expression &expr, int &value, bool allow_undef = false, const std::string &equ_name = "");

    bool get_symtab_entry(const std::string &str, Symtab_Entry &entry, bool silent = false);
    std::string get_section_name(unsigned shndx);
    int get_operand_code_size(const std::string &str, uint8_t operand_size);

    bool add_symbol(const std::string &symbol);
    bool add_shdr(const std::string &name, Elf16_Word type, Elf16_Word flags, bool reloc = false, Elf16_Word info = 0, Elf16_Word entsize = 0);

    void push_byte(Elf16_Half byte);
    void push_word(Elf16_Word word);

    bool insert_operand(const std::string &str, uint8_t size, Elf16_Addr next_instr);
    bool insert_reloc(const std::string &symbol, Elf16_Half type, Elf16_Addr next_instr = 0, bool place = true, std::vector<Reltab_Entry> *relocs_vect = nullptr);
};

#endif  // assembler.h