#include "assembler.h"

#include <iostream>
#include <iomanip>

using std::cerr;
using std::cout;
using std::dec;
using std::hex;
using std::ifstream;
using std::ios;
using std::left;
using std::map;
using std::ostream;
using std::pair;
using std::right;
using std::setfill;
using std::setw;
using std::stack;
using std::string;
using std::unique_ptr;
using std::vector;

Line_Info::Line_Info() {}
Line_Info::Line_Info(unsigned line_num, Elf16_Addr loc_cnt, Line line) : line_num(line_num), loc_cnt(loc_cnt), line(line) {}

Symtab_Entry::Symtab_Entry() {};

Symtab_Entry::Symtab_Entry(Elf16_Word name, Elf16_Addr value, uint8_t info, Elf16_Section shndx, bool is_equ)
    : index(symtab_index++), is_equ(is_equ)
{
    sym.st_name     = name;     // String table index
    sym.st_value    = value;    // Symbol value
    sym.st_size     = 0;        // Symbol size
    sym.st_info     = info;     // Symbol type and binding
    sym.st_other    = 0;        // No defined meaning, 0
    sym.st_shndx    = shndx;    // Section header table index
}

Elf16_Addr Symtab_Entry::symtab_index   = 0;

Shdrtab_Entry::Shdrtab_Entry() {};

Shdrtab_Entry::Shdrtab_Entry(Elf16_Word type, Elf16_Word flags, Elf16_Word info, Elf16_Word entsize, Elf16_Word size)
    : index(shdrtab_index++)
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

Elf16_Addr Shdrtab_Entry::shdrtab_index = 0;

Reltab_Entry::Reltab_Entry(Elf16_Word info, Elf16_Addr offset)
{
    rel.r_offset = offset;
    rel.r_info = info;
}

Assembler::Assembler(const string &input_file, const string &output_file, bool binary)
{
    this->input_file    = input_file;
    this->output_file   = output_file;
    this->binary        = binary;

    // Initializing lexer and parser
    lexer   = new Lexer();
    parser  = new Parser(lexer);

    // Initializing variables
    cur_sect.name           = "";
    cur_sect.type           = SHT_NULL;
    cur_sect.flags          = 0;
    cur_sect.loc_cnt        = 0;
    cur_sect.shdrtab_index  = 0;

    // Inserting a dummy symbol
    Symtab_Entry dummySym(0, 0, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), SHN_UNDEF);
    symtab_map.insert(symtab_pair_t("", dummySym));
    strtab_vect.push_back("");

    // Inserting a dummy section header
    Shdrtab_Entry dummyShdr(SHT_NULL, 0, 0);
    shdrtab_map.insert(shdrtab_pair_t("", dummyShdr));
    shstrtab_vect.push_back("");
}

Assembler::~Assembler()
{
    delete(parser);
    delete(lexer);
    if (input.is_open())
        input.close();
    if (output.is_open())
        output.close();
}

bool Assembler::assemble()
{
    if (!run_first_pass())
    {
        cerr << "ERROR: Assembler failed to complete first pass!\n";
        return false;
    }

    cur_sect.name = "";
    cur_sect.type = SHT_NULL;
    cur_sect.flags = 0;
    cur_sect.loc_cnt = 0;
    cur_sect.shdrtab_index = 0;
    lc_map.clear();

    if (!evaluate_expressions()) return false;

    if (!run_second_pass())
    {
        cerr << "ERROR: Assembler failed to complete second pass!\n";
        return false;
    }

    finalize();
    write_output();

    return true;
}

bool Assembler::run_first_pass()
{
    pass = Pass::First;
    bool res = true;
    string line_str;
    Line_Info info;

    cout << ">>> FIRST PASS <<<\n\n";

    if (input.is_open())
        input.close();
    input.open(input_file, ifstream::in);

    for (info.line_num = 1; !input.eof(); ++info.line_num)
    {
        getline(input, line_str);
        cout << info.line_num << ":\t" << line_str << '\n';
        if (parser->parse_line(line_str, info.line))
        {
            Result tmp = process_line(info);
            if (tmp == Result::Success) continue;
            if (tmp == Result::Error)
            {
                cerr << "ERROR: Failed to process line: " << info.line_num << "!\n";
                res = false;
                break;
            }
            cout << "End of file reached at line: " << info.line_num << "!\n";
            break;
        }
        else
        {
            cerr << "ERROR: Failed to parse line: " << info.line_num << "!\n";
            res = false;
            break;
        }
    }

    input.close();
    return res;
}

bool Assembler::run_second_pass()
{
    pass = Pass::Second;
    bool res = true;

    cout << "\n>>> SECOND PASS <<<\n\n";

    for (unsigned i = 0; i < file_vect.size(); ++i)
    {
        print_line(file_vect[i]); // temporary
        cout << '\n';
        Result tmp = process_line(file_vect[i]);
        if (tmp == Result::Success) continue;
        if (tmp == Result::Error)
        {
            cerr << "ERROR: Failed to process line: " << file_vect[i].line_num << "!\n";
            res = false;
            break;
        }
        cout << "End of file reached at line: " << file_vect[i].line_num << "!\n";
        break;
    }

    return res;
}

bool Assembler::evaluate_expressions()
{
    Result res;
    int value;
    bool changes = true;
    while (changes)
    {
        changes = false;
        for (auto it = equ_uneval_map.begin(); it != equ_uneval_map.end(); ++it)
        {
            res = process_expression(*(it->second), value, true, it->first);
            if (res == Result::Error)
            {
                cerr << "ERROR: Failed to evaluate expression for .equ symbol '" << it->first << "'!\n";
                return false;
            }
            if (res == Result::Success)
            {
                if (symtab_map.count(it->first) == 0)
                {   // should never happen
                    cerr << "ERROR: Assembler error: Unevaluated .equ symbol '" << it->first << "' is undefined!\n";
                    return false;
                }
                Symtab_Entry &entry = symtab_map.at(it->first);
                entry.sym.st_shndx = SHN_ABS;
                entry.sym.st_value = value;
            }
            changes = true;
            equ_uneval_map.erase(it->first);
            break;
        }
    }
    if (equ_uneval_map.empty()) return true;
    return false;
}

