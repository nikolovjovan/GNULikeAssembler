#include "lexer.h"

using std::regex;
using std::smatch;
using std::string;
using std::vector;

Lexer::Lexer()
{
    line_regex.assign(line_string, regex::icase | regex::optimize);
}

bool Lexer::tokenize_line(const string &line, vector<string> &tokens)
{
    smatch m;
    if (regex_match(line, m, line_regex))
    {
        for (unsigned i = 1; i < m.size(); ++i)
            tokens.push_back(m.str(i));
        return true;
    }
    return false;
}

bool Lexer::tokenize_content(const string &content, vector<string> &tokens)
{
}