#include "assembler.h"

#include <iostream>

using std::cerr;
using std::cout;
using std::ifstream;
using std::ios;
using std::make_pair;
using std::string;

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
    symtab_map.insert(Symtab_Pair("", dummySym));
    symtab_vect.push_back(dummySym);
    strtab_vect.push_back("");

    // Inserting a dummy section header
    Shdrtab_Entry dummyShdr(SHT_NULL, 0, 0);
    shdrtab_map.insert(Shdrtab_Pair("", dummyShdr));
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

    cout << "lc_map:\n";
    for (auto it = lc_map.begin(); it != lc_map.end(); ++it)
        cout << "Section name:\t" << it->first << "\tLocation counter:\t" << it->second << '\n';

    cout << "symtab_map:\n";
    for (auto it = symtab_map.begin(); it != symtab_map.end(); ++it)
        cout << it->first << "\t->" << it->second.index << "\t= "
             << it->second.sym.st_name << ':' << (int)it->second.sym.st_value << ':'
             << it->second.sym.st_size << ':' << ELF16_ST_BIND(it->second.sym.st_info)
             << ':' << ELF16_ST_TYPE(it->second.sym.st_info) << ':' << it->second.sym.st_shndx << '\n';

    cout << "shdrtab_map:\n";
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        cout << "Section name:" << it->first << "\t\tIndex:\t" << it->second.index << "\tSection type:\t" << it->second.shdr.sh_type << "\tSection flags:\t" << it->second.shdr.sh_flags << "\tSection size:\t" << it->second.shdr.sh_size << "\tSection info:\t" << it->second.shdr.sh_info << "\tLink:\t" << it->second.shdr.sh_link << '\n';

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

void Assembler::write_output()
{
    if (output.is_open())
        output.close();
    // THIS NEEDS TO BE SET BEFORE WRITING TO OUTPUT FILE!!!
    // shdrtab_map.at(relshdr).shdr.sh_link    = shdrtab_map.at(".strtab").index;
    if (binary)
    {
        output.open(output_file, ios::out | ios::binary);
        // not implemented yet
    }
    else
    {
        output.open(output_file, ifstream::out);

        // temporary
        for (auto it = section_map.begin(); it != section_map.end(); ++it)
        {
            output << "Section: " << it->first << '\n';
            if (it->first == ".text")
                for (unsigned i = 0; i < it->second.size(); ++i)
                {
                    Elf16_Half byte = it->second[i];
                    for (unsigned j = 0; j < 8; ++j)
                        output << (bool) (byte & (0x80 >> j));
                    output << (i % 4 ? ' ' : '\n');
                }
            else
                for (unsigned i = 0; i < it->second.size(); ++i)
                    output << (unsigned) (it->second[i]) << ' ';
            output << '\n';
        }
    }
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
                    symtab_map.insert(Symtab_Pair(symbol, entry));
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
        // temporary, need to implement expression parsing
        string symbol = dir.p1, expr = dir.p2;
        uint16_t word;
        if (!parser->decode_word(expr, word))
        {
            cerr << "ERROR: Failed to decode: \"" << expr << "\" as a word value!\n";
            return Result::Error;
        }
        if (symtab_map.count(symbol) > 0)
        {
            if (dir.code == Directive::Set) symtab_map.at(symbol).sym.st_value = word;
            else
            {
                cerr << "ERROR: Symbol \"" << symbol << "\" already in use!\n";
                return Result::Error;
            }
        }
        else
        {
            strtab_vect.push_back(symbol);
            Symtab_Entry entry(strtab_vect.size() - 1, word, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), SHN_ABS);
            symtab_map.insert(Symtab_Pair(symbol, entry));
            symtab_vect.push_back(entry);
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
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!\n";
            return Result::Error;
        }
        Elf16_Half byte;
        for (string token : lexer->split_string(dir.p1))
            if (parser->decode_byte(token, byte))
            {
                if (cur_sect.type == SHT_NOBITS && byte != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                if (pass == Pass::Second)
                    section_map.at(cur_sect.name).push_back(byte);
                cur_sect.loc_cnt += sizeof(Elf16_Half);
            }
            else
            {
                cerr << "ERROR: Failed to decode: \"" << token << "\" as a byte value!\n";
                return Result::Error;
            }
        return Result::Success;
    }
    case Directive::Word:
    {
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!" << cur_sect.name << "\n";
            return Result::Error;
        }
        Elf16_Word word;
        for (string token : lexer->split_string(dir.p1))
            if (parser->decode_word(token, word))
            {
                if (cur_sect.type == SHT_NOBITS && word != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Result::Error;
                }
                if (pass == Pass::Second)
                {
                    section_map.at(cur_sect.name).push_back(word & 0x00ff); // little-endian
                    section_map.at(cur_sect.name).push_back(word >> 8);
                }
                cur_sect.loc_cnt += sizeof(Elf16_Word);
            }
            else
            {
                cerr << "ERROR: Failed to decode: \"" << token << "\" as a word value!\n";
                return Result::Error;
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
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!\n";
            return Result::Error;
        }
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
    symtab_map.insert(Symtab_Pair(symbol, entry));
    symtab_vect.push_back(entry);

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
    shdrtab_map.insert(Shdrtab_Pair(name, entry));
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
                if (add_reloc(token1.substr(1), R_VN_16, next_instr))
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
                section_map.at(cur_sect.name).push_back(word & 0x00ff); // little-endian
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
            section_map.at(cur_sect.name).push_back(wordoff & 0x00ff); // little-endian
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
        if (add_reloc(token2, R_VN_16, next_instr))
            return true;
    }
    else if (lexer->match_memsym(str, token1))
    {
        bool pcrel = token1[0] == '$';
        if (pcrel) section_map.at(cur_sect.name).push_back(Addressing_Mode::RegIndOff16 | 7 << 1);
        else section_map.at(cur_sect.name).push_back(Addressing_Mode::Mem);
        cur_sect.loc_cnt++;
        if (add_reloc(pcrel ? token1.substr(1) : token1, pcrel ? R_VN_PC16 : R_VN_16, next_instr))
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
        section_map.at(cur_sect.name).push_back(address & 0x00ff); // little-endian
        section_map.at(cur_sect.name).push_back(address >> 8);
        cur_sect.loc_cnt += 3;
        return true;
    }
    cerr << "ERROR: Invalid operand: \"" << str << "\"!\n";
    return false;
}

bool Assembler::add_reloc(const std::string &symbol, Elf16_Half type, Elf16_Addr next_instr)
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
    section_map.at(cur_sect.name).push_back(value & 0x00ff); // little-endian
    section_map.at(cur_sect.name).push_back(value >> 8);
    cur_sect.loc_cnt += 2;
    return true;
}