void Assembler::print_line(Line_Info &info)
{
    cout << info.line_num << ":\t";
    cout << "LC = " << info.loc_cnt << "\t";

    if (!info.line.label.empty())
        cout << info.line.label << ": ";

    if (info.line.content_type == Content_Type::None) return;

    if (info.line.content_type == Content_Type::Directive)
    {
        cout << "." << parser->get_directive(info.line.getDir().code);
        if (!info.line.getDir().p1.empty())
            cout << " " << info.line.getDir().p1;
        if (!info.line.getDir().p2.empty())
            cout << ", " << info.line.getDir().p2;
        if (!info.line.getDir().p3.empty())
            cout << ", " << info.line.getDir().p3;
    }
    else
    {
        cout << parser->get_instruction(info.line.getInstr().code);
        if (info.line.getInstr().op_cnt > 0)
        {
            cout << (info.line.getInstr().op_size == Operand_Size::Byte ? 'b' : 'w');
            cout << " " << info.line.getInstr().op1;
            if (info.line.getInstr().op_cnt > 1)
                cout << ", " << info.line.getInstr().op2;
        }
    }
}

void Assembler::print_file(ostream &out)
{
    // Output ELF Header
    out << "ELF Header:\n";
    out << "  Magic:   ";
    for (unsigned i = 0; i < EI_NIDENT; ++i)
        out << hex << (unsigned) elf_header.e_ident[i] << (i < EI_NIDENT - 1 ? ' ' : '\n');
    out << "  Class:                             " << (elf_header.e_ident[EI_CLASS] == ELFCLASS16 ? "ELF16" : "unknown") << '\n';
    out << "  Data:                              " << (elf_header.e_ident[EI_DATA] == ELFDATA2LSB ? "2's complement, little endian" : "unknown") << '\n';
    out << "  Version:                           " << (elf_header.e_ident[EI_CLASS] == EV_CURRENT ? "1 (current)" : "unknown") << '\n';
    out << "  Type:                              ";
    switch (elf_header.e_type)
    {
    case ET_REL: out << "REL (Relocatable file)"; break;
    case ET_EXEC: out << "EXEC (Executable file"; break;
    case ET_DYN: out << "DYN (Shared object file)"; break;
    default: out << "unknown";
    }
    out << '\n';
    out << "  Machine:                           " << (elf_header.e_machine == EM_VN16 ? "Von-Neumann 16-bit" : "unknown") << '\n';
    out << "  Version:                           " << hex << (unsigned) elf_header.e_version << '\n';
    out << "  Entry point address:               " << hex << (unsigned) elf_header.e_entry << '\n';
    out << "  Start of program headers:          " << dec << (unsigned) elf_header.e_phoff << " (bytes into file)\n";
    out << "  Start of section headers:          " << dec << (unsigned) elf_header.e_shoff << " (bytes into file)\n";
    out << "  Flags:                             " << hex << (unsigned) elf_header.e_flags << '\n';
    out << "  Size of this header:               " << dec << (unsigned) elf_header.e_ehsize << " (bytes)\n";
    out << "  Size of program headers:           " << dec << (unsigned) elf_header.e_phentsize << " (bytes)\n";
    out << "  Number of program headers:         " << dec << (unsigned) elf_header.e_phnum << '\n';
    out << "  Size of section headers:           " << dec << (unsigned) elf_header.e_shentsize << " (bytes)\n";
    out << "  Number of section headers:         " << dec << (unsigned) elf_header.e_shnum << '\n';
    out << "  Section header string table index: " << dec << (unsigned) elf_header.e_shstrndx << '\n';

    out << '\n';
    out << "Section Headers:\n";
    out << "  [Nr] Name                 Type                 Address   Offset\n";
    out << "       Size      EntSize    Flags  Link   Info   Align\n";

    string flags;
    for (unsigned i = 0; i < shdrtab_vect.size(); ++i)
    {
        out << "  [" << dec << setw(2) << setfill(' ') << right << i << "] ";
        out << setw(20) << setfill(' ') << left << get_section_name(shdrtab_vect[i]->sh_name) << ' ';
        out << setw(20) << setfill(' ') << left;
        switch (shdrtab_vect[i]->sh_type)
        {
        case SHT_NULL: out << "NULL"; break;
        case SHT_PROGBITS: out << "PROGBITS"; break;
        case SHT_SYMTAB: out << "SYMTAB"; break;
        case SHT_STRTAB: out << "STRTAB"; break;
        case SHT_NOBITS: out << "NOBITS"; break;
        case SHT_REL: out << "REL"; break;
        default: out << "UNKNOWN"; break;
        }
        out << ' ';
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i]->sh_addr << "      ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i]->sh_offset << "\n       ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i]->sh_size << "      ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i]->sh_entsize << "       ";
        flags.clear();
        if (shdrtab_vect[i]->sh_flags & SHF_WRITE) flags.push_back('W');
        if (shdrtab_vect[i]->sh_flags & SHF_ALLOC) flags.push_back('A');
        if (shdrtab_vect[i]->sh_flags & SHF_EXECINSTR) flags.push_back('X');
        if (shdrtab_vect[i]->sh_flags & SHF_INFO_LINK) flags.push_back('I');
        out << setw(7) << setfill(' ') << left << flags;
        out << setw(7) << setfill(' ') << left << dec << (unsigned) shdrtab_vect[i]->sh_link;
        out << setw(7) << setfill(' ') << left << dec << (unsigned) shdrtab_vect[i]->sh_info;
        out << left << dec << (shdrtab_vect[i]->sh_addralign == 0 ? 1 : 2 << shdrtab_vect[i]->sh_addralign - 1) << '\n';
    }
    out << "Key to Flags:\n  W (write), A (alloc), X (execute), I (info)\n";

    for (auto it = shdrtab_vect.begin(); it != shdrtab_vect.end(); ++it)
    {
        string name = shstrtab_vect[(*it)->sh_name];
        switch ((*it)->sh_type)
        {
        case SHT_NULL: break;   // Only section header, no data
        case SHT_PROGBITS:
        {
            vector<Elf16_Half> &data = section_map[name];
            if (data.size() == 0) continue;
            out << "\nContents of section '" << name << "':\n";
            out << setw(8) << setfill(' ') << " ";
            for (unsigned i = 0; i < 0x10; ++i)
                out << hex << i << ':' << (i + 1 < 0x10 ? ' ' : '\n');
            for (unsigned i = 0, offset = (*it)->sh_offset & ~0xf; i < data.size();)
            {
                out << "  " << setw(4) << setfill('0') << right << hex << offset << ": ";
                for (; offset < (*it)->sh_offset; ++offset) out << setw(1) << setfill(' ') << "   ";
                for (unsigned j = 0; i < data.size() && j < 0x10; ++i, ++j, ++offset)
                    out << setw(2) << setfill('0') << right << hex << (unsigned) data[i]
                        << (j + 1 < 0x10 && i + 1 < data.size() ? ' ' : '\n');
            }
        }
        case SHT_SYMTAB:
        {
            if (name != ".symtab") break;
            out << "\nSymbol table '.symtab' contains " << symtab_vect.size() << " entries:\n"
                << "  Num: Value  Size   Type       Bind       Ndx  Name\n";
            for (unsigned i = 0; i < symtab_vect.size(); ++i)
            {
                out << setw(5) << setfill(' ') << right << i << ": ";
                out << setw(4) << setfill('0') << right << hex << (unsigned) symtab_vect[i]->st_value << "   ";
                out << setw(7) << setfill(' ') << left << dec << (unsigned) symtab_vect[i]->st_size;
                out << setw(11) << setfill(' ') << left;
                switch (ELF16_ST_TYPE(symtab_vect[i]->st_info))
                {
                case STT_NOTYPE: out << "NOTYPE"; break;
                case STT_OBJECT: out << "OBJECT"; break;
                case STT_FUNC: out << "FUNC"; break;
                case STT_SECTION: out << "SECTION"; break;
                case STT_FILE: out << "FILE"; break;
                default: out << "unknown"; break;
                }
                out << setw(11) << setfill(' ') << left;
                switch (ELF16_ST_BIND(symtab_vect[i]->st_info))
                {
                case STB_LOCAL: out << "LOCAL"; break;
                case STB_GLOBAL: out << "GLOBAL"; break;
                case STB_WEAK: out << "WEAK"; break;
                default: out << "unknown"; break;
                }
                out << setw(5) << setfill(' ') << left << dec;
                if (symtab_vect[i]->st_shndx == SHN_UNDEF)
                    out << "UND";
                else if (symtab_vect[i]->st_shndx == SHN_ABS)
                    out << "ABS";
                else
                    out << (unsigned) symtab_vect[i]->st_shndx;
                out << strtab_vect[symtab_vect[i]->st_name];
                out << '\n';
            }
            break;
        }
        case SHT_STRTAB:
        {
            if (name == ".strtab")
            {
                out << "\nString table '.strtab' contains " << strtab_vect.size() << " entries:\n";
                for (unsigned i = 0, offset = (*it)->sh_offset; i < strtab_vect.size(); offset += (strtab_vect[i++].length() + 1))
                    out << "  " << setw(4) << setfill('0') << right << hex << offset << ": " << strtab_vect[i] << '\n';
            }
            else if (name == ".shstrtab")
            {
                out << "\nString table '.shstrtab' contains " << shstrtab_vect.size() << " entries:\n";
                for (unsigned i = 0, offset = (*it)->sh_offset; i < shstrtab_vect.size(); offset += (shstrtab_vect[i++].length() + 1))
                    out << "  " << setw(4) << setfill('0') << right << hex << offset << ": " << shstrtab_vect[i] << '\n';
            }
            break;
        }
        case SHT_NOBITS: break; // Only section header, uninitialized data
        case SHT_REL:
        {
            out << "\nRelocation section '" << name << "' contains " << (unsigned) ((*it)->sh_size / (*it)->sh_entsize) << " entries:\n"
                << "  Offset  Info  Type       Section              Symbol\n";
            vector<Reltab_Entry> &reloc = reltab_map[name.substr(4)];
            for (unsigned i = 0; i < reloc.size(); ++i)
            {
                out << "  " << setw(4) << setfill('0') << right << hex << reloc[i].rel.r_offset << "    ";
                out << setw(4) << setfill('0') << right << hex << reloc[i].rel.r_info << "  ";
                out << setw(11) << setfill(' ') << left;
                switch (ELF16_R_TYPE(reloc[i].rel.r_info))
                {
                case R_VN_16: out << "R_VN_16"; break;
                case R_VN_PC16: out << "R_VN_PC_16"; break;
                default: out << "unknown"; break;
                }
                Elf16_Sym *sym = symtab_vect[ELF16_R_SYM(reloc[i].rel.r_info)];
                bool is_section = ELF16_ST_TYPE(sym->st_info) == STT_SECTION;
                if (is_section) out << left << shstrtab_vect[sym->st_shndx];
                else out << "                     " << strtab_vect[sym->st_name];
                out << '\n';
            }
            break;
        }
        }
    }
}

