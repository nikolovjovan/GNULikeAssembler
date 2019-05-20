#include "asm.h"

Assembler::Assembler(std::string input_file, std::string output_file, bool binary)
{
    this->input_file = input_file;
    this->output_file = output_file;
    this->binary = binary;
}

Assembler::~Assembler()
{
    
}

bool Assembler::assemble()
{
    return true; // temp
}