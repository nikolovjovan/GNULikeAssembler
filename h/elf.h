#ifndef _ELF_H
#define _ELF_H

// Standard ELF types

#include <stdint.h>

// Type for a 8-bit quantity
typedef uint8_t     Elf16_Half;

// Type for signed and unsigned 16-bit quantities
typedef uint16_t    Elf16_Word;
typedef int16_t     Elf16_Sword;

// Type of addresses
typedef uint16_t    Elf16_Addr;

// Type of file offsets
typedef int16_t     Elf16_Off;

// Type for section indices, which are 16-bit quantities
typedef uint16_t    Elf16_Section;

// The ELF file header which appears at the start of every ELF file

#define EI_NIDENT 16

typedef struct elf16_ehdr
{
    unsigned char   e_ident[EI_NIDENT]; // Magic number and other info
    Elf16_Half      e_type;             // Object file type
    Elf16_Half      e_machine;          // Architecture
    Elf16_Word      e_version;          // Object file version
    Elf16_Addr      e_entry;            // Entry point virtual address
    Elf16_Off       e_phoff;            // Program header table file offset
    Elf16_Off       e_shoff;            // Section header table file offset
    Elf16_Word      e_flags;            // Processor-specific flags
    Elf16_Half      e_ehsize;           // ELF header size in bytes
    Elf16_Half      e_phentsize;        // Program header table entry size
    Elf16_Half      e_phnum;            // Program header table entry count
    Elf16_Half      e_shentsize;        // Section header table entry size
    Elf16_Half      e_shnum;            // Section header table entry count
    Elf16_Half      e_shstrndx;         // Section header string table index
} Elf16_Ehdr;

// Indices into the e_ident array
enum elf16_ident
{
    EI_MAG0         = 0,    // 0x7F
    EI_MAG1         = 1,    // 'E'
    EI_MAG2         = 2,    // 'L'
    EI_MAG3         = 3,    // 'F'
    EI_CLASS        = 4,    // Architecture (16)
    EI_DATA         = 5,    // Byte Order
    EI_VERSION      = 6,    // ELF Version, value must be EV_CURRENT!
    EI_PAD          = 7     // Padding
};

#define ELFMAG0 0x7f    // Magic number byte 0: e_ident[EI_MAG0]
#define ELFMAG1 'E'     // Magic number byte 1: e_ident[EI_MAG1]
#define ELFMAG2 'L'     // Magic number byte 2: e_ident[EI_MAG2]
#define ELFMAG3 'F'     // Magic number byte 3: e_ident[EI_MAG3]

// Conglomeration of the identification bytes, for easy testing as a word
#define ELFMAG  "\177ELF"
#define SELFMAG 4

#define ELFCLASSNONE    0   // Invalid class
#define ELFCLASS16      1   // 16-bit Von-Neumann Architecture
#define ELFCLASSNUM     2

#define ELFDATANONE     0   // Invalid data encoding
#define ELFDATA2LSB     1   // 2's complement, little endian
#define ELFDATANUM      2

// Legal values for e_type (object file type)

#define ET_NONE     0       // No file type
#define ET_REL      1       // Relocatable file
#define ET_EXEC     2       // Executable file
#define ET_DYN      3       // Shared object file
#define ET_CORE     4       // Core file
#define ET_LOOS     0xfe00  // OS-specific range start
#define ET_HIOS     0xfeff  // OS-specific range end
#define ET_LOPROC   0xff00  // Processor-specific range start
#define ET_HIPROC   0xffff  // Processor-specific range end

// Legal values for e_machine (architecture)

#define EM_NONE     0   // No machine
#define EM_VN16     1   // 16-bit Von-Neumann
#define EM_NUM      2

// Legal values for e_version (version)

#define EV_NONE     0   // Invalid ELF version
#define EV_CURRENT  1   // Current version
#define EV_NUM      2

// Section header

typedef struct elf16_shdr
{
    Elf16_Word  sh_name;        // Section name (string tbl index)
    Elf16_Word  sh_type;        // Section type
    Elf16_Word  sh_flags;       // Section flags
    Elf16_Addr  sh_addr;        // Section virtual addr at execution
    Elf16_Off   sh_offset;      // Section file offset
    Elf16_Word  sh_size;        // Section size in bytes
    Elf16_Word  sh_link;        // Link to another section
    Elf16_Word  sh_info;        // Additional section information
    Elf16_Word  sh_addralign;   // Section alignment
    Elf16_Word  sh_entsize;     // Entry size if section holds table
} Elf16_Shdr;

// Special section indices

#define SHN_UNDEF       0       // Undefined section
#define SHN_LORESERVE   0xff00  // Start of reserved indices
#define SHN_LOPROC      0xff00  // Start of processor-specific
#define SHN_HIPROC      0xff1f  // End of processor-specific
#define SHN_LOOS        0xff20  // Start of OS-specific
#define SHN_HIOS        0xff3f  // End of OS-specific
#define SHN_ABS         0xfff1  // Associated symbol is absolute
#define SHN_COMMON      0xfff2  // Associated symbol is common
#define SHN_HIRESERVE   0xffff  // End of reserved indices

