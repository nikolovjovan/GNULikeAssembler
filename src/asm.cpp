#include "asm.h"

#include <iostream>

using std::cerr;
using std::cout;
using std::map;
using std::regex;
using std::smatch;
using std::string;

inline string lowercase(const string &s)
{
    string res = s;
    for (unsigned j = 0; j < res.length(); ++j)
        if (res[j] >= 'A' && res[j] <= 'Z')
            res[j] = (char) (res[j] - 'A' + 'a');
    return res;
}

Assembler::Assembler(const string &input_file, const string &output_file, bool binary)
{
    this->input_file = input_file;
    this->output_file = output_file;
    this->binary = binary;

    for (unsigned i = 0; i < REGEX_CNT; ++i)
        regex_exprs[i].assign(regex_strings[i], regex::icase | regex::optimize);
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

    lcMap.clear();
    cur_sect = "";
    cur_lc = 0;

    if (!run_second_pass())
    {
        cerr << "ERROR: Assembler failed to complete second pass!\n";
        return false;
    }

    // print to file

    return true;
}

bool Assembler::run_first_pass()
{
    pass = FIRST_PASS;
    if (input.is_open())
        input.close();
    string line = "";
    input.open(input_file, std::ifstream::in);
    for (unsigned int lnum = 1; !input.eof(); ++lnum)
    {
        getline(input, line);
        cout << lnum << ":\t" << line << '\n'; // temporary
        parse_t ret = parse(line);
        if (ret == ERROR)
        {
            cerr << "ERROR: Failed to parse line: " << lnum << "!\n";
            input.close();
            return false;
        }
        if (ret == END)
        {
            cout << "End of file reached at line: " << lnum << "!\n";
            break;
        }
    }
    input.close();
    return true;
}

bool Assembler::run_second_pass()
{
    pass = SECOND_PASS;
    return true;
}

parse_t Assembler::parse(const string &s)
{
    smatch match;
    for (unsigned i = 0; i < REGEX_CNT; ++i)
        if (regex_match(s, match, regex_exprs[i]))
        {
            unsigned first = 1;
            while (first < match.size() && match.str(first).empty()) first++;

            switch (i)
            {
            case EMPTY:
            {
                cout << "Parsed: Empty line\n";
                return SUCCESS;
            }
            case LABEL:
            {
                string label = match.str(first);
                string other = match.str(first + 1);

                cout << "Parsed: LABEL = " << label << " OTHER: " << other << '\n';

                if (pass == FIRST_PASS)
                    if (!add_symbol(label))
                        return ERROR; // failed to add symbol
                // else SECOND_PASS ...

                if (!other.empty())
                    return parse(other); // return whatever is parsed from other
                return SUCCESS;
            }
            case DIRECTIVE:
            {
                return parse_directive(match, first);
            }
            case ZEROADDR:
            {
                return parse_zeroaddr(match, first);
            }
            case ONEADDR:
            {
                return parse_oneaddr(match, first);
            }
            case TWOADDR:
            {
                return parse_twoaddr(match, first);
            }
            // default:
            // {
            //     // printing matches
            //     cout << "match: ";
            //     for (unsigned i = 1; i < match.size(); ++i)
            //     {
            //         cout << '\"' << match.str(i) << '\"';
            //         cout << (i < match.size() - 1 ? '|' : ' ');
            //         // for (int j = i + 1; j < match.size(); ++j)
            //         //     if (!match.str(j).empty())
            //         //     {
            //         //         cout << "|";
            //         //         break;
            //         //     }
            //     }
            //     cout << '\n';
            // }
            }
        }
    return ERROR;
}

parse_t Assembler::parse_directive(const std::smatch &match, unsigned first)
{
    parse_t res = SUCCESS;
    string dir = "";
    for (unsigned i = 1; i < match.size(); ++i)
    {
        if (match.str(i).empty())
            continue;

        dir = lowercase(match.str(i));

        cout << "Parsed: DIRECTIVE = " << dir;

        if (!dir.compare("global") || !dir.compare("extern") ||
            !dir.compare("byte") || !dir.compare("word"))
        {
            cout << " SYMBOLS: " << match.str(i + 1);
        }
        else if (!dir.compare("equ") || !dir.compare("set"))
        {
            cout << " SYMBOL: " << match.str(i + 1) << " EXPR: " << match.str(i + 2);
        }
        else if (!dir.compare("text") || !dir.compare("data") ||
                 !dir.compare("bss") || !dir.compare("end"))
        {
            if (!dir.compare("end"))
                res = END;
        }
        else if (!dir.compare("section"))
        {
            cout << " NAME: " << match.str(i + 1) << " FLAGS: " << match.str(i + 2);
        }
        else if (!dir.compare("align"))
        {
            cout << " BYTES: " << match.str(i + 1) << " FILL: " << match.str(i + 2) << " MAX: " << match.str(i + 3);
        }
        else if (!dir.compare("skip"))
        {
            cout << " BYTES: " << match.str(i + 1) << " FILL: " << match.str(i + 2);
        }
        else
            res = ERROR; // invalid directive (should never happen)

        cout << '\n';

        return res;
    }
    return ERROR;
}

