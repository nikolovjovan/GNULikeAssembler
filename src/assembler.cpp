#include "assembler.h"

#include <iostream>

using std::cerr;
using std::cout;
using std::ifstream;
using std::ios;
using std::make_pair;
using std::string;

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

    /*
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
*/
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

    /*
    cout << "symtab_map:\n";
    for (auto it = symtab_map.begin(); it != symtab_map.end(); ++it)
        cout << it->first << "\t->" << it->second.index << "\t= "
             << it->second.sym.st_name << ':' << (int)it->second.sym.st_value << ':'
             << it->second.sym.st_size << ':' << ELF16_ST_BIND(it->second.sym.st_info)
             << ':' << ELF16_ST_TYPE(it->second.sym.st_info) << ':' << it->second.sym.st_shndx << '\n';

    cout << "shdrtab_map:\n";
    for (auto it = shdrtab_map.begin(); it != shdrtab_map.end(); ++it)
        cout << "Section name:" << it->first << "\t\tIndex:\t" << it->second.index << "\tSection type:\t" << it->second.shdr.sh_type << "\tSection flags:\t" << it->second.shdr.sh_flags << "\tSection size:\t" << it->second.shdr.sh_size << "\tSection info:\t" << it->second.shdr.sh_info << "\tLink:\t" << it->second.shdr.sh_link << '\n';
    */

    // write_output();

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
            if (process_line(info)) continue;
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
        print_line(file_vect[i]);
        cout << '\n';
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

bool Assembler::process_line(Line_Info &info)
{
    info.loc_cnt = cur_sect.loc_cnt;
    if (info.line.label.empty() && info.line.content_type == Content_Type::None) return true; // empty line
    file_vect.push_back(info);
    if (info.line.content_type == Content_Type::Directive)
    {
        if (!process_directive(info.line.getDir()))
            return false; // .end directive = end of file, return false
    }
    else process_instruction(info.line.getInstr());
    // {
    //     if (info.line.getDir().code == Directive_Code::End) 
    //     if (pass == Pass::First)
    //     {
    //         cur_sect.loc_cnt += get_directive_size(info.line.getDir());
    //     }
    //     else
    //     {

    //     }
    // }
    // else
    // {
    //     if (pass == Pass::First)
    //     {
    //         cur_sect.loc_cnt += get_instruction_size(info.line.getInstr());
    //     }
    //     else
    //     {

    //     }
    // }
    return true;
}

bool Assembler::process_directive(const Directive &dir)
{
    // cout << "DIRECTIVE = " << parser->get_directive(dir.code);

    // switch (dir.code)
    // {
    // case Directive_Code::Global:
    // }

    return true;
}

bool Assembler::process_instruction(const Instruction &instr)
{
    return true;
}

uint8_t Assembler::get_directive_size(const Directive &dir)
{

}

uint8_t Assembler::get_instruction_size(const Instruction &instr)
{

}

uint8_t Assembler::get_operand_size(const string &op)
{

}