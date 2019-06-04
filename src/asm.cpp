#include "asm.h"

#include <iostream>
#include <string>
#include <list>

using std::cerr;
using std::cout;
using std::ifstream;
using std::ios;
using std::map;
using std::regex;
using std::smatch;
using std::sregex_token_iterator;
using std::string;
using std::stoul;
using std::list;

Elf16_Addr Symtab_Entry::symtab_index   = 0;
Elf16_Addr Shdrtab_Entry::shdrtab_index = 0;

inline string lowercase(const string &str)
{
    string res = str;
    for (unsigned j = 0; j < res.length(); ++j)
        if (res[j] >= 'A' && res[j] <= 'Z')
            res[j] = (char)(res[j] - 'A' + 'a');
    return res;
}

inline list<string> tokenize(const string &str, const regex &regex)
{
    list<string> tokens;
    sregex_token_iterator it(str.begin(), str.end(), regex, -1), reg_end;
    for (; it != reg_end; ++it)
        tokens.emplace_back(it->str());
    return tokens;
}

Assembler::Assembler(const string &input_file, const string &output_file, bool binary)
{
    this->input_file    = input_file;
    this->output_file   = output_file;
    this->binary        = binary;

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
    // shdrtab_map.emplace("", dummyShdr);
    shstrtab_vect.push_back("");

    // Initializing regex
    regex_split.assign(regex_split_string, regex::icase | regex::optimize);
    regex_symbol.assign(regex_symbol_string, regex::icase | regex::optimize);
    regex_byte.assign(regex_byte_string, regex::icase | regex::optimize);
    regex_word.assign(regex_word_string, regex::icase | regex::optimize);
    regex_op1b.assign(regex_op1b_string, regex::icase | regex::optimize);
    regex_op2b.assign(regex_op2b_string, regex::icase | regex::optimize);
    regex_imm_b.assign("\\s*(" REGEX_ADR_IMM_B ")\\s*", regex::icase | regex::optimize);
    regex_imm_w.assign("\\s*(" REGEX_ADR_IMM_W ")\\s*", regex::icase | regex::optimize);
    regex_regdir_b.assign("\\s*(" REGEX_ADR_REGDIR_B ")\\s*", regex::icase | regex::optimize);
    regex_regdir_w.assign("\\s*(" REGEX_ADR_REGDIR_W ")\\s*", regex::icase | regex::optimize);
    regex_regind.assign("\\s*\\[\\s*(" REGEX_ADR_REGDIR_W ")\\s*\\]\\s*", regex::icase | regex::optimize);
    regex_regindoff.assign("\\s*(" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(" REGEX_VAL_W ")\\s*\\]\\s*", regex::icase | regex::optimize);
    regex_regindsym.assign("\\s*(" REGEX_ADR_REGDIR_W ")\\s*\\[\\s*(" REGEX_SYM ")\\s*\\]\\s*", regex::icase | regex::optimize);
    regex_memsym.assign("\\s*(\\$?)(" REGEX_SYM ")\\s*", regex::icase | regex::optimize);
    regex_memabs.assign("\\s*\\*(" REGEX_VAL_W ")\\s*", regex::icase | regex::optimize);
    for (unsigned i = 0; i < REGEX_CNT; ++i)
        regex_exprs[i].assign(regex_strings[i], regex::icase | regex::optimize);

    // Initializing string maps
    for (unsigned i = 0; i < DIRECTIVE_CNT; ++i)
        directive_map[directive[i]] = i;
    for (unsigned i = 0; i < ZEROADDRINSTR_CNT; ++i)
        zero_addr_instr_map[zero_addr_instr[i]] = i;
    for (unsigned i = 0; i < ONEADDRINSTR_CNT; ++i)
        one_addr_instr_map[one_addr_instr[i]] = i;
    for (unsigned i = 0; i < TWOADDRINSTR_CNT; ++i)
        two_addr_instr_map[two_addr_instr[i]] = i;

    // Initializing opcode map
    for (Elf16_Half oc = 0; oc < OPCODE_CNT; ++oc)
        opcode_map[opcodeMnems[oc]] = oc << 3; // OC4 OC3 OC2 OC1 OC0 (S UN UN)
}

