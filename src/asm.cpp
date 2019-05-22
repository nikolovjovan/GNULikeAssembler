#include "asm.h"

#include <iostream>
#include <cctype>

using std::cerr;
using std::cout;
using std::map;
using std::regex;
using std::smatch;
using std::string;

Assembler::Assembler(const string &input_file, const string &output_file, bool binary)
{
    this->input_file = input_file;
    this->output_file = output_file;
    this->binary = binary;

    for (unsigned i = 0; i < REGEX_CNT; ++i)
    {
        regex_exprs[i].assign(regex_strings[i], regex::icase | regex::optimize); //, std::regex::extended); // = std::regex{regex_strings[i], std::regex::extended};
    }
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
            // printing matches
            // cout << "match: ";
            // for (unsigned i = 1; i < match.size(); ++i)
            // {
            //     cout << '\"' << match.str(i) << '\"';
            //     cout << (i < match.size() - 1 ? '|' : ' ');
            //     // for (int j = i + 1; j < match.size(); ++j)
            //     //     if (!match.str(j).empty())
            //     //     {
            //     //         cout << "|";
            //     //         break;
            //     //     }
            // }
            // cout << '\n';

            // parsing matches
            switch (i)
            {
            case EMPTY:
            {
                cout << "Parsed: Empty line\n";
                return SUCCESS;
            }
            case LABEL:
            {
                string label = match.str(1);
                string other = match.str(2);

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
                return parse_directive(match);
            }
            }
        }
    return ERROR;
}

parse_t Assembler::parse_directive(const std::smatch &match)
{
    parse_t res = SUCCESS;
    string dir = "";
    for (unsigned i = 1; i < match.size(); ++i)
    {
        if (match.str(i).empty())
            continue;

        dir.assign(match.str(i).c_str());
        for (unsigned j = 0; j < dir.length(); ++j)
            if (dir[j] >= 'A' && dir[j] <= 'Z')
                dir[j] = (char) (dir[j] - 'A' + 'a');

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

bool Assembler::add_symbol(const string &symbol)
{
    // unimplemented method!
    return true;
}