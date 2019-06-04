#include <iostream>
#include <fstream>
#include <string>

#include "assembler.h"

using std::cerr;
using std::cout;
using std::ifstream;
using std::ofstream;
using std::string;

void show_usage(const string &program_name)
{
    cout << "Usage: " << program_name << " [options] file...\n"; // [-e] [-o <output_file>] <input_file>\n";
    cout << "Options:\n";
    cout << "  -e\t\tOutput in binary format for use in the provided emulator.\n";
    cout << "  -o <file>\tPlace the output into <file>.\n";
}

bool file_exists(const string &name)
{
    if (FILE *file = fopen(name.c_str(), "r"))
    {
        fclose(file);
        return true;
    }
    return false;
}

string get_output_file(const string &input_file)
{
    size_t lastslash = 0, lastdot = 0;
    for (size_t i = 0; i < input_file.length(); ++i)
    {
        if (input_file[i] == '/')
            lastslash = i;
        else if (input_file[i] == '.')
            lastdot = i;
    }
    if (lastslash > lastdot)
        return input_file + ".o";
    return input_file.substr(0, lastdot) + ".o";
}

int main(int argc, char *argv[])
{
    if (argc < 2) // zero arguments
    {
        cerr << "ERROR: No input file!\n";
        show_usage(argv[0]);
        return 1;
    }

    string input_file, output_file;
    bool eflag = false;

    for (int i = 1; i < argc; ++i)
    {
        if (string(argv[i]) == "-e")
            eflag = true; // set -e flag
        else if (string(argv[i]) == "-o")
        {
            if (i == argc - 1) // -o flag is the last argument
            {
                cerr << "ERROR: Invalid output file switch position!\n";
                show_usage(argv[0]);
                return 1;
            }
            output_file = argv[++i]; // set output file
        }
        else if (input_file.empty())
            input_file = argv[i]; // set input file
        else
        {
            cerr << "ERROR: Invalid number of input files!\n";
            show_usage(argv[0]);
            return 1;
        }
    }

    if (!std::ifstream(input_file)) // invalid input file
    {
        cerr << "ERROR: Input file: " << input_file << " does not exist or cannot be opened for reading!\n";
        return 2;
    }
    if (output_file.empty()) // try getting output file name from input file
        output_file = get_output_file(input_file);
    if (!std::ofstream(output_file)) // invalid output file
    {
        cerr << "ERROR: Output file: " << output_file << " cannot be opened for writing!\n";
        return 3;
    }

    Assembler assembler(input_file, output_file, eflag);
    if (!assembler.assemble())
    {
        cerr << "ERROR: Failed to assemble: " << input_file << "!\n";
        return 0;
    }
    cout << "Successfully assembled: " << input_file << "!\n";
    return 0;
}