Assembler::~Assembler()
{
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

    cur_sect.name           = "";
    cur_sect.type           = SHT_NULL;
    cur_sect.flags          = 0;
    cur_sect.shdrtab_index  = 0;
    cur_sect.loc_cnt        = 0;

    lc_map.clear();

    if (!run_second_pass())
    {
        cerr << "ERROR: Assembler failed to complete second pass!\n";
        return false;
    }

    cout << "symtab_map:\n";
    for (auto it = symtab_map.begin(); it != symtab_map.end(); ++it)
        cout << it->first << "\t->" << it->second.index << "\t= "
            << it->second.sym.st_name << ':' << (int)it->second.sym.st_value << ':'
            << it->second.sym.st_size << ':' << ELF16_ST_BIND(it->second.sym.st_info)
            << ':' << ELF16_ST_TYPE(it->second.sym.st_info) << ':' << it->second.sym.st_shndx << '\n';

    cout << "shdrtab_map:\n";
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        cout << "Section name:" << it->first << "\t\tIndex:\t" << it->second.index <<
                "\tSection type:\t" << it->second.shdr.sh_type << "\tSection flags:\t" << it->second.shdr.sh_flags <<
                "\tSection size:\t" << it->second.shdr.sh_size << "\tSection info:\t" << it->second.shdr.sh_info <<
                "\tLink:\t" << it->second.shdr.sh_link << '\n';

    cout << "lc_map:\n";
    for (auto it = lc_map.begin(); it != lc_map.end(); ++it)
        cout << "Section name:\t" << it->first << "\tLocation counter:\t" << it->second << '\n';

    write_output();

    return true;
}

bool Assembler::run_first_pass()
{
    pass = Pass::First;
    bool res = true;
    string line;
    unsigned lnum;

    if (input.is_open())
        input.close();
    input.open(input_file, ifstream::in);

    for (lnum = 1; !input.eof(); ++lnum)
    {
        getline(input, line);
        cout << lnum << ":\t" << line << '\n'; // temporary
        Parse_Result ret = parse(line);
        if (ret == Parse_Result::Success)
            continue;
        if (ret == Parse_Result::Error)
        {
            cerr << "ERROR: Failed to parse line: " << lnum << "!\n";
            res = false;
        }
        else
            cout << "End of file reached at line: " << lnum << "!\n";
        break;
    }

    input.close();
    return res;
}