parse_t Assembler::parse_zeroaddr(const std::smatch &match, unsigned first)
{
    parse_t res = SUCCESS;
    string opMnem = "";
    for (unsigned i = 1; i < match.size(); ++i)
    {
        if (match.str(i).empty())
            continue;

        opMnem = lowercase(match.str(i));

        cout << "Parsed: MNEMOMIC = " << opMnem << ' ';

        if (!opMnem.compare("nop"))
        {
            cout << "no operation";
        }
        else if (!opMnem.compare("halt"))
        {
            cout << "halt";
        }
        else if (!opMnem.compare("ret"))
        {
            cout << "return from subroutine";
        }
        else if (!opMnem.compare("iret"))
        {
            cout << "return from interrupt routine";
        }
        else
            res = ERROR; // invalid directive (should never happen)

        cout << '\n';

        return res;
    }
    return ERROR;
}

parse_t Assembler::parse_oneaddr(const std::smatch &match, unsigned first)
{
    parse_t res = SUCCESS;
    string opMnem = "";
    for (unsigned i = 1; i < match.size(); ++i)
    {
        if (match.str(i).empty())
            continue;

        opMnem = lowercase(match.str(i));

        cout << "Parsed: MNEMOMIC = " << opMnem << ' ';

        if (!opMnem.compare("int"))
        {
            cout << "software interrupt, ENTRY = ";
        }
        else if (!opMnem.compare("not"))
        {
            cout << "negation, OPERAND = ";
        }
        else if (!opMnem.compare("jmp") || !opMnem.compare("jeq") ||
                 !opMnem.compare("jne") || !opMnem.compare("jgt"))
        {
            cout << "jump, TO = ";
        }
        else if (!opMnem.compare("call"))
        {
            cout << "function call, FUNCTION = ";
        }
        else if (!opMnem.compare("push") || !opMnem.compare("pop"))
        {
            cout << "stack manipulation, OPERAND = ";
        }
        else
            res = ERROR; // invalid directive (should never happen)

        cout << match.str(i + 1) << '\n';

        return res;
    }
    return ERROR;
}

parse_t Assembler::parse_twoaddr(const std::smatch &match, unsigned first)
{
    parse_t res = SUCCESS;
    string opMnem = "", opWidth;
    for (unsigned i = 1; i < match.size(); ++i)
    {
        if (match.str(i).empty())
            continue;

        opMnem = lowercase(match.str(i));
        opWidth = lowercase(match.str(i + 1));

        if (!opWidth.compare(""))
            opWidth = "w";

        cout << "Parsed: MNEMOMIC = " << opMnem << opWidth << ' ';

        // char width = opMnem[opMnem.length() - 1] == 'b' ? 'b' : 'w';

        // if (opMnem[opMnem.length() - 1] == 'b' ||
        //     opMnem[opMnem.length() - 1] == 'w')
        //     opMnem = opMnem.substr(0, opMnem.length() - 1);

        if (!opMnem.compare("xchg"))
        {
            cout << "operand exchange";
        }
        else if (!opMnem.compare("mov"))
        {
            cout << "moving from SRC to DST";
        }
        else if (!opMnem.compare("add"))
        {
            cout << "DST = DST + SRC";
        }
        else if (!opMnem.compare("sub"))
        {
            cout << "DST = DST - SRC";
        }
        else if (!opMnem.compare("mul"))
        {
            cout << "DST = DST * SRC";
        }
        else if (!opMnem.compare("div"))
        {
            cout << "DST = DST / SRC";
        }
        else if (!opMnem.compare("cmp"))
        {
            cout << "temp = DST - SRC";
        }
        else if (!opMnem.compare("and"))
        {
            cout << "DST = DST & SRC";
        }
        else if (!opMnem.compare("or"))
        {
            cout << "DST = DST | SRC";
        }
        else if (!opMnem.compare("xor"))
        {
            cout << "DST = DST ^ SRC";
        }
        else if (!opMnem.compare("test"))
        {
            cout << "temp = DST & SRC";
        }
        else
            res = ERROR; // invalid directive (should never happen)

        cout << " DST = " << match.str(i + 2) << " SRC = " << match.str(i + 3) << '\n';

        return res;
    }
    return ERROR;
}

bool Assembler::add_symbol(const string &symbol)
{
    // unimplemented method!
    return true;
}