// Legal values for sh_type (section type)

#define SHT_NULL        0   // Section header table entry unused
#define SHT_PROGBITS    1   // Program data (text and data)
#define SHT_SYMTAB      2   // Symbol table
#define SHT_STRTAB      3   // String table
#define SHT_NOBITS      8   // Program space with no data (bss)
#define SHT_REL         9   // Relocation entries, no addends

// Legal values for sh_flags (section flags)

#define SHF_WRITE       0x1     // Writable
#define SHF_ALLOC       0x2     // Occupies memory during execution
#define SHF_EXECINSTR   0x4     // Executable
#define SHF_INFO_LINK   0x40    // sh_info contains SHT index (used for reloc tables)

// Symbol table entry

typedef struct elf16_sym
{
    Elf16_Word      st_name;    // Symbol name (string tbl index)
    Elf16_Addr      st_value;   // Symbol value
    Elf16_Word      st_size;    // Symbol size
    unsigned char   st_info;    // Symbol type and binding
    unsigned char   st_other;   // No defined meaning, 0
    Elf16_Section   st_shndx;   // Section index
} Elf16_Sym;

// How to extract and insert information held in the st_info field

#define ELF16_ST_BIND(val)          (((unsigned char) (val)) >> 4)
#define ELF16_ST_TYPE(val)          ((val) & 0xf)
#define ELF16_ST_INFO(bind, type)   (((bind) << 4) + ((type) & 0xf))

// Legal values for ST_BIND subfield of st_info (symbol binding)

#define STB_LOCAL   0   // Local symbol
#define STB_GLOBAL  1   // Global symbol
#define STB_WEAK    2   // Weak symbol
#define STB_NUM     3   // Number of defined types
#define STB_LOOS    10  // Start of OS-specific
#define STB_HIOS    12  // End of OS-specific
#define STB_LOPROC  13  // Start of processor-specific
#define STB_HIPROC  15  // End of processor-specific

// Legal values for ST_TYPE subfield of st_info (symbol type)

#define STT_NOTYPE  0   // Symbol type is unspecified
#define STT_OBJECT  1   // Symbol is a data object
#define STT_FUNC    2   // Symbol is a code object
#define STT_SECTION 3   // Symbol associated with a section
#define STT_FILE    4   // Symbol's name is file name
#define STT_NUM     5   // Number of defined types
#define STT_LOOS    11  // Start of OS-specific
#define STT_HIOS    12  // End of OS-specific
#define STT_LOPROC  13  // Start of processor-specific
#define STT_HIPROC  15  // End of processor-specific

// Relocation table entry without addend (in section of type SHT_REL)

typedef struct elf16_rel
{
    Elf16_Addr  r_offset;   // Address
    Elf16_Word  r_info;     // Relocation type and symbol index
} Elf16_Rel;

// How to extract and insert information held in the r_info field

#define ELF16_R_SYM(val)        ((val) >> 8)
#define ELF16_R_TYPE(val)       ((val) & 0xff)
#define ELF16_R_INFO(sym, type) (((sym) << 8) + ((type) & 0xff))

// Program segment header

typedef struct elf16_phdr
{
    Elf16_Word  p_type;     // Segment type
    Elf16_Off   p_offset;   // Segment file offset
    Elf16_Addr  p_vaddr;    // Segment virtual address
    Elf16_Addr  p_paddr;    // Segment physical address
    Elf16_Word  p_filesz;   // Segment size in file
    Elf16_Word  p_memsz;    // Segment size in memory
    Elf16_Word  p_flags;    // Segment flags
    Elf16_Word  p_align;    // Segment alignment
} Elf16_Phdr;

// Legal values for p_type (segment type)

#define PT_NULL     0           // Program header table entry unused
#define PT_LOAD     1           // Loadable program segment
#define PT_DYNAMIC  2           // Dynamic linking information
#define PT_INTERP   3           // Program interpreter
#define PT_NOTE     4           // Auxiliary information
#define PT_SHLIB    5           // Reserved
#define PT_PHDR     6           // Entry for header table itself
#define PT_TLS      7           // Thread-local storage segment
#define PT_NUM      8           // Number of defined types
#define PT_LOOS     0x60000000  // Start of OS-specific
#define PT_HIOS     0x6fffffff  // End of OS-specific
#define PT_LOPROC   0x70000000  // Start of processor-specific
#define PT_HIPROC   0x7fffffff  // End of processor-specific

// Legal values for p_flags (segment flags)

#define PF_X        0x1         // Segment is executable
#define PF_W        0x2         // Segment is writable
#define PF_R        0x4         // Segment is readable
#define PF_MASKOS   0x0ff00000  // OS-specific
#define PF_MASKPROC 0xf0000000  // Processor-specific

// Von-Neumann 16-bit two-address CPU specific definitions

// VN relocs

#define R_VN_NONE   0   // No reloc
#define R_VN_16     1   // Direct 16 bit
#define R_VN_PC16   2   // PC relative 16 bit
// Keep this the last entry
#define R_VN_NUM    3

#endif  // elf.h