bool Assembler::run_second_pass()
{
    pass = Pass::Second;
    bool res = true;
    string line;
    unsigned lnum;

    if (input.is_open())
        input.close();
    input.open(input_file, ifstream::in);

    for (lnum = 1; !input.eof(); ++lnum)
    {
        getline(input, line);
        cout << lnum << ":\t" << line << '\n'; // temporary
        Parse_Result ret = parse(line);
        if (ret == Parse_Result::Success)
            continue;
        if (ret == Parse_Result::Error)
        {
            cerr << "ERROR: Failed to parse line: " << lnum << "!\n";
            res = false;
        }
        else
            cout << "End of file reached at line: " << lnum << "!\n";
        break;
    }

    input.close();
    return true;
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

Parse_Result Assembler::parse(const string &s)
{
    smatch match;
    for (unsigned i = 0; i < REGEX_CNT; ++i)
        if (regex_match(s, match, regex_exprs[i]))
        {
            if (i == Regex_Match::Empty)
            {
                cout << "Parsed: Empty line\n";
                return Parse_Result::Success;
            }

            unsigned index = 1;
            while (index < match.size() && match.str(index).empty())
                index++;

            if (index == match.size())
                return Parse_Result::Error;

            switch (i)
            {
            case Regex_Match::Label:
                return parse_label(match, index);
            case Regex_Match::Directive:
                return parse_directive(match, index);
            case Regex_Match::ZeroAddr:
                return parse_zeroaddr(match, index);
            case Regex_Match::OneAddr:
                return parse_oneaddr(match, index);
            case Regex_Match::TwoAddr:
                return parse_twoaddr(match, index);
            }
        }
    return Parse_Result::Error;
}

Parse_Result Assembler::parse_label(const smatch &match, unsigned index)
{
    string label = match.str(index);
    string other = match.str(index + 1);

    cout << "Parsed: LABEL = " << label << " OTHER: " << other << '\n';

    if (pass == Pass::First)
        if (!add_symbol(label))
            return Parse_Result::Error; // failed to add symbol
    // else SECOND_PASS ...

    if (!other.empty())
        return parse(other); // return whatever is parsed from other

    return Parse_Result::Success;
}

Parse_Result Assembler::parse_directive(const smatch &match, unsigned index)
{
    if (match.str(index).empty())
        return Parse_Result::Error;

    Parse_Result res = Parse_Result::Success;
    string dir = lowercase(match.str(index++));

    cout << "Parsed: DIRECTIVE = " << dir;

    switch (directive_map[dir])
    {
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

        string name = match.str(index++), flags = match.str(index++);

        if (dir[0] != 's')
        {
            name = "." + dir;
            flags = "";
        }

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
                    return Parse_Result::Error;
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

        cout << " SECTION NAME: " << name << " SECTION FLAGS: " << flags;
        break;
    }
    case Directive::End:
    {
        lc_map[cur_sect.name] = cur_sect.loc_cnt;
        shdrtab_map.at(cur_sect.name).shdr.sh_size = cur_sect.loc_cnt;
        res = Parse_Result::End;
        break;
    }
    case Directive::Global:
    case Directive::Extern:
    {
        cout << " SYMBOLS: " << match.str(index++);
        if (pass == Pass::First) return Parse_Result::Success;
        global_symbol(match.str(index++));
        break;
    }
    case Directive::Byte:
    {
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!\n";
            return Parse_Result::Error;
        }
        Elf16_Half byte;
        for (string token : tokenize(match.str(index++), regex_split))
            if (decode_byte(token, byte))
            {
                cout << "Decoded byte: " << (unsigned) byte << '\n';
                if (cur_sect.type == SHT_NOBITS && byte != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Parse_Result::Error;
                }

                if (pass == Pass::Second)
                    section_map.at(cur_sect.name).push_back(byte);

                cur_sect.loc_cnt += sizeof(Elf16_Half);
            }
            else
            {
                cerr << "ERROR: Failed to decode: \"" << token << "\" as a byte value!\n";
                return Parse_Result::Error;
            }
        break;
    }
    case Directive::Word:
    {
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!" << cur_sect.name << "\n";
            return Parse_Result::Error;
        }
        Elf16_Word word;
        for (string token : tokenize(match.str(index++), regex_split))
            if (decode_word(token, word))
            {
                cout << "Decoded word: " << (unsigned) word << '\n';
                if (cur_sect.type == SHT_NOBITS && word != 0)
                {
                    cerr << "ERROR: Data cannot be initialized in .bss section!\n";
                    return Parse_Result::Error;
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
                return Parse_Result::Error;
            }
        break;
    }
    case Directive::Equ:
    case Directive::Set:
    {
        // temporary, need to implement expression parsing
        string symbol = match.str(index++), expr = match.str(index++);
        Elf16_Word word;
        if (!decode_word(expr, word))
        {
            cerr << "ERROR: Failed to decode: \"" << expr << "\" as a word value!\n";
            return Parse_Result::Error;
        }
        if (symtab_map.count(symbol) > 0)
        {
            if (directive_map[dir] == Directive::Equ)
            {
                cerr << "ERROR: Symbol \"" << symbol << "\" already in use!\n";
                return Parse_Result::Error;
            }
            else
            {
                symtab_map.at(symbol).sym.st_value = word;
            }
        }
        else
        {
            strtab_vect.push_back(symbol);
            Symtab_Entry entry(strtab_vect.size() - 1, word, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), SHN_ABS);
            symtab_map.insert(Symtab_Pair(symbol, entry));
            symtab_vect.push_back(entry);
        }
        cout << " SYMBOL: " << symbol << " EXPR: " << expr;
        break;
    }
    case Directive::Align:
    {
        if (cur_sect.name == "") return Parse_Result::Error;

        string param1 = match.str(index++), param2 = match.str(index++), param3 = match.str(index++);

        if (param1 == "")
        {
            cerr << "ERROR: Empty alignment size parameter!\n";
            return Parse_Result::Error;
        }

        Elf16_Half alignment;
        if (!decode_byte(param1, alignment))
        {
            cerr << "ERROR: Failed to decode: \"" << param1 << "\" as a byte value!\n";
            return Parse_Result::Error;
        }

        Elf16_Half fill;
        if (param2 == "") fill = 0x00;
        else if (!decode_byte(param2, fill))
        {
            cerr << "ERROR: Failed to decode: \"" << param2 << "\" as a byte value!\n";
            return Parse_Result::Error;
        }

        Elf16_Half max;
        if (param3 == "") max = alignment;
        else if (!decode_byte(param3, max))
        {
            cerr << "ERROR: Failed to decode: \"" << param3 << "\" as a byte value!\n";
            return Parse_Result::Error;
        }

        if (!alignment || alignment & (alignment - 1))
        {
            cerr << "ERROR: Value: " << alignment << " is not a power of two! Cannot apply alignment!\n";
            return Parse_Result::Error;
        }
        
        Elf16_Word remainder = cur_sect.loc_cnt & (alignment - 1);
        if (remainder)
        {
            unsigned fill_size = alignment - remainder;

            if (fill_size > max)
            {
                cerr << "ERROR: Required fill: " << fill_size << " is larger than max allowed: " << (unsigned) max << "! Cannot apply alignment!\n";
                return Parse_Result::Error;
            }

            if (pass == Pass::Second)
                for (unsigned i = 0; i < fill_size; ++i)
                    section_map.at(cur_sect.name).push_back(fill);

            cur_sect.loc_cnt += fill_size;
        }

        cout << " BYTES: " << (unsigned) alignment << " FILL: " << (unsigned) fill << " MAX: " << (unsigned) max;
        break;
    }
    case Directive::Skip:
    {
        if (cur_sect.flags & SHF_EXECINSTR)
        {
            cerr << "ERROR: Data in .text section!\n";
            return Parse_Result::Error;
        }

        string param1 = match.str(index++), param2 = match.str(index++);

        if (param1 == "")
        {
            cerr << "ERROR: Empty skip size parameter!\n";
            return Parse_Result::Error;
        }

        Elf16_Half size;
        if (!decode_byte(param1, size))
        {
            cerr << "ERROR: Failed to decode: \"" << param1 << "\" as a byte value!\n";
            return Parse_Result::Error;
        }

        Elf16_Half fill;
        if (param2 == "") fill = 0x00;
        else if (!decode_byte(param2, fill))
        {
            cerr << "ERROR: Failed to decode: \"" << param2 << "\" as a byte value!\n";
            return Parse_Result::Error;
        }

        if (pass == Pass::Second)
            for (unsigned i = 0; i < size; ++i)
                section_map.at(cur_sect.name).push_back(fill);

        cur_sect.loc_cnt += size;

        cout << " SIZE: " << (unsigned) size << " FILL: " << (unsigned) fill;
        break;
    }
    default:
    {
        res = Parse_Result::Error; // invalid directive (should never happen)
        break;
    }
    }

    cout << '\n';

    return res;
}

