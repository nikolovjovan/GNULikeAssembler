#include "lexer.h"

using std::list;
using std::regex;
using std::smatch;
using std::sregex_token_iterator;
using std::string;

Lexer::Lexer()
{
    // g++ < 8.1 have a bug with regex::icase and that was messing it up...
    // source: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=71500#c20
    empty_rx.assign(empty_str, regex::optimize);
    line_rx.assign(line_str, regex::optimize);
    split_rx.assign(split_str, regex::optimize);
    symbol_rx.assign(symbol_str, regex::optimize);
    byte_rx.assign(byte_str, regex::optimize);
    word_rx.assign(word_str, regex::optimize);
    operand_1b_rx.assign(operand_1b_str, regex::optimize);
    operand_2b_rx.assign(operand_2b_str, regex::optimize);
    imm_b_rx.assign(imm_b_str, regex::optimize);
    imm_w_rx.assign(imm_w_str, regex::optimize);
    regdir_b_rx.assign(regdir_b_str, regex::optimize);
    regdir_w_rx.assign(regdir_w_str, regex::optimize);
    regind_rx.assign(regind_str, regex::optimize);
    regindoff_rx.assign(regindoff_str, regex::optimize);
    regindsym_rx.assign(regindsym_str, regex::optimize);
    memsym_rx.assign(memsym_str, regex::optimize);
    memabs_rx.assign(memabs_str, regex::optimize);
    directive_rx.assign(directive_str, regex::optimize);
    zeroaddr_rx.assign(zeroaddr_str, regex::optimize);
    oneaddr_rx.assign(oneaddr_str, regex::optimize);
    twoaddr_rx.assign(twoaddr_str, regex::optimize);
    expr_rx.assign(expr_str, regex::optimize);
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

bool Lexer::match_symbol(const string &str, string &result)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, symbol_rx, tokens);
    if (res) result = tokens[0];
    return res;
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

bool Lexer::match_operand_1b(const string &str)
{
    tokens_t tokens;
    return Lexer::tokenize_content(str, operand_1b_rx, tokens);
}

bool Lexer::match_operand_2b(const string &str, string &offset)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, operand_2b_rx, tokens);
    if (res) offset = tokens[0];
    return res;
}

bool Lexer::match_imm_b(const string &str, string &value)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, imm_b_rx, tokens);
    if (res) value = tokens[0];
    return res;
}

bool Lexer::match_imm_w(const string &str, string &value)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, imm_w_rx, tokens);
    if (res) value = tokens[0];
    return res;
}

bool Lexer::match_regdir_b(const string &str, string &reg)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, regdir_b_rx, tokens);
    if (res) reg = tokens[0];
    return res;
}

bool Lexer::match_regdir_w(const string &str, string &reg)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, regdir_w_rx, tokens);
    if (res) reg = tokens[0];
    return res;
}

bool Lexer::match_regind(const string &str, string &reg)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, regind_rx, tokens);
    if (res) reg = tokens[0];
    return res;
}

bool Lexer::match_regindoff(const string &str, string &reg, string &offset)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, regindoff_rx, tokens);
    if (res) 
    {
        reg = tokens[0];
        offset = tokens[1];
    }
    return res;
}

bool Lexer::match_regindsym(const string &str, string &reg, string &symbol)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, regindsym_rx, tokens);
    if (res)
    {
        reg = tokens[0];
        symbol = tokens[1];
    }
    return res;
}

bool Lexer::match_memsym(const string &str, string &symbol)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, memsym_rx, tokens);
    if (res) symbol = tokens[0];
    return res;
}

bool Lexer::match_memabs(const string &str, string &address)
{
    tokens_t tokens;
    bool res = Lexer::tokenize_content(str, memabs_rx, tokens);
    if (res) address = tokens[0];
    return res;
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

bool Lexer::tokenize_expression(const string &str, tokens_t &tokens)
{
    smatch m;
    string tmp = str;
    bool valid = false;
    while (!tmp.empty())
        if (!regex_match(tmp, m, expr_rx)) break;
        else
        {
            tokens.push_back(m.str(2));
            tmp = tmp.substr(m.str(1).length());
            if (tmp.empty()) valid = true;
        }
    return valid;
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