void Assembler::finalize()
{
    // Add extra section headers
    Shdrtab_Entry symtab_entry = Shdrtab_Entry(SHT_SYMTAB, 0, 0, sizeof(Elf16_Sym), sizeof(Elf16_Sym) * symtab_map.size());
    shdrtab_map.insert(shdrtab_pair_t(".symtab", symtab_entry));
    shstrtab_vect.push_back(".symtab");

    unsigned size = 0;
    for (unsigned i = 0; i < strtab_vect.size(); ++i)
        size += (strtab_vect[i].length() + 1);
    Shdrtab_Entry strtab_entry = Shdrtab_Entry(SHT_STRTAB, 0, 0, 0, size);
    shdrtab_map.insert(shdrtab_pair_t(".strtab", strtab_entry));
    shstrtab_vect.push_back(".strtab");

    size = 0;
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        size += it->first.size() + 1;
    Shdrtab_Entry shstrtab_entry = Shdrtab_Entry(SHT_STRTAB, 0, 0, 0, size);
    shdrtab_map.insert(shdrtab_pair_t(".shstrtab", shstrtab_entry));
    shstrtab_vect.push_back(".shstrtab");

    // Generate symbol header table
    if (symtab_vect.capacity() < Symtab_Entry::symtab_index)
        symtab_vect.resize(Symtab_Entry::symtab_index);
    for (auto it = symtab_map.begin(); it != symtab_map.end(); ++it)
        symtab_vect[it->second.index] = &(it->second.sym);

    // Generate section header table
    if (shdrtab_vect.capacity() < Shdrtab_Entry::shdrtab_index)
        shdrtab_vect.resize(Shdrtab_Entry::shdrtab_index);
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        shdrtab_vect[it->second.index] = &(it->second.shdr);

    // Link relocation tables to the symbol table
    for (unsigned i = 0; i < shdrtab_vect.size(); ++i)
        if (shdrtab_vect[i]->sh_type == SHT_REL)
            shdrtab_vect[i]->sh_link = symtab_entry.index;

    // ELF Header
    elf_header.e_ident[EI_MAG0]     = ELFMAG0;
    elf_header.e_ident[EI_MAG1]     = ELFMAG1;
    elf_header.e_ident[EI_MAG2]     = ELFMAG2;
    elf_header.e_ident[EI_MAG3]     = ELFMAG3;
    elf_header.e_ident[EI_CLASS]    = ELFCLASS16;
    elf_header.e_ident[EI_DATA]     = ELFDATA2LSB;
    elf_header.e_ident[EI_VERSION]  = EV_CURRENT;
    for (unsigned i = EI_PAD; i < EI_NIDENT; ++i) elf_header.e_ident[i] = 0;
    elf_header.e_type       = ET_REL;
    elf_header.e_machine    = EM_VN16;
    elf_header.e_version    = EV_CURRENT;
    elf_header.e_entry      = 0;
    elf_header.e_phoff      = 0;
    elf_header.e_shoff      = sizeof(Elf16_Ehdr);
    elf_header.e_flags      = 0;
    elf_header.e_ehsize     = sizeof(Elf16_Ehdr);
    elf_header.e_phentsize  = 0;
    elf_header.e_phnum      = 0;
    elf_header.e_shentsize  = sizeof(Elf16_Shdr);
    elf_header.e_shnum      = shdrtab_map.size();
    elf_header.e_shstrndx   = shstrtab_entry.index;
}