Parse_Result Assembler::parse_zeroaddr(const smatch &match, unsigned index)
{
    if (match.str(index).empty())
        return Parse_Result::Error;

    if (!(cur_sect.flags & SHF_EXECINSTR))
    {
        cerr << "ERROR: Code in unexecutable section: \"" << cur_sect.name << "\"!\n";
        return Parse_Result::Error;
    }

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++));

    if (pass == Pass::Second)
        section_map.at(cur_sect.name).push_back(opcode_map[opMnem]);

    cout << "Parsed: MNEMOMIC = " << opMnem << ' ';

    cur_sect.loc_cnt += sizeof(Elf16_Half);

    // this switch may be unnecessary...

    switch (zero_addr_instr_map[opMnem])
    {
    case ZeroAddrInstr::Nop:
    {
        cout << "no operation";
        break;
    }
    case ZeroAddrInstr::Halt:
    {
        cout << "halt";
        break;
    }
    case ZeroAddrInstr::Ret:
    {
        cout << "return from subroutine";
        break;
    }
    case ZeroAddrInstr::Iret:
    {
        cout << "return from interrupt routine";
        break;
    }
    default:
    {
        res = Parse_Result::Error; // invalid mnemonic (should never happen)
        break;
    }
    }

    cout << '\n';

    return res;
}

