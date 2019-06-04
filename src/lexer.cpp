#include "lexer.h"

using std::list;
using std::regex;
using std::smatch;
using std::sregex_token_iterator;
using std::string;

Lexer::Lexer()
{
    empty_rx.assign(empty_str, regex::icase | regex::optimize);
    line_rx.assign(line_str, regex::icase | regex::optimize);

    directive_rx.assign(directive_str, regex::icase | regex::optimize);
    zeroaddr_rx.assign(zeroaddr_str, regex::icase | regex::optimize);
    oneaddr_rx.assign(oneaddr_str, regex::icase | regex::optimize);
    twoaddr_rx.assign(twoaddr_str, regex::icase | regex::optimize);

    split_rx.assign(split_str, regex::icase | regex::optimize);
    symbol_rx.assign(symbol_str, regex::icase | regex::optimize);
    byte_rx.assign(byte_str, regex::icase | regex::optimize);
    word_rx.assign(word_str, regex::icase | regex::optimize);
    byte_operand_rx.assign(byte_operand_str, regex::icase | regex::optimize);
    word_operand_rx.assign(word_operand_str, regex::icase | regex::optimize);
}

string Lexer::tolower(const string &str)
{
    string res = str;
    for (unsigned j = 0; j < res.length(); ++j)
        if (res[j] >= 'A' && res[j] <= 'Z')
            res[j] = (char)(res[j] - 'A' + 'a');
    return res;
}

bool Lexer::is_empty(const string &str)
{
    return regex_match(str, empty_rx);
}

list<string> Lexer::split_string(const string &str)
{
    return Lexer::tokenize_string(str, split_rx);
}

bool Lexer::tokenize_line(const string &str, tokens_t &tokens)
{
    smatch m;
    if (regex_match(str, m, line_rx))
    {
        for (unsigned i = 1; i < m.size(); ++i)
            tokens.push_back(m.str(i));
        return true;
    }
    return false;
}

bool Lexer::tokenize_directive(const string &str, tokens_t &tokens)
{
    return Lexer::tokenize_content(str, directive_rx, tokens);
}

bool Lexer::tokenize_zeroaddr(const string &str, tokens_t &tokens)
{
    return Lexer::tokenize_content(str, zeroaddr_rx, tokens);
}

bool Lexer::tokenize_oneaddr(const string &str, tokens_t &tokens)
{
    return Lexer::tokenize_content(str, oneaddr_rx, tokens, true);
}

bool Lexer::tokenize_twoaddr(const string &str, tokens_t &tokens)
{
    return Lexer::tokenize_content(str, twoaddr_rx, tokens, true);
}

bool Lexer::match_byte(const string &str, string &result)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, byte_rx, tokens);
    if (res) result = tokens[0];
    return res;
}

bool Lexer::match_word(const string &str, string &result)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, word_rx, tokens);
    if (res) result = tokens[0];
    return res;
}

bool Lexer::match_byte_operand(const std::string &str)
{
    tokens_t tokens;
    return Lexer::tokenize_content(str, byte_operand_rx, tokens);
}

bool Lexer::match_word_operand(const std::string &str, std::string &offset)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, word_operand_rx, tokens);
    offset = tokens[0];
    return res;
}

list<string> Lexer::tokenize_string(const string &str, const regex &regex)
{
    list<string> tokens;
    sregex_token_iterator it(str.begin(), str.end(), regex, -1), reg_end;
    for (; it != reg_end; ++it)
        tokens.emplace_back(it->str());
    return tokens;
}

bool Lexer::tokenize_content(const string &str, const regex &regex, tokens_t &tokens, bool ignore_second)
{
    smatch m;
    if (regex_match(str, m, regex))
    {
        unsigned i = 1;
        while (i < m.size() && m.str(i).empty()) i++;
        if (i == m.size()) return false;
        tokens.push_back(m.str(i++));
        if (!ignore_second && m.str(i).empty()) return true;
        tokens.push_back(m.str(i++));
        for (; i < m.size() && !m.str(i).empty(); ++i)
            tokens.push_back(m.str(i));
        return true;
    }
    return false;
}