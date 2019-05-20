#ifndef _ASM_H
#define _ASM_H

#include <fstream>

class Assembler
{
public:
    Assembler(std::string input_file, std::string output_file, bool binary = false);
    ~Assembler();

    bool assemble();
private:
    std::string input_file, output_file;
    std::ifstream input;
    std::ofstream output;
    bool binary;
};

#endif  /* asm.h */