Parse_Result Assembler::parse_oneaddr(const smatch &match, unsigned index)
{
    if (match.str(index).empty())
        return Parse_Result::Error;

    if (!(cur_sect.flags & SHF_EXECINSTR))
    {
        cerr << "ERROR: Code in unexecutable section: \"" << cur_sect.name << "\"!\n";
        return Parse_Result::Error;
    }

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++)),
           opWidth = lowercase(match.str(index++)),
           operand = lowercase(match.str(index));

    if (opWidth == "")
        opWidth = "w";

    cout << "Parsed: MNEMOMIC = " << opMnem << opWidth << ' ';

    switch (one_addr_instr_map[opMnem])
    {
    case OneAddrInstr::Pushf:
    {
        if (pass == Pass::Second)
        {
            Elf16_Half opcode = opcode_map["push"] | 0x4;
            Elf16_Half op1desc = Addressing_Mode::RegDir | 0xF << 1;
            section_map.at(cur_sect.name).push_back(opcode);
            section_map.at(cur_sect.name).push_back(op1desc);
        }
        cur_sect.loc_cnt += 2;
        cout << "stack psw push";
        return Parse_Result::Success;
    }
    case OneAddrInstr::Popf:
    {
        if (pass == Pass::Second)
        {
            Elf16_Half opcode = opcode_map["pop"] | 0x4;
            Elf16_Half op1desc = Addressing_Mode::RegDir | 0xF << 1;
            section_map.at(cur_sect.name).push_back(opcode);
            section_map.at(cur_sect.name).push_back(op1desc);
        }
        cur_sect.loc_cnt += 2;
        cout << "stack psw pop";
        return Parse_Result::Success;
    }
    case OneAddrInstr::Push:
    {
        cout << "stack push, OPERAND = ";
        break;
    }
    case OneAddrInstr::Pop:
    {
        cout << "stack pop, OPERAND = ";
        break;
    }
    case OneAddrInstr::Int:
    {
        cout << "software interrupt, ENTRY = ";
        break;
    }
    case OneAddrInstr::Not:
    {
        cout << "negation, OPERAND = ";
        break;
    }
    case OneAddrInstr::Jmp:
    case OneAddrInstr::Jeq:
    case OneAddrInstr::Jne:
    case OneAddrInstr::Jgt:
    {
        cout << "negation, OPERAND = ";
        break;
    }
    case OneAddrInstr::Call:
    {
        cout << "function call, FUNCTION = ";
        break;
    }
    default:
    {
        res = Parse_Result::Error; // invalid mnemonic (should never happen)
        break;
    }
    }

    cout << operand << '\n';

    cur_sect.loc_cnt += sizeof(Elf16_Half);
    Elf16_Addr next_instr = cur_sect.loc_cnt + get_operand_size(operand);

    if (pass == Pass::First)
        cur_sect.loc_cnt = next_instr;
    else
    {
        Elf16_Half opcode = opcode_map[opMnem];
        if (opWidth[0] == 'w') opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
        section_map.at(cur_sect.name).push_back(opcode);
        if (!insert_operand(operand, opWidth[0], next_instr)) return Parse_Result::Error;
    }

    return res;
}

