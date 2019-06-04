#include "parser.h"

#include <vector>

using std::string;
using std::vector;

Parser::Parser()
{
    // Initializing maps
    for (unsigned i = 0; i < DIRECTIVE_CNT; ++i)
        directive_map[directive_strings[i]] = i;
    for (unsigned i = 0; i < INSTRUCTION_CNT; ++i)
        instruction_map[instruction_strings[i]];
}

bool Parser::parse_line(const std::string &line, Line &result)
{
    vector<string> tokens();
}

bool Parser::parse_content(const std::string &content, Content &result)
{
}