void Assembler::write_output()
{
    if (output.is_open())
        output.close();
    if (binary)
    {
        // Not implemented yet
        // output.open(output_file, ios::out | ios::binary);
    }
    else
    {
        output.open(output_file, ifstream::out);
        print_file(output);
        output.close();
    }
}

Result Assembler::process_line(Line_Info &info)
{
    Result res = Result::Success;
    if (!info.line.label.empty() || info.line.content_type != Content_Type::None)
    {
        if (!info.line.label.empty() && pass == Pass::First)
            if (!add_symbol(info.line.label))
                return Result::Error;
        if (info.line.content_type == Content_Type::Directive)
        {
            if ((res = process_directive(info.line.getDir())) == Result::Error)
                return Result::Error;
        }
        else if (info.line.content_type == Content_Type::Instruction)
        {
            if ((res = process_instruction(info.line.getInstr())) == Result::Error)
                return Result::Error;
        }
        info.loc_cnt = cur_sect.loc_cnt;
        if (pass == Pass::First)
            file_vect.push_back(info);
    }
    return res;
}

Result Assembler::process_directive(const Directive &dir)
{
    switch (dir.code)
    {
    case Directive::Global:
    {
        if (pass == Pass::First) return Result::Success;
        string symbol;
        for (string token : lexer->split_string(dir.p1))
            if (lexer->match_symbol(token, symbol))
            {
                if (symtab_map.count(symbol) > 0)
                {
                    Symtab_Entry &entry = symtab_map.at(symbol);
                    if (entry.is_equ && entry.sym.st_shndx != SHN_ABS)
                    {
                        cerr << "ERROR: Relative .equ symbol '" << symbol << "' cannot be global!\n";
                        return Result::Error;
                    }
                    int type = ELF16_ST_TYPE(entry.sym.st_info);
                    entry.sym.st_info = ELF16_ST_INFO(STB_GLOBAL, type);
                }
                else
                {
                    cerr << "ERROR: Global symbol '" << token << "' is undefined!\n";
                    return Result::Error;
                }
            }
            else
            {
                cerr << "ERROR: Invalid symbol '" << token << "'!\n";
                return Result::Error;
            }
        return Result::Success;
    }
    case Directive::Extern:
    {
        if (pass == Pass::Second) return Result::Success;
        string symbol;
        for (string token : lexer->split_string(dir.p1))
            if (lexer->match_symbol(token, symbol))
            {
                // If the symbol is already defined, ignore this directive
                if (symtab_map.count(symbol) > 0) continue;
                strtab_vect.push_back(symbol);
                Symtab_Entry entry(strtab_vect.size() - 1, 0, ELF16_ST_INFO(STB_GLOBAL, STT_NOTYPE), SHN_UNDEF);
                symtab_map.insert(symtab_pair_t(symbol, entry));
            }
            else
            {
                cerr << "ERROR: Invalid symbol '" << token << "'!\n";
                return Result::Error;
            }
        return Result::Success;
    }
    case Directive::Equ:
    case Directive::Set:
    {
        if (pass == Pass::Second) return Result::Success;
        string symbol = dir.p1;
        unique_ptr<Expression> expr(new Expression());
        if (!parser->parse_expression(dir.p2, *expr))
        {
            cerr << "ERROR: Failed to parse expression: '" << dir.p2 << "'!\n";
            return Result::Error;
        }
        int value;
        Result res;
        if ((res = process_expression(*expr, value, true, symbol)) == Result::Error)
        {
            cerr << "ERROR: Invalid expression: '" << dir.p2 <<"'!\n";
            return Result::Error;
        }
        if (symtab_map.count(symbol) > 0)
        {
            Symtab_Entry &entry = symtab_map.at(symbol);
            if (dir.code == Directive::Set || entry.sym.st_info == ELF16_ST_INFO(STB_GLOBAL, STT_NOTYPE)
                && entry.sym.st_shndx == SHN_UNDEF && entry.sym.st_value == 0)
            {
                Symtab_Entry &entry = symtab_map.at(symbol);
                entry.sym.st_info = ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE);
                entry.sym.st_shndx = SHN_UNDEF;
                entry.sym.st_value = value;
                entry.is_equ = true;
                if (res == Result::Success && entry.sym.st_shndx == SHN_UNDEF)
                {
                    entry.sym.st_shndx = SHN_ABS;
                    if (equ_uneval_map.count(symbol) > 0) equ_uneval_map.erase(symbol);
                    if (equ_reloc_map.count(symbol) > 0) equ_reloc_map.erase(symbol);
                }
                else if (res == Result::Uneval)
                    equ_uneval_map.emplace(equ_uneval_pair_t(symbol, std::move(expr)));
            }
            else
            {
                cerr << "ERROR: Symbol '" << symbol << "' already in use!\n";
                return Result::Error;
            }
        }
        else
        {
            strtab_vect.push_back(symbol);
            Symtab_Entry entry(strtab_vect.size() - 1, value, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), res != Result::Success ? SHN_UNDEF : SHN_ABS, true);
            symtab_map.insert(symtab_pair_t(symbol, entry));
            if (res == Result::Uneval) equ_uneval_map.emplace(equ_uneval_pair_t(symbol, std::move(expr)));
        }
        return Result::Success;
    }
    case Directive::Text:
    case Directive::Data:
    case Directive::Bss:
    case Directive::Section:
    {
        if (cur_sect.name != "")
        {
            lc_map[cur_sect.name] = cur_sect.loc_cnt;
            shdrtab_map.at(cur_sect.name).shdr.sh_size = cur_sect.loc_cnt;
        }

        string name = dir.p1, flags = dir.p2;

        if (dir.code != Directive::Section)
            name = "." + parser->get_directive(dir.code);

        cur_sect.name       = name;
        cur_sect.loc_cnt    = lc_map[name];

        if (cur_sect.loc_cnt == 0 && pass == Pass::First)
        {
            Elf16_Word sh_type = 0, sh_flags = 0;
            if (flags == "") // try to infer section type and flags from section name
            {
                sh_type = name == ".bss" ? SHT_NOBITS : SHT_PROGBITS;
                sh_flags = SHF_ALLOC;
                if (name == ".bss" || name == ".data") sh_flags |= SHF_WRITE;
                else if (name == ".text") sh_flags |= SHF_EXECINSTR;
                else if (name != ".rodata") // .rodata has only flags SHF_ALLOC which are set above
                {
                    cerr << "ERROR: Cannot infer section type and flags from section name: '" << name << "'\n";
                    return Result::Error;
                }
                
            }
            else // parse flags string
            {
                sh_type = SHT_PROGBITS;
                for (char c : flags)
                    if (c == 'a') sh_flags |= SHF_ALLOC;
                    else if (c == 'e') sh_type = SHT_NOBITS;
                    else if (c == 'w') sh_flags |= SHF_WRITE;
                    else if (c == 'x') sh_flags |= SHF_EXECINSTR;
            }
            add_shdr(name, sh_type, sh_flags);
            section_map[cur_sect.name]; // initialize an empty section vector
        }
        else
        {
            Shdrtab_Entry &entry    = shdrtab_map.at(cur_sect.name);
            cur_sect.type           = entry.shdr.sh_type;
            cur_sect.flags          = entry.shdr.sh_flags;
            cur_sect.shdrtab_index  = entry.index;
        }

        return Result::Success;
    }
    case Directive::End:
    {
        lc_map[cur_sect.name] = cur_sect.loc_cnt;
        shdrtab_map.at(cur_sect.name).shdr.sh_size = cur_sect.loc_cnt;
        return Result::End;
    }
    case Directive::Byte:
    {
        if (pass == Pass::First)
            cur_sect.loc_cnt += lexer->split_string(dir.p1).size() * sizeof(Elf16_Half);
        else
            for (string token : lexer->split_string(dir.p1))
            {
                Expression expr;
                if (!parser->parse_expression(token, expr))
                {
                    cerr << "ERROR: Failed to parse expression: '" << token << "'!\n";
                    return Result::Error;
                }
                int value;
                if (process_expression(expr, value, false) != Result::Success)
                {
                    cerr << "ERROR: Invalid expression: '" << token <<"'!\n";
                    return Result::Error;
                }
                if (cur_sect.type == SHT_NOBITS && value != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                push_byte(value);
            }
        return Result::Success;
    }
    case Directive::Word:
    {
        if (pass == Pass::First)
            cur_sect.loc_cnt += lexer->split_string(dir.p1).size() * sizeof(Elf16_Word);
        else
            for (string token : lexer->split_string(dir.p1))
            {
                Expression expr;
                if (!parser->parse_expression(token, expr))
                {
                    cerr << "ERROR: Failed to parse expression: '" << token << "'!\n";
                    return Result::Error;
                }
                int value;
                if (process_expression(expr, value, false) != Result::Success)
                {
                    cerr << "ERROR: Invalid expression: '" << token <<"'!\n";
                    return Result::Error;
                }
                if (cur_sect.type == SHT_NOBITS && value != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                push_word(value);
            }
        return Result::Success;
    }
    case Directive::Align:
        {
        if (cur_sect.name == "") return Result::Error;
        if (dir.p1 == "")
        {
            cerr << "ERROR: Empty alignment size parameter!\n";
            return Result::Error;
        }
        Elf16_Half alignment;
        if (!parser->decode_byte(dir.p1, alignment))
        {
            cerr << "ERROR: Failed to decode: '" << dir.p1 << "' as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half fill;
        if (dir.p2 == "") fill = 0x00;
        else if (!parser->decode_byte(dir.p2, fill))
        {
            cerr << "ERROR: Failed to decode: '" << dir.p2 << "' as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half max;
        if (dir.p3 == "") max = alignment;
        else if (!parser->decode_byte(dir.p3, max))
        {
            cerr << "ERROR: Failed to decode: '" << dir.p3 << "' as a byte value!\n";
            return Result::Error;
        }
        if (!alignment || alignment & (alignment - 1))
        {
            cerr << "ERROR: Value: " << alignment << " is not a power of two! Cannot apply alignment!\n";
            return Result::Error;
        }
        Elf16_Word remainder = cur_sect.loc_cnt & (alignment - 1);
        if (remainder)
        {
            unsigned size = alignment - remainder;
            if (size > max)
            {
                cerr << "ERROR: Required fill: " << size << " is larger than max allowed: " << (unsigned) max << "! Cannot apply alignment!\n";
                return Result::Error;
            }
            if (pass == Pass::First) cur_sect.loc_cnt += size;
            else for (unsigned i = 0; i < size; ++i) push_byte(fill);
        }
        return Result::Success;
    }
    case Directive::Skip:
    {
        if (dir.p1 == "")
        {
            cerr << "ERROR: Empty skip size parameter!\n";
            return Result::Error;
        }
        Elf16_Half size;
        if (!parser->decode_byte(dir.p1, size))
        {
            cerr << "ERROR: Failed to decode: '" << dir.p1 << "' as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half fill;
        if (dir.p2 == "") fill = 0x00;
        else if (!parser->decode_byte(dir.p2, fill))
        {
            cerr << "ERROR: Failed to decode: '" << dir.p2 << "' as a byte value!\n";
            return Result::Error;
        }
        if (pass == Pass::First) cur_sect.loc_cnt += size;
        else for (unsigned i = 0; i < size; ++i) push_byte(fill);
        return Result::Success;
    }
    default: return Result::Error;
    }
}

Result Assembler::process_instruction(const Instruction &instr)
{
    if (!(cur_sect.flags & SHF_EXECINSTR))
    {
        cerr << "ERROR: Code in unexecutable section: '" << cur_sect.name << "'!\n";
        return Result::Error;
    }
    if (instr.op_cnt == 0)
    {   // zero-address instructions
        if (pass == Pass::First) cur_sect.loc_cnt += sizeof(Elf16_Half);
        else push_byte(instr.code << 3);
        return Result::Success;
    }
    else if (instr.op_cnt == 1)
    {   // one-address instructions
        int op_size = get_operand_code_size(instr.op1, instr.op_size);
        if (op_size < 1) return Result::Error;
        Elf16_Addr next_instr = cur_sect.loc_cnt + sizeof(Elf16_Half) + op_size;
        if (pass == Pass::First) cur_sect.loc_cnt = next_instr;
        else
        {
            Elf16_Half opcode = instr.code << 3;
            if (instr.op_size == Operand_Size::Word) opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
            push_byte(opcode);
            if (!insert_operand(instr.op1, instr.op_size, next_instr)) return Result::Error;
        }
        return Result::Success;
    }
    else if (instr.op_cnt == 2)
    {   // two-address instructions
        int op1_size = get_operand_code_size(instr.op1, instr.op_size);
        if (op1_size < 1) return Result::Error;
        int op2_size = get_operand_code_size(instr.op2, instr.op_size);
        if (op2_size < 1) return Result::Error;
        Elf16_Addr next_instr = cur_sect.loc_cnt + sizeof(Elf16_Half) + op1_size + op2_size;
        if (pass == Pass::First) cur_sect.loc_cnt = next_instr;
        else
        {
            Elf16_Half opcode = instr.code << 3;
            if (instr.op_size == Operand_Size::Word) opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
            push_byte(opcode);
            if (!insert_operand(instr.op1, instr.op_size, next_instr)) return Result::Error;
            if (!insert_operand(instr.op2, instr.op_size, next_instr)) return Result::Error;
        }
        return Result::Success;
    }
    return Result::Error;
}

Result Assembler::process_expression(const Expression &expr, int &value, bool allow_undef, const string &equ_name)
{
    typedef struct { int value, clidx, shndx; } operand_t; // clidx: 0 = ABS, 1 = REL, other = INVALID
    typedef Operator_Token operator_t;
    stack<operand_t> values;
    stack<operator_t> ops;
    int rank = 0;
    value = 0;
    for (auto &token : expr)
    {
        if (token->type == Expression_Token::Operator)
        {
            auto &op = static_cast<Operator_Token&>(*token);
            if (op.op_type == Operator_Token::Open)
                ops.push(op);
            else
            {
                while (!ops.empty() && (
                    op.op_type == Operator_Token::Close && ops.top().op_type != Operator_Token::Open || 
                    op.op_type != Operator_Token::Close && ops.top().priority() >= op.priority()))
                {
                    operand_t val2 = values.top();
                    values.pop();
                    operand_t val1 = values.top();
                    values.pop();
                    Operator_Token oper = ops.top();
                    ops.pop();
                    operand_t result;
                    result.shndx = oper.get_st_shndx(val1.shndx, val2.shndx);
                    // Any bad combination of operator, val1 section id, val2 section id
                    // returns -1 which means its incompatible!
                    if (result.shndx == -1)
                    {
                        cerr << "ERROR: Invalid operands (*" << get_section_name(val1.shndx) << "* and *"
                                << get_section_name(val2.shndx) << "* sections) for operator '" << oper.get_symbol() << "'!\n";
                        return Result::Error;
                    }
                    result.clidx = oper.get_clidx(val1.clidx, val2.clidx);
                    result.value = oper.calculate(val1.value, val2.value);
                    values.push(result);
                    rank--;
                    if (rank < 1) return Result::Error;
                }
                if (op.op_type == Operator_Token::Close)
                    ops.pop(); // Pop opening brace
                else
                    ops.push(op); // Push current operator
            }
        }
        else if (token->type == Expression_Token::Number)
        {
            auto &num = static_cast<Number_Token&>(*token);
            operand_t op;
            op.value = num.value;
            op.clidx = 0; // absolute value
            op.shndx = SHN_ABS; // absolute value
            values.push(op);
            rank++;
        }
        else
        {
            auto &sym = static_cast<Symbol_Token&>(*token);
            Symtab_Entry entry;
            if (!get_symtab_entry(sym.name, entry, allow_undef))
            {
                if (allow_undef) return Result::Uneval;
                return Result::Error;
            }
            operand_t op;
            op.value = ELF16_ST_BIND(entry.sym.st_info) == STB_LOCAL ? entry.sym.st_value : 0;
            op.clidx = (entry.sym.st_shndx == SHN_ABS ? 0 : 1);
            op.shndx = entry.sym.st_shndx;
            values.push(op);
            rank++;
        }
    }
    while (!ops.empty())
    {
        operand_t val2 = values.top();
        values.pop();
        operand_t val1 = values.top();
        values.pop();
        Operator_Token oper = ops.top();
        ops.pop();
        operand_t result;
        result.shndx = oper.get_st_shndx(val1.shndx, val2.shndx);
        // Any bad combination of operator, val1 section id, val2 section id
        // returns -1 which means its incompatible!
        if (result.shndx == -1)
        {
            cerr << "ERROR: Invalid operands (*" << get_section_name(val1.shndx) << "* and *"
                    << get_section_name(val2.shndx) << "* sections) for operator '" << oper.get_symbol() << "'!\n";
            return Result::Error;
        }
        result.clidx = oper.get_clidx(val1.clidx, val2.clidx);
        result.value = oper.calculate(val1.value, val2.value);
        values.push(result);
        rank--;
    }
    if (rank != 1) return Result::Error;
    operand_t result = values.top();
    values.pop();
    if (result.clidx == 0) value = result.value;
    else if (result.clidx == 1)
    {
        if (!equ_name.empty())
        {
            vector<Reltab_Entry> reloc_vect;
            for (auto &token : expr)
                if (token->type == Expression_Token::Symbol)
                {
                    auto &sym = static_cast<Symbol_Token&>(*token);
                    if (!insert_reloc(sym.name, R_VN_16, 0, false, &reloc_vect))
                    {
                        cerr << "ERROR: Failed to insert .equ reloc for: '" << sym.name << "'!\n";
                        return Result::Error;
                    }
                }
            equ_reloc_map.insert(equ_relocs_pair_t(equ_name, reloc_pair_t(result.value, reloc_vect)));
            return Result::Reloc;
        }
        else
        {
            for (auto &token : expr)
                if (token->type == Expression_Token::Symbol)
                {
                    auto &sym = static_cast<Symbol_Token&>(*token);
                    if (!insert_reloc(sym.name, R_VN_16, 0, false))
                    {
                        cerr << "ERROR: Failed to insert reloc for: '" << sym.name << "'!\n";
                        return Result::Error;
                    }
                }
            value = result.value;
        }
    }
    else
    {
        cerr << "ERROR: Invalid class index: " << result.clidx << "!\n";
        return Result::Error;
    }
    return Result::Success;
}

bool Assembler::get_symtab_entry(const string &str, Symtab_Entry &entry, bool silent)
{
    if (symtab_map.count(str) == 0)
    {
        if (!silent)
            cerr << "ERROR: Undefined reference to: '" << str << "'!\n";
        return false;
    }
    entry = symtab_map.at(str);
    return true;
}

string Assembler::get_section_name(unsigned shndx)
{
    if (shndx == SHN_UNDEF)
        return "UND";
    else if (shndx == SHN_ABS)
        return "ABS";
    else
        return shstrtab_vect[shndx];
}

int Assembler::get_operand_code_size(const string &str, uint8_t expected_size)
{
    if (lexer->match_operand_1b(str)) return 1;
    string offset_str;
    if (!lexer->match_operand_2b(str, offset_str)) return 3;
    string symbol_str;
    uint8_t offset;
    if (lexer->match_symbol(offset_str.substr(offset_str[0] == '&'), symbol_str))
        return 1 + expected_size; // cannot verify symbol now, just use expected size
    else if (parser->decode_byte(offset_str, offset))
    {
        if (offset == 0) return 1; // if offset is zero assume regind without offset
        return 2;
    }
    return 3;
}

bool Assembler::add_symbol(const string &symbol)
{
    bool exists = symtab_map.count(symbol) > 0;

    Elf16_Word name = 0;
    Elf16_Half type = STT_NOTYPE;

    if (symbol == cur_sect.name) type = STT_SECTION;
    else
    {
        if (exists) name = symtab_map.at(symbol).sym.st_name;
        else
        {
            strtab_vect.push_back(symbol);
            name = strtab_vect.size() - 1;
        }
        if (cur_sect.flags & SHF_EXECINSTR) type = STT_FUNC;
        else if (cur_sect.flags & SHF_ALLOC) type = STT_OBJECT;
    }

    if (exists)
    {
        Symtab_Entry &entry = symtab_map.at(symbol);
        if (entry.sym.st_value == 0 && entry.sym.st_shndx == SHN_UNDEF
            && entry.sym.st_info == ELF16_ST_INFO(STB_GLOBAL, STT_NOTYPE))
        {   // extern global symbol
            entry.is_equ = false;
            entry.sym.st_info = ELF16_ST_INFO(STB_LOCAL, type);
            entry.sym.st_shndx = cur_sect.shdrtab_index;
            entry.sym.st_value = cur_sect.loc_cnt;
            return true;
        }
        else
        {
            cerr << "ERROR: Symbol '" << symbol << "' already in use!\n";
            return false;
        }
    }

    Symtab_Entry entry(name, cur_sect.loc_cnt, ELF16_ST_INFO(STB_LOCAL, type), cur_sect.shdrtab_index);
    symtab_map.insert(symtab_pair_t(symbol, entry));

    return true;
}

bool Assembler::add_shdr(const string &name, Elf16_Word type, Elf16_Word flags, bool reloc, Elf16_Word info, Elf16_Word entsize)
{
    if (shdrtab_map.count(name) > 0)
        return true;

    Shdrtab_Entry entry(type, flags, info, entsize);
    shdrtab_map.insert(shdrtab_pair_t(name, entry));
    shstrtab_vect.push_back(name);

    if (!reloc)
    {
        cur_sect.type           = entry.shdr.sh_type;
        cur_sect.flags          = entry.shdr.sh_flags;
        cur_sect.shdrtab_index  = entry.index;
        if (!add_symbol(cur_sect.name)) return false;
    }

    return true;
}

void Assembler::push_byte(Elf16_Half byte)
{
    section_map.at(cur_sect.name).push_back(byte);
    cur_sect.loc_cnt += sizeof(Elf16_Half);
}

void Assembler::push_word(Elf16_Word word)
{
    section_map.at(cur_sect.name).push_back(word & 0xff); // little-endian
    section_map.at(cur_sect.name).push_back(word >> 8);
    cur_sect.loc_cnt += sizeof(Elf16_Word);
}

bool Assembler::insert_operand(const string &str, uint8_t size, Elf16_Addr next_instr)
{
    if (size == Operand_Size::None) return false;
    string token1, token2;
    if (size == Operand_Size::Byte)
    {
        if (lexer->match_imm_b(str, token1))
        {
            push_byte(Addressing_Mode::Imm);
            if (token1[0] == '&')
            {
                Symtab_Entry entry;
                if (!get_symtab_entry(token1, entry)) return false;
                if (entry.sym.st_shndx != SHN_ABS)
                {
                    cerr << "ERROR: Symbol: '" << token1 << "' is not an absolute symbol and cannot be used for byte-immediate addressing!\n";
                    return false;
                }
                int16_t value = entry.sym.st_value;
                push_byte(value & 0xff);
                if (value >= -128 && value <= 127) return true;
                cerr << "ERROR: Value of absolute symbol: '" << token1 << "' is greater than a byte value and cannot be used for byte-immediate addressing!\n";
                return false;
            }
            else
            {
                Elf16_Half byte;
                if (!parser->decode_byte(token1, byte))
                {
                    cerr << "ERROR: Failed to decode: '" << token1 << "' as a byte value!\n";
                    return false;
                }
                push_byte(byte);
                return true;
            }
        }
        else if (lexer->match_regdir_b(str, token1))
        {
            Elf16_Half opdesc = Addressing_Mode::RegDir;
            opdesc |= (token1[1] - '0') << 1;
            if (token1[2] == 'h') opdesc |= 0x1;
            push_byte(opdesc);
            return true;
        }
    }
    else
    {
        if (lexer->match_imm_w(str, token1))
        {
            push_byte(Addressing_Mode::Imm);
            if (token1[0] == '&')
            {
                if (insert_reloc(token1.substr(1), R_VN_16, next_instr)) return true;
            }
            else
            {
                Elf16_Word word;
                if (!parser->decode_word(token1, word))
                {
                    cerr << "ERROR: Failed to decode: '" << token1 << "' as a word value!\n";
                    return false;
                }
                push_word(word);
                return true;
            }
        }
        else if (lexer->match_regdir_w(str, token1))
        {
            Elf16_Half opdesc;
            if (!parser->decode_register(token1, opdesc))
            {
                cerr << "ERROR: Invalid register: '" << token1 << "'!\n";
                return false;
            }
            opdesc |= Addressing_Mode::RegDir;
            push_byte(opdesc);
            return true;
        }
    }
    if (lexer->match_regind(str, token1))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: '" << token1 << "'!\n";
            return false;
        }
        opdesc |= Addressing_Mode::RegInd;
        push_byte(opdesc);
        return true;
    }
    else if (lexer->match_regindoff(str, token1, token2))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: '" << token1 << "'!\n";
            return false;
        }
        Elf16_Half byteoff;
        Elf16_Word wordoff;
        if (parser->decode_byte(token2, byteoff))
        {
            if (byteoff == 0)
            {
                opdesc |= Addressing_Mode::RegInd; // zero-offset = regind without offset
                push_byte(opdesc);
            }
            else
            {
                opdesc |= Addressing_Mode::RegIndOff8; // 8-bit offset
                push_byte(opdesc);
                push_byte(byteoff);
            }
        }
        else if (parser->decode_word(token2, wordoff))
        {
            opdesc |= Addressing_Mode::RegIndOff16; // 16-bit offset
            push_byte(opdesc);
            push_word(wordoff);
        }
        else
        {
            cerr << "ERROR: Failed to decode: '" << token2 << "' as a byte or word value!\n";
            return false;
        }
        return true;
    }
    else if (lexer->match_regindsym(str, token1, token2))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: '" << token1 << "'!\n";
            return false;
        }
        opdesc |= Addressing_Mode::RegIndOff16;
        push_byte(opdesc);
        Symtab_Entry entry;
        if (!get_symtab_entry(token2, entry)) return false;
        if (entry.sym.st_shndx != SHN_ABS)
        {
            cerr << "ERROR: Relative symbol: '" << token2 << "' cannot be used as an offset for register indirect addressing!\n";
            return false;
        }
        push_byte(entry.sym.st_value & 0xff);
        int16_t value = entry.sym.st_value;
        if (value >= -128 && value <= 127) return true;
        push_byte(entry.sym.st_value >> 8);
        return true;
    }
    else if (lexer->match_memsym(str, token1))
    {
        bool pcrel = token1[0] == '$';
        push_byte(pcrel ? Addressing_Mode::RegIndOff16 | 7 << 1 : Addressing_Mode::Mem);
        if (insert_reloc(pcrel ? token1.substr(1) : token1, pcrel ? R_VN_PC16 : R_VN_16, next_instr)) return true;
    }
    else if (lexer->match_memabs(str, token1))
    {
        Elf16_Word address;
        if (!parser->decode_word(token1, address))
        {
            cerr << "ERROR: Invalid address: '" << token1 << "'!\n";
            return false;
        }
        push_byte(Addressing_Mode::Mem);
        push_word(address);
        return true;
    }
    cerr << "ERROR: Invalid operand: '" << str << "'!\n";
    return false;
}