Parse_Result Assembler::parse_twoaddr(const smatch &match, unsigned index)
{
    if (match.str(index).empty())
        return Parse_Result::Error;

    if (!(cur_sect.flags & SHF_EXECINSTR))
    {
        cerr << "ERROR: Code in unexecutable section: \"" << cur_sect.name << "\"!\n";
        return Parse_Result::Error;
    }

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++)),
           opWidth = lowercase(match.str(index++)),
           op1 = match.str(index++),
           op2 = match.str(index++);

    if (opWidth == "")
        opWidth = "w";

    cout << "Parsed: MNEMOMIC = " << opMnem << opWidth << ' ';

    switch (two_addr_instr_map[opMnem])
    {
    case TwoAddrInstr::Xchg:
    {
        cout << "operand exchange";
        break;
    }
    case TwoAddrInstr::Mov:
    {
        cout << "moving from SRC to DST";
        break;
    }
    case TwoAddrInstr::Add:
    {
        cout << "DST = DST + SRC";
        break;
    }
    case TwoAddrInstr::Sub:
    {
        cout << "DST = DST - SRC";
        break;
    }
    case TwoAddrInstr::Mul:
    {
        cout << "DST = DST * SRC";
        break;
    }
    case TwoAddrInstr::Div:
    {
        cout << "DST = DST / SRC";
        break;
    }
    case TwoAddrInstr::Cmp:
    {
        cout << "temp = DST - SRC";
        break;
    }
    case TwoAddrInstr::And:
    {
        cout << "DST = DST & SRC";
        break;
    }
    case TwoAddrInstr::Or:
    {
        cout << "DST = DST | SRC";
        break;
    }
    case TwoAddrInstr::Xor:
    {
        cout << "DST = DST ^ SRC";
        break;
    }
    case TwoAddrInstr::Test:
    {
        cout << "temp = DST & SRC";
        break;
    }
    default:
    {
        res = Parse_Result::Error; // invalid mnemonic (should never happen)
        break;
    }
    }

    cout << " DST = " << op1 << " SRC = " << op2 << '\n';

    cur_sect.loc_cnt += sizeof(Elf16_Half);
    Elf16_Addr next_instr = cur_sect.loc_cnt + get_operand_size(op1) + get_operand_size(op2);

    if (pass == Pass::First)
        cur_sect.loc_cnt = next_instr;
    else
    {
        Elf16_Half opcode = opcode_map[opMnem];
        if (opWidth[0] == 'w') opcode |= 0x4; // S bit = 0 for byte sized operands, = 1 for word sized operands
        section_map.at(cur_sect.name).push_back(opcode);
        if (!insert_operand(op1, opWidth[0], next_instr)) return Parse_Result::Error;
        if (!insert_operand(op2, opWidth[0], next_instr)) return Parse_Result::Error;
    }

    return res;
}

bool Assembler::decode_word(const string &str, Elf16_Word &word)
{
    word = 0;
    if (str == "") return true;
    smatch match;
    if (!regex_match(str, match, regex_word)) return false;
    string value = match.str(1);
    bool inv = value[0] == '~', neg = value[0] == '-';
    int first = inv || neg;
    unsigned long temp;
    if (value[first] != '0') temp = stoul(value.substr(first), 0, 10);
    else if (value.length() == first + 1) temp = 0; // 0 || ~0 || -0
    else if (value[first + 1] == 'b') temp = stoul(value.substr(first + 2), 0, 2);
    else if (value[first + 1] != 'x') temp = stoul(value.substr(first + 1), 0, 8);
    else temp = stoul(value.substr(first + 2), 0, 16);
    if (temp >= 0 && temp <= 0xffff)
    {
        word = temp;
        if (inv) word = ~word;
        else if (neg) word = -word;
        return true;
    }
    return false;
}

