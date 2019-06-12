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
using std::vector;

Elf16_Addr Symtab_Entry::symtab_index   = 0;
Elf16_Addr Shdrtab_Entry::shdrtab_index = 0;

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
    cur_sect.shdrtab_index  = 1;
    cur_sect.loc_cnt        = 0;

    // Inserting a dummy symbol
    Symtab_Entry dummySym(0, 0, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), SHN_UNDEF);
    symtab_map.insert(symtab_pair_t("", dummySym));
    symtab_vect.push_back(dummySym.sym);
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
    cur_sect.shdrtab_index = 0;
    cur_sect.loc_cnt = 0;
    lc_map.clear();

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
        out << setw(20) << setfill(' ') << left << shstrtab_vect[shdrtab_vect[i].sh_name] << ' ';
        out << setw(20) << setfill(' ') << left;
        switch (shdrtab_vect[i].sh_type)
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
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i].sh_addr << "      ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i].sh_offset << "\n       ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i].sh_size << "      ";
        out << setw(4) << setfill('0') << right << hex << (unsigned) shdrtab_vect[i].sh_entsize << "       ";
        flags.clear();
        if (shdrtab_vect[i].sh_flags & SHF_WRITE) flags.push_back('W');
        if (shdrtab_vect[i].sh_flags & SHF_ALLOC) flags.push_back('A');
        if (shdrtab_vect[i].sh_flags & SHF_EXECINSTR) flags.push_back('X');
        if (shdrtab_vect[i].sh_flags & SHF_INFO_LINK) flags.push_back('I');
        out << setw(7) << setfill(' ') << left << flags;
        out << setw(7) << setfill(' ') << left << dec << (unsigned) shdrtab_vect[i].sh_link;
        out << setw(7) << setfill(' ') << left << dec << (unsigned) shdrtab_vect[i].sh_info;
        out << left << dec << (shdrtab_vect[i].sh_addralign == 0 ? 1 : 2 << shdrtab_vect[i].sh_addralign - 1) << '\n';
    }
    out << "Key to Flags:\n  W (write), A (alloc), X (execute), I (info)\n";

    for (auto it = shdrtab_vect.begin(); it != shdrtab_vect.end(); ++it)
    {
        string name = shstrtab_vect[it->sh_name];
        switch (it->sh_type)
        {
        case SHT_NULL: break;   // Only section header, no data
        case SHT_PROGBITS:
        {
            vector<Elf16_Half> &data = section_map[name];
            out << "\nContents of section '" << name << "':\n";
            for (unsigned i = 0; i < data.size(); ++i)
                out << setw(2) << setfill('0') << right << hex << (unsigned) data[i]
                    << (((i + 1) % 20 && (i + 1) < data.size()) ? ' ' : '\n');
        }
        case SHT_SYMTAB:
        {
            if (name != ".symtab") break;
            out << "\nSymbol table '.symtab' contains " << symtab_vect.size() << " entries:\n"
                << "  Num: Value  Size   Type       Bind       Ndx  Name\n";
            for (unsigned i = 0; i < symtab_vect.size(); ++i)
            {
                out << setw(5) << setfill(' ') << right << i << ": ";
                out << setw(4) << setfill('0') << right << hex << (unsigned) symtab_vect[i].st_value << "   ";
                out << setw(7) << setfill(' ') << left << dec << (unsigned) symtab_vect[i].st_size;
                out << setw(11) << setfill(' ') << left;
                switch (ELF16_ST_TYPE(symtab_vect[i].st_info))
                {
                case STT_NOTYPE: out << "NOTYPE"; break;
                case STT_OBJECT: out << "OBJECT"; break;
                case STT_FUNC: out << "FUNC"; break;
                case STT_SECTION: out << "SECTION"; break;
                case STT_FILE: out << "FILE"; break;
                default: out << "unknown"; break;
                }
                out << setw(11) << setfill(' ') << left;
                switch (ELF16_ST_BIND(symtab_vect[i].st_info))
                {
                case STB_LOCAL: out << "LOCAL"; break;
                case STB_GLOBAL: out << "GLOBAL"; break;
                case STB_WEAK: out << "WEAK"; break;
                default: out << "unknown"; break;
                }
                out << setw(5) << setfill(' ') << left << dec;
                if (symtab_vect[i].st_shndx == SHN_UNDEF)
                    out << "UND";
                else if (symtab_vect[i].st_shndx == SHN_ABS)
                    out << "ABS";
                else
                    out << (unsigned) symtab_vect[i].st_shndx;
                out << strtab_vect[symtab_vect[i].st_name];
                out << '\n';
            }
            break;
        }
        case SHT_STRTAB:
        {
            if (name == ".strtab")
            {
                out << "\nString table '.strtab' contains " << strtab_vect.size() << " entries:\n";
                for (unsigned i = 0, offset = it->sh_offset; i < strtab_vect.size(); offset += (strtab_vect[i++].length() + 1))
                    out << "  " << setw(4) << setfill('0') << right << hex << offset << ": " << strtab_vect[i] << '\n';
            }
            else if (name == ".shstrtab")
            {
                out << "\nString table '.shstrtab' contains " << shstrtab_vect.size() << " entries:\n";
                for (unsigned i = 0, offset = it->sh_offset; i < shstrtab_vect.size(); offset += (shstrtab_vect[i++].length() + 1))
                    out << "  " << setw(4) << setfill('0') << right << hex << offset << ": " << shstrtab_vect[i] << '\n';
            }
            break;
        }
        case SHT_NOBITS: break; // Only section header, uninitialized data
        case SHT_REL:
        {
            out << "\nRelocation section '" << name << "' contains " << (unsigned) (it->sh_size / it->sh_entsize) << " entries:\n"
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
                Elf16_Sym sym = symtab_vect[ELF16_R_SYM(reloc[i].rel.r_info)];
                bool is_section = ELF16_ST_TYPE(sym.st_info) == STT_SECTION;
                if (is_section) out << left << shstrtab_vect[sym.st_shndx];
                else out << "                     " << strtab_vect[sym.st_name];
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

    // Generate section header table
    if (shdrtab_vect.capacity() < Shdrtab_Entry::shdrtab_index)
        shdrtab_vect.resize(Shdrtab_Entry::shdrtab_index);
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        shdrtab_vect[it->second.index] = it->second.shdr;

    // Link relocation tables to the symbol table
    for (unsigned i = 0; i < shdrtab_vect.size(); ++i)
        if (shdrtab_vect[i].sh_type == SHT_REL)
            shdrtab_vect[i].sh_link = symtab_entry.index;

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
        output.open(output_file, ios::out | ios::binary);
        // not implemented yet
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
    info.loc_cnt = cur_sect.loc_cnt;
    if (info.line.label.empty() && info.line.content_type == Content_Type::None)
        return Result::Success; // empty line, skip
    if (pass == Pass::First)
        file_vect.push_back(info);
    if (!info.line.label.empty())
    {
        if (pass == Pass::First)
            if (!add_symbol(info.line.label))
                return Result::Error;
        if (info.line.content_type == Content_Type::None)
            return Result::Success;
    }
    if (info.line.content_type == Content_Type::Directive)
        return process_directive(info.line.getDir());
    else
        return process_instruction(info.line.getInstr());
}

Result Assembler::process_directive(const Directive &dir)
{
    switch (dir.code)
    {
    case Directive::Global:
    case Directive::Extern:
    {
        if (pass == Pass::First) return Result::Success;
        string symbol;
        for (string token : lexer->split_string(dir.p1))
            if (lexer->match_symbol(token, symbol))
            {
                if (symtab_map.count(symbol) > 0)
                {
                    int type = ELF16_ST_TYPE(symtab_map.at(symbol).sym.st_info);
                    symtab_map.at(symbol).sym.st_info = ELF16_ST_INFO(STB_GLOBAL, type);
                }
                else
                {
                    strtab_vect.push_back(symbol);
                    Symtab_Entry entry(strtab_vect.size() - 1, 0, ELF16_ST_INFO(STB_GLOBAL, STT_NOTYPE), SHN_UNDEF);
                    symtab_map.insert(symtab_pair_t(symbol, entry));
                    symtab_vect.push_back(entry.sym);
                }
            }
            else
            {
                cerr << "ERROR: Invalid symbol \"" << token << "\"!";
                return Result::Error;
            }
        return Result::Success;
    }
    case Directive::Equ:
    case Directive::Set:
    {
        if (pass == Pass::Second) return Result::Success;
        string symbol = dir.p1;
        Expression expr;
        if (!parser->parse_expression(dir.p2, expr))
        {
            cerr << "ERROR: Failed to parse expression: \"" << dir.p2 << "\"!\n";
            return Result::Error;
        }
        int value;
        if (process_expression(expr, value, false) == Result::Error)
        {
            cerr << "ERROR: Invalid expression: \"" << dir.p2 <<"\"!\n";
            return Result::Error;
        }
        if (symtab_map.count(symbol) > 0)
        {
            if (dir.code == Directive::Set) symtab_map.at(symbol).sym.st_value = value;
            else
            {
                cerr << "ERROR: Symbol \"" << symbol << "\" already in use!\n";
                return Result::Error;
            }
        }
        else
        {
            strtab_vect.push_back(symbol);
            Symtab_Entry entry(strtab_vect.size() - 1, value, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), SHN_ABS);
            symtab_map.insert(symtab_pair_t(symbol, entry));
            symtab_vect.push_back(entry.sym);
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
                    cerr << "ERROR: Cannot infer section type and flags from section name: \"" << name << "\"\n";
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

        cur_sect.type = shdrtab_map.at(cur_sect.name).shdr.sh_type;
        cur_sect.flags = shdrtab_map.at(cur_sect.name).shdr.sh_flags;
        cur_sect.shdrtab_index = shdrtab_map.at(cur_sect.name).index;

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
                    cerr << "ERROR: Failed to parse expression: \"" << token << "\"!\n";
                    return Result::Error;
                }
                int value;
                if (process_expression(expr, value) == Result::Error)
                {
                    cerr << "ERROR: Invalid expression: \"" << token <<"\"!\n";
                    return Result::Error;
                }
                if (cur_sect.type == SHT_NOBITS && value != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                section_map.at(cur_sect.name).push_back(value & 0xff);
                cur_sect.loc_cnt += sizeof(Elf16_Half);
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
                    cerr << "ERROR: Failed to parse expression: \"" << token << "\"!\n";
                    return Result::Error;
                }
                int value;
                if (process_expression(expr, value) == Result::Error)
                {
                    cerr << "ERROR: Invalid expression: \"" << token <<"\"!\n";
                    return Result::Error;
                }
                if (cur_sect.type == SHT_NOBITS && value != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                section_map.at(cur_sect.name).push_back(value & 0xff); // little-endian
                section_map.at(cur_sect.name).push_back(value >> 8);
                cur_sect.loc_cnt += sizeof(Elf16_Word);
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
            cerr << "ERROR: Failed to decode: \"" << dir.p1 << "\" as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half fill;
        if (dir.p2 == "") fill = 0x00;
        else if (!parser->decode_byte(dir.p2, fill))
        {
            cerr << "ERROR: Failed to decode: \"" << dir.p2 << "\" as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half max;
        if (dir.p3 == "") max = alignment;
        else if (!parser->decode_byte(dir.p3, max))
        {
            cerr << "ERROR: Failed to decode: \"" << dir.p3 << "\" as a byte value!\n";
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
            unsigned fill_size = alignment - remainder;
            if (fill_size > max)
            {
                cerr << "ERROR: Required fill: " << fill_size << " is larger than max allowed: " << (unsigned) max << "! Cannot apply alignment!\n";
                return Result::Error;
            }
            if (pass == Pass::Second)
                for (unsigned i = 0; i < fill_size; ++i)
                    section_map.at(cur_sect.name).push_back(fill);
            cur_sect.loc_cnt += fill_size;
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
            cerr << "ERROR: Failed to decode: \"" << dir.p1 << "\" as a byte value!\n";
            return Result::Error;
        }
        Elf16_Half fill;
        if (dir.p2 == "") fill = 0x00;
        else if (!parser->decode_byte(dir.p2, fill))
        {
            cerr << "ERROR: Failed to decode: \"" << dir.p2 << "\" as a byte value!\n";
            return Result::Error;
        }
        if (pass == Pass::Second)
            for (unsigned i = 0; i < size; ++i)
                section_map.at(cur_sect.name).push_back(fill);
        cur_sect.loc_cnt += size;
        return Result::Success;
    }
    default: return Result::Error;
    }
}

Result Assembler::process_instruction(const Instruction &instr)
{
    if (!(cur_sect.flags & SHF_EXECINSTR))
    {
        cerr << "ERROR: Code in unexecutable section: \"" << cur_sect.name << "\"!\n";
        return Result::Error;
    }
    cur_sect.loc_cnt += sizeof(Elf16_Half);
    if (instr.op_cnt == 0)
    {   // zero-address instructions
        if (pass == Pass::Second)
            section_map.at(cur_sect.name).push_back(instr.code << 3);
        return Result::Success;
    }
    else if (instr.op_cnt == 1)
    {   // one-address instructions
        Elf16_Addr next_instr = cur_sect.loc_cnt +
            parser->get_operand_size(instr.op1);
        if (pass == Pass::First)
            cur_sect.loc_cnt = next_instr;
        else
        {
            Elf16_Half opcode = instr.code << 3;
            if (instr.op_size == Operand_Size::Word) opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
            section_map.at(cur_sect.name).push_back(opcode);
            if (!insert_operand(instr.op1, instr.op_size, next_instr))
                return Result::Error;
        }
        return Result::Success;
    }
    else if (instr.op_cnt == 2)
    {   // two-address instructions
        Elf16_Addr next_instr = cur_sect.loc_cnt +
            parser->get_operand_size(instr.op1) +
            parser->get_operand_size(instr.op2);
        if (pass == Pass::First)
            cur_sect.loc_cnt = next_instr;
        else
        {
            Elf16_Half opcode = instr.code << 3;
            if (instr.op_size == Operand_Size::Word) opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
            section_map.at(cur_sect.name).push_back(opcode);
            if (!insert_operand(instr.op1, instr.op_size, next_instr))
                return Result::Error;
            if (!insert_operand(instr.op2, instr.op_size, next_instr))
                return Result::Error;
        }
        return Result::Success;
    }
    return Result::Error;
}

Result Assembler::process_expression(const Expression &expr, int &value, bool allow_reloc)
{
    typedef pair<int, int> operand_t; // Operand : { value, class_index (0 - absolute, 1 - relative) }
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
                    values.push(operand_t(oper.calculate(val1.first, val2.first), oper.get_class_index(val1.second, val2.second)));
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
            values.push(operand_t(num.value, 0)); // absolute value
            rank++;
        }
        else
        {
            auto &sym = static_cast<Symbol_Token&>(*token);
            if (symtab_map.count(sym.name) == 0)
            {
                cerr << "ERROR: Undefined reference to: \"" << sym.name << "\"!\n";
                return Result::Error;
            }
            Symtab_Entry entry = symtab_map.at(sym.name);
            values.push(operand_t(entry.sym.st_value, (entry.sym.st_shndx == SHN_ABS ? 0 : 1)));
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
        values.push(operand_t(oper.calculate(val1.first, val2.first), oper.get_class_index(val1.second, val2.second)));
        rank--;
    }
    if (rank != 1)
    {
        cerr << "ERROR: Invalid expression!\n";
        return Result::Error;
    }
    operand_t result = values.top();
    values.pop();
    if (result.second == 0) value = result.first;
    else if (result.second == 1)
    {
        if (!allow_reloc)
        {
            cerr << "ERROR: Expression result relocatable, but not allowed!\n";
            return Result::Error;
        }
        for (auto &token : expr)
            if (token->type == Expression_Token::Symbol)
            {
                auto &sym = static_cast<Symbol_Token&>(*token);
                if (!insert_reloc(sym.name, R_VN_16, 0, false))
                {
                    cerr << "ERROR: Failed to insert reloc for: \"" << sym.name << "\"!\n";
                    return Result::Error;
                }
            }
        value = result.first;
    }
    else
    {
        cerr << "ERROR: Invalid class index: " << result.second << "!\n";
        return Result::Error;
    }
    return Result::Success;
}

bool Assembler::add_symbol(const string &symbol)
{
    if (symtab_map.count(symbol) > 0)
    {
        cerr << "ERROR: Symbol \"" << symbol << "\" already in use!\n";
        return false;
    }

    Elf16_Word name = 0;
    Elf16_Half type = STT_NOTYPE;

    if (symbol != cur_sect.name)
    {
        strtab_vect.push_back(symbol);
        name = strtab_vect.size() - 1;
        if (cur_sect.flags & SHF_EXECINSTR) type = STT_FUNC;
        else if (cur_sect.flags & SHF_ALLOC) type = STT_OBJECT;
    }
    else type = STT_SECTION;

    Symtab_Entry entry(name, cur_sect.loc_cnt, ELF16_ST_INFO(STB_LOCAL, type), cur_sect.shdrtab_index);
    symtab_map.insert(symtab_pair_t(symbol, entry));
    symtab_vect.push_back(entry.sym);

    return true;
}

bool Assembler::add_shdr(const string &name, Elf16_Word type, Elf16_Word flags, bool reloc, Elf16_Word info, Elf16_Word entsize)
{
    if (!reloc)
        if (!add_symbol(cur_sect.name))
            return false;

    if (shdrtab_map.count(name) > 0)
        return true;

    Shdrtab_Entry entry(type, flags, info, entsize);
    shdrtab_map.insert(shdrtab_pair_t(name, entry));
    shstrtab_vect.push_back(name);

    return true;
}

bool Assembler::insert_operand(const string &str, uint8_t size, Elf16_Addr next_instr)
{
    if (size == Operand_Size::None) return false;
    string token1, token2;
    if (size == Operand_Size::Byte)
    {
        if (lexer->match_imm_b(str, token1))
        {
            Elf16_Half byte;
            if (!parser->decode_byte(token1, byte))
            {
                cerr << "ERROR: Failed to decode: \"" << token1 << "\" as a byte value!\n";
                return false;
            }
            section_map.at(cur_sect.name).push_back(Addressing_Mode::Imm);
            section_map.at(cur_sect.name).push_back(byte);
            cur_sect.loc_cnt += 2;
            return true;
        }
        else if (lexer->match_regdir_b(str, token1))
        {
            Elf16_Half opdesc = Addressing_Mode::RegDir;
            opdesc |= (token1[1] - '0') << 1;
            if (token1[2] == 'h') opdesc |= 0x1;
            section_map.at(cur_sect.name).push_back(opdesc);
            cur_sect.loc_cnt++;
            return true;
        }
    }
    else
    {
        if (lexer->match_imm_w(str, token1))
        {
            section_map.at(cur_sect.name).push_back(Addressing_Mode::Imm);
            cur_sect.loc_cnt++;
            if (token1[0] == '&')
            {
                if (insert_reloc(token1.substr(1), R_VN_16, next_instr))
                    return true;
            }
            else
            {
                Elf16_Word word;
                if (!parser->decode_word(token1, word))
                {
                    cerr << "ERROR: Failed to decode: \"" << token1 << "\" as a word value!\n";
                    return false;
                }
                section_map.at(cur_sect.name).push_back(word & 0xff); // little-endian
                section_map.at(cur_sect.name).push_back(word >> 8);
                cur_sect.loc_cnt += 2;
                return true;
            }
        }
        else if (lexer->match_regdir_w(str, token1))
        {
            Elf16_Half opdesc;
            if (!parser->decode_register(token1, opdesc))
            {
                cerr << "ERROR: Invalid register: \"" << token1 << "\"!\n";
                return false;
            }
            opdesc |= Addressing_Mode::RegDir;
            section_map.at(cur_sect.name).push_back(opdesc);
            cur_sect.loc_cnt++;
            return true;
        }
    }
    if (lexer->match_regind(str, token1))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: \"" << token1 << "\"!\n";
            return false;
        }
        opdesc |= Addressing_Mode::RegInd;
        section_map.at(cur_sect.name).push_back(opdesc);
        cur_sect.loc_cnt++;
        return true;
    }
    else if (lexer->match_regindoff(str, token1, token2))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: \"" << token1 << "\"!\n";
            return false;
        }
        Elf16_Half byteoff;
        Elf16_Word wordoff;
        if (parser->decode_byte(token2, byteoff))
        {
            if (byteoff == 0)
            {
                opdesc |= Addressing_Mode::RegInd; // zero-offset = regind without offset
                section_map.at(cur_sect.name).push_back(opdesc);
                cur_sect.loc_cnt++;
            }
            else
            {
                opdesc |= Addressing_Mode::RegIndOff8; // 8-bit offset
                section_map.at(cur_sect.name).push_back(opdesc);
                section_map.at(cur_sect.name).push_back(byteoff);
                cur_sect.loc_cnt += 2;
            }
        }
        else if (parser->decode_word(token2, wordoff))
        {
            opdesc |= Addressing_Mode::RegIndOff16; // 16-bit offset
            section_map.at(cur_sect.name).push_back(opdesc);
            section_map.at(cur_sect.name).push_back(wordoff & 0xff); // little-endian
            section_map.at(cur_sect.name).push_back(wordoff >> 8);
            cur_sect.loc_cnt += 3;
        }
        else
        {
            cerr << "ERROR: Failed to decode: \"" << token2 << "\" as a byte or word value!\n";
            return false;
        }
        return true;
    }
    else if (lexer->match_regindsym(str, token1, token2))
    {
        Elf16_Half opdesc;
        if (!parser->decode_register(token1, opdesc))
        {
            cerr << "ERROR: Invalid register: \"" << token1 << "\"!\n";
            return false;
        }
        opdesc |= Addressing_Mode::RegIndOff16;
        section_map.at(cur_sect.name).push_back(opdesc);
        cur_sect.loc_cnt++;
        if (symtab_map.count(token2) == 0)
        {
            cerr << "ERROR: Undefined reference to: \"" << token2 << "\"!\n";
            return false;
        }
        Symtab_Entry entry = symtab_map.at(token2);
        if (entry.sym.st_shndx != SHN_ABS)
        {
            cerr << "ERROR: Relative symbol: \"" << token2 << "\" cannot be used as an offset for register indirect addressing!\n";
            return false;
        }
        section_map.at(cur_sect.name).push_back(entry.sym.st_value & 0xff); // little-endian
        section_map.at(cur_sect.name).push_back(entry.sym.st_value >> 8);
        cur_sect.loc_cnt += 2;
        return true;
    }
    else if (lexer->match_memsym(str, token1))
    {
        bool pcrel = token1[0] == '$';
        if (pcrel) section_map.at(cur_sect.name).push_back(Addressing_Mode::RegIndOff16 | 7 << 1);
        else section_map.at(cur_sect.name).push_back(Addressing_Mode::Mem);
        cur_sect.loc_cnt++;
        if (insert_reloc(pcrel ? token1.substr(1) : token1, pcrel ? R_VN_PC16 : R_VN_16, next_instr))
            return true;
    }
    else if (lexer->match_memabs(str, token1))
    {
        Elf16_Word address;
        if (!parser->decode_word(token1, address))
        {
            cerr << "ERROR: Invalid address: \"" << token1 << "\"!\n";
            return false;
        }
        section_map.at(cur_sect.name).push_back(Addressing_Mode::Mem);
        section_map.at(cur_sect.name).push_back(address & 0xff); // little-endian
        section_map.at(cur_sect.name).push_back(address >> 8);
        cur_sect.loc_cnt += 3;
        return true;
    }
    cerr << "ERROR: Invalid operand: \"" << str << "\"!\n";
    return false;
}

