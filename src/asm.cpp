#include "asm.h"

#include <iostream>
#include <string>

using std::cerr;
using std::cout;
using std::ifstream;
using std::map;
using std::regex;
using std::smatch;
using std::string;
using std::stoul;

Elf16_Addr Symtab_Entry::symtab_index   = 0;
Elf16_Addr Shdrtab_Entry::shdrtab_index = 0;

inline string lowercase(const string &s)
{
    string res = s;
    for (unsigned j = 0; j < res.length(); ++j)
        if (res[j] >= 'A' && res[j] <= 'Z')
            res[j] = (char)(res[j] - 'A' + 'a');
    return res;
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
    Symtab_Entry dummySym(0, 0, ELF16_ST_INFO(STB_LOCAL, STT_NOTYPE), 0, "");
    symtab_map.insert(Symtab_Pair("", dummySym));
    // symtab_map.emplace("", dummySym);
    symtab_vect.push_back(dummySym);
    strtab_vect.push_back("");

    // Inserting a dummy section header
    Shdrtab_Entry dummyShdr(SHT_NULL, 0, 0);
    shdrtab_map.insert(Shdrtab_Pair("", dummyShdr));
    // shdrtab_map.emplace("", dummyShdr);
    shstrtab_vect.push_back("");

    // Initializing regex
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

    // print to file

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
    return true;
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
            if (flags == "")
            {
                // try to infer section type and flags from section name
                if (name == ".bss") add_shdr(name, SHT_NOBITS, SHF_WRITE);
                else if (name == ".data") add_shdr(name, SHT_PROGBITS, SHF_ALLOC);
                else if (name == ".text") add_shdr(name, SHT_PROGBITS, SHF_EXECINSTR);
                else
                {
                    cerr << "ERROR: Cannot infer section type and flags from section name: " << name << '\n';
                    return Parse_Result::Error;
                }
            }
            else
            {
                // parse flags string
                bool nobits = false, read = false, write = false, execute = false;
                for (char c : flags)
                    if (c == 'b') nobits = true;
                    else if (c == 'r') read = true;
                    else if (c == 'w') write = true;
                    else if (c == 'x') execute = true;
                    // else if (c >= '0' && c <= '9') cur_sect.alignment = 1 << (c - '0');
                if (nobits && write && !execute) add_shdr(name, SHT_NOBITS, SHF_WRITE);
                else if (!nobits && write && !execute) add_shdr(name, SHT_PROGBITS, SHF_ALLOC);
                else if (!nobits && !write && execute) add_shdr(name, SHT_PROGBITS, SHF_EXECINSTR);
                else
                {
                    cerr << "ERROR: Invalid section flags combination: " << flags << '\n';
                    return Parse_Result::Error;
                }
            }
        }

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
        // second pass modify symbol info
        break;
    }
    case Directive::Byte:
    case Directive::Word:
    {
        cout << " SYMBOLS: " << match.str(index++);
        break;
    }
    case Directive::Equ:
    case Directive::Set:
    {
        cout << " SYMBOL: " << match.str(index++) << " EXPR: " << match.str(index++);
        // calculate expression
        // add symbol
        break;
    }
    case Directive::Align:
    {
        if (cur_sect.name == "") return Parse_Result::Error;

        Elf16_Half alignment;
        if (!decode_byte(match.str(index++), alignment)) return Parse_Result::Error;

        Elf16_Half fill;
        if (!decode_byte(match.str(index++), fill)) return Parse_Result::Error;

        Elf16_Half max;
        if (!decode_byte(match.str(index++), max)) return Parse_Result::Error;

        if (!alignment || alignment & (alignment - 1))
        {
            cerr << "ERROR: Value: " << alignment << " is not a power of two! Cannot apply alignment!\n";
            return Parse_Result::Error;
        }
        
        Elf16_Word remainder = cur_sect.loc_cnt & (alignment - 1);
        if (remainder && pass == Pass::First)
            cur_sect.loc_cnt += alignment - remainder;

        cout << " BYTES: " << alignment << " FILL: " << fill << " MAX: " << max;
        break;
    }
    case Directive::Skip:
    {
        Elf16_Half bytes;
        if (!decode_byte(match.str(index++), bytes)) return Parse_Result::Error;

        Elf16_Half fill;
        if (!decode_byte(match.str(index++), fill)) return Parse_Result::Error;

        if (pass == Pass::First)
            cur_sect.loc_cnt += bytes;

        cout << " BYTES: " << bytes << " FILL: " << fill;
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

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++));

    cout << "Parsed: MNEMOMIC = " << opMnem << ' ';

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

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++));
    string operand = match.str(index);

    cout << "Parsed: MNEMOMIC = " << opMnem << ' ';

    switch (one_addr_instr_map[opMnem])
    {
    case OneAddrInstr::Pushf:
        opMnem = "push";
        operand = "psw";
    case OneAddrInstr::Push:
    {
        cout << "stack push, OPERAND = ";
        break;
    }
    case OneAddrInstr::Popf:
        opMnem = "pop";
        operand = "psw";
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

    return res;
}