bool Assembler::decode_byte(const string &str, Elf16_Half &byte)
{
    byte = 0;
    if (str == "") return true;
    smatch match;
    if (!regex_match(str, match, regex_byte)) return false;
    string value = match.str(1);
    bool inv = value[0] == '~', neg = value[0] == '-';
    int first = inv || neg;
    unsigned long long temp;
    if (value[first] != '0') temp = stoul(value.substr(first), 0, 10);
    else if (value.length() == first + 1) temp = 0; // 0 || ~0 || -0
    else if (value[first + 1] == 'b') temp = stoul(value.substr(first + 2), 0, 2);
    else if (value[first + 1] != 'x') temp = stoul(value.substr(first + 1), 0, 8);
    else temp = stoul(value.substr(first + 2), 0, 16);
    if (temp >= 0 && temp <= 0xff)
    {
        byte = temp;
        if (inv) byte = ~byte;
        else if (neg) byte = -byte;
        return true;
    }
    return false;
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

void Assembler::global_symbol(const std::string &str)
{
    string sym;
    smatch match;
    for (string token : tokenize(str, regex_split))
        if (regex_match(token, match, regex_symbol))
        {
            sym = match.str(1);
            if (symtab_map.count(sym) > 0)
            {
                int type = ELF16_ST_TYPE(symtab_map.at(sym).sym.st_info);
                symtab_map.at(sym).sym.st_info = ELF16_ST_INFO(STB_GLOBAL, type);
            }
            else
            {
                strtab_vect.push_back(sym);
                // st_name may be 0 check this
                Symtab_Entry entry(strtab_vect.size() - 1, 0, ELF16_ST_INFO(STB_GLOBAL, STT_NOTYPE), SHN_UNDEF);
                symtab_map.insert(Symtab_Pair(sym, entry));
            }
        }
}

unsigned Assembler::get_operand_size(const string &operand)
{
    smatch match;
    if (regex_match(operand, match, regex_op1b)) return 1;
    if (!regex_match(operand, match, regex_op2b)) return 3;
    unsigned index = 1;
    while (index < match.size() && match.str(index).empty())
        index++;
    Elf16_Half offset;
    if (decode_byte(match.str(index), offset))
    {
        if (offset == 0) return 1; // if offset is zero assume regind without offset
        return 2;
    }
    return 3;
}

bool Assembler::insert_operand(const string &operand, const char size, Elf16_Addr next_instr)
{
    smatch match;
    if (size == 'b')
    {
        if (regex_match(operand, match, regex_imm_b))
        {
            Elf16_Half byte;
            if (!decode_byte(match.str(1), byte))
            {
                cerr << "ERROR: Failed to decode: \"" << match.str(1) << "\" as a byte value!\n";
                return false;
            }
            section_map.at(cur_sect.name).push_back(Addressing_Mode::Imm);
            section_map.at(cur_sect.name).push_back(byte);
            cur_sect.loc_cnt += 2;
            return true;
        }
        else if (regex_match(operand, match, regex_regdir_b))
        {
            Elf16_Half opdesc = Addressing_Mode::RegDir;
            string reg = match.str(1);
            opdesc |= (reg[1] - '0') << 1;
            if (reg[2] == 'h') opdesc |= 0x1;
            section_map.at(cur_sect.name).push_back(opdesc);
            cur_sect.loc_cnt++;
            return true;
        }
    }
    else
    {
        if (regex_match(operand, match, regex_imm_w))
        {
            section_map.at(cur_sect.name).push_back(Addressing_Mode::Imm);
            cur_sect.loc_cnt++;
            if (match.str(1)[0] == '&')
            {
                if (add_reloc(match.str(1).substr(1), R_VN_16, next_instr))
                    return true;
            }
            else
            {
                Elf16_Word word;
                if (!decode_word(match.str(1), word))
                {
                    cerr << "ERROR: Failed to decode: \"" << match.str(1) << "\" as a word value!\n";
                    return false;
                }
                section_map.at(cur_sect.name).push_back(word & 0x00ff); // little-endian
                section_map.at(cur_sect.name).push_back(word >> 8);
                cur_sect.loc_cnt += 2;
                return true;
            }
        }
        else if (regex_match(operand, match, regex_regdir_w))
        {
            Elf16_Half opdesc = Addressing_Mode::RegDir;
            string reg = match.str(1);
            if (reg[0] == 'r') opdesc |= (reg[1] - '0') << 1;
            else if (reg == "sp") opdesc |= 6 << 1;
            else if (reg == "pc") opdesc |= 7 << 1;
            else
            {
                cerr << "ERROR: Invalid register: \"" << reg << "\"!\n";
                return false;
            }
            section_map.at(cur_sect.name).push_back(opdesc);
            cur_sect.loc_cnt++;
            return true;
        }
    }
    if (regex_match(operand, match, regex_regind))
    {
        Elf16_Half opdesc = Addressing_Mode::RegInd;
        string reg = match.str(1);
        if (reg[0] == 'r') opdesc |= (reg[1] - '0') << 1;
        else if (reg == "sp") opdesc |= 6 << 1;
        else if (reg == "pc") opdesc |= 7 << 1;
        else
        {
            cerr << "ERROR: Invalid register: \"" << reg << "\"!\n";
            return false;
        }
        section_map.at(cur_sect.name).push_back(opdesc);
        cur_sect.loc_cnt++;
        return true;
    }
    else if (regex_match(operand, match, regex_regindoff))
    {
        Elf16_Half opdesc = 0;
        string reg = match.str(1), value = match.str(2);
        if (reg[0] == 'r') opdesc |= (reg[1] - '0') << 1;
        else if (reg == "sp") opdesc |= 6 << 1;
        else if (reg == "pc") opdesc |= 7 << 1;
        else
        {
            cerr << "ERROR: Invalid register: \"" << reg << "\"!\n";
            return false;
        }
        Elf16_Half byteoff;
        Elf16_Word wordoff;
        if (decode_byte(value, byteoff))
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
        else if (decode_word(value, wordoff))
        {
            opdesc |= Addressing_Mode::RegIndOff16; // 16-bit offset
            section_map.at(cur_sect.name).push_back(opdesc);
            section_map.at(cur_sect.name).push_back(wordoff & 0x00ff); // little-endian
            section_map.at(cur_sect.name).push_back(wordoff >> 8);
            cur_sect.loc_cnt += 3;
        }
        else
        {
            cerr << "ERROR: Failed to decode: \"" << match.str(1) << "\" as a byte or word value!\n";
            return false;
        }
        return true;
    }
    else if (regex_match(operand, match, regex_regindsym))
    {
        Elf16_Half opdesc = Addressing_Mode::RegIndOff16;
        string reg = match.str(1);
        if (reg[0] == 'r') opdesc |= (reg[1] - '0') << 1;
        else if (reg == "sp") opdesc |= 6 << 1;
        else if (reg == "pc") opdesc |= 7 << 1;
        else
        {
            cerr << "ERROR: Invalid register: \"" << reg << "\"!\n";
            return false;
        }
        section_map.at(cur_sect.name).push_back(opdesc);
        cur_sect.loc_cnt++;
        if (add_reloc(match.str(2), R_VN_16, next_instr))
            return true;
    }
    else if (regex_match(operand, match, regex_memsym))
    {
        bool pcrel = match.str(1)[0] == '$';
        if (pcrel) section_map.at(cur_sect.name).push_back(Addressing_Mode::RegIndOff16 | 7 << 1);
        else section_map.at(cur_sect.name).push_back(Addressing_Mode::Mem);
        cur_sect.loc_cnt++;
        if (add_reloc(match.str(2), pcrel ? R_VN_PC16 : R_VN_16, next_instr))
            return true;
    }
    else if (regex_match(operand, match, regex_memabs))
    {
        Elf16_Word address;
        if (!decode_word(match.str(1), address))
        {
            cerr << "ERROR: Invalid address: \"" << match.str(1) << "\"!\n";
            return false;
        }
        section_map.at(cur_sect.name).push_back(Addressing_Mode::Mem);
        section_map.at(cur_sect.name).push_back(address & 0x00ff); // little-endian
        section_map.at(cur_sect.name).push_back(address >> 8);
        cur_sect.loc_cnt += 3;
        return true;
    }
    cerr << "ERROR: Invalid operand: \"" << operand << "\"!\n";
    return false;
}


bool Assembler::add_reloc(const std::string symbol, Elf16_Half type, Elf16_Addr next_instr)
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