bool Assembler::insert_reloc(const std::string &symbol, Elf16_Half type, Elf16_Addr next_instr, bool place)
{
    if (symtab_map.count(symbol) == 0)
    {
        cerr << "ERROR: Undefined reference to: \"" << symbol << "\"!\n";
        return false;
    }
    Symtab_Entry entry = symtab_map.at(symbol);
    Elf16_Word value;
    if (entry.sym.st_shndx == SHN_ABS)
    {
        if (type == R_VN_16) value = entry.sym.st_value;
        else
        {
            cerr << "ERROR: Symbol: \"" << symbol << "\" is an absolute symbol and cannot be used for memory addressing!\n";
            return false;
        }
    }
    else
    {
        string relshdr = ".rel" + cur_sect.name;
        add_shdr(relshdr, SHT_REL, SHF_INFO_LINK, true, shdrtab_map.at(cur_sect.name).index, sizeof(Elf16_Rel));
        bool global = ELF16_ST_BIND(entry.sym.st_info) == STB_GLOBAL;
        reltab_map[cur_sect.name].push_back(Reltab_Entry(ELF16_R_INFO(global ? entry.index : symtab_map.at(shstrtab_vect[entry.sym.st_shndx]).index, type), cur_sect.loc_cnt + 1));
        shdrtab_map.at(relshdr).shdr.sh_size += sizeof(Elf16_Rel);
        value = global ? 0 : entry.sym.st_value;
        if (type == R_VN_PC16) value += cur_sect.loc_cnt + 1 - next_instr;
    }
    if (!place) return true;
    section_map.at(cur_sect.name).push_back(value & 0xff); // little-endian
    section_map.at(cur_sect.name).push_back(value >> 8);
    cur_sect.loc_cnt += 2;
    return true;
}