Parse_Result Assembler::parse_twoaddr(const smatch &match, unsigned index)
{
    if (match.str(index).empty())
        return Parse_Result::Error;

    Parse_Result res = Parse_Result::Success;
    string opMnem = lowercase(match.str(index++)),
           opWidth = lowercase(match.str(index++));

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

    cout << " DST = " << match.str(index++) << " SRC = " << match.str(index++) << '\n';

    return res;
}

bool Assembler::decode_word(const string &value, Elf16_Word &result)
{
    if (value == "")
    {
        result = 0;
        return true;
    }
    bool inv = value[0] == '~', neg = value[0] == '-';
    int first = inv || neg;
    unsigned long temp;
    if (value[first] != '0') temp = stoul(value.substr(first), 0, 10);
    else if (value[first + 1] == 'b') temp = stoul(value.substr(first + 2), 0, 2);
    else if (value[first + 1] != 'x') temp = stoul(value.substr(first + 1), 0, 8);
    else temp = stoul(value.substr(first + 2), 0, 16);
    if (temp >= 0 && temp <= 0xffff)
    {
        result = temp;
        if (inv) result = ~result;
        else if (neg) result = -result;
        return true;
    }
    cerr << "ERROR: Failed to decode: " << value << " as a word value!\n";
    return false;
}

bool Assembler::decode_byte(const string &value, Elf16_Half &result)
{
    Elf16_Word temp;
    if (decode_word(value, temp) && temp >= 0 && temp <= 0xff)
    {
        result = temp;
        return true;
    }
    cerr << "ERROR: Failed to decode: " << value << " as a byte value!\n";
    return false;
}

bool Assembler::add_symbol(const string &symbol)
{
    if (symtab_map.count(symbol) > 0)
    {
        cerr << "ERROR: Symbol \"" << symbol << "\" already in use!\n";
        return false;
    }

    Elf16_Word st_name = 0;
    string section = "";

    // for relocation sections...
    if (symbol != cur_sect.name)
    {
        strtab_vect.push_back(symbol);
        st_name = strtab_vect.size() - 1;
        section = cur_sect.name;
    }

    Symtab_Entry entry(st_name, cur_sect.loc_cnt, ELF16_ST_INFO(STB_LOCAL, STT_OBJECT), cur_sect.shdrtab_index, section);
    symtab_map.insert(Symtab_Pair(symbol, entry));
    // symtab_map.emplace(symbol, entry);
    symtab_vect.push_back(entry);

    return true;
}

bool Assembler::add_shdr(const string &name, Elf16_Word type, Elf16_Word flags)
{
    // for relocation sections, not implemented yet
    // if (name[0] != 'r')
    //     if (add_symbol(cur_sect.name, cur_sect.loc_cnt) == -1)
    //         return -1;

    if (shdrtab_map.count(name) > 0)
        return true;

    Shdrtab_Entry entry(type, flags);
    shdrtab_map.insert(Shdrtab_Pair(name, entry));
    shstrtab_vect.push_back(name);

    cur_sect.type = type;
    cur_sect.flags = flags;
    cur_sect.shdrtab_index = entry.shdrtab_index;

    return true;
}