bool Assembler::insert_reloc(const string &symbol, Elf16_Half type, Elf16_Addr next_instr, bool place, std::vector<Reltab_Entry> *relocs_vect)
{
    Symtab_Entry entry;
    if (!get_symtab_entry(symbol, entry)) return false;
    int value;
    if (entry.sym.st_shndx == SHN_ABS)
    {
        if (type == R_VN_16) value = entry.sym.st_value;
        else
        {
            cerr << "ERROR: Absolute symbol: '" << symbol << "' cannot be used for memory addressing!\n";
            return false;
        }
    }
    else
    {
        bool global = ELF16_ST_BIND(entry.sym.st_info) == STB_GLOBAL;
        if (type == R_VN_PC16 && !global && entry.sym.st_shndx == cur_sect.shdrtab_index)
            value = entry.sym.st_value - next_instr;
        else
        {
            if (entry.is_equ && equ_reloc_map.count(symbol) == 0)
                return false; // relocs map vector not yet defined, wait for next try
            if (relocs_vect == nullptr)
            {
                string relshdr = ".rel" + cur_sect.name;
                add_shdr(relshdr, SHT_REL, SHF_INFO_LINK, true, shdrtab_map.at(cur_sect.name).index, sizeof(Elf16_Rel));
                if (entry.is_equ)
                {
                    value = equ_reloc_map.at(symbol).first;
                    for (auto reloc : equ_reloc_map.at(symbol).second)
                        reltab_map[cur_sect.name].push_back(Reltab_Entry(reloc.rel.r_info, cur_sect.loc_cnt));
                    shdrtab_map.at(relshdr).shdr.sh_size += equ_reloc_map.at(symbol).second.size() * sizeof(Elf16_Rel);
                }
                else
                {
                    value = global ? 0 : entry.sym.st_value;
                    reltab_map[cur_sect.name].push_back(Reltab_Entry(ELF16_R_INFO(global ? entry.index : symtab_map.at(shstrtab_vect[entry.sym.st_shndx]).index, type), cur_sect.loc_cnt));
                    shdrtab_map.at(relshdr).shdr.sh_size += sizeof(Elf16_Rel);
                }
                if (type == R_VN_PC16) value += cur_sect.loc_cnt - next_instr;
            }
            else if (entry.is_equ)
                for (auto reloc : equ_reloc_map.at(symbol).second)
                    relocs_vect->push_back(Reltab_Entry(reloc.rel.r_info, cur_sect.loc_cnt));
            else
                relocs_vect->push_back(Reltab_Entry(ELF16_R_INFO(global ? entry.index : symtab_map.at(shstrtab_vect[entry.sym.st_shndx]).index, type), cur_sect.loc_cnt));
        }
    }
    if (place) push_word(value);
    return true;
}