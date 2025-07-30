#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/task/scheduler.h>
#include <BasicRenderer.h>
#include <VFS/vfs.h>
#include <users.h>



namespace ELFLoader{

    #define EI_NIDENT 4
    
    #define PT_NULL     0
    #define PT_LOAD     1
    #define PT_DYNAMIC  2
    #define PT_INTERP   3
    #define PT_NOTE     4
    #define PT_SHLIB    5
    #define PT_PHDR     6
    #define PT_LOPROC   0x70000000
    #define PT_HIPROC   0x7fffffff


    #define AT_NULL   0	/* end of vector */
    #define AT_IGNORE 1	/* entry should be ignored */
    #define AT_EXECFD 2	/* file descriptor of program */
    #define AT_PHDR   3	/* program headers for program */
    #define AT_PHENT  4	/* size of program header entry */
    #define AT_PHNUM  5	/* number of program headers */
    #define AT_PAGESZ 6	/* system page size */
    #define AT_BASE   7	/* base address of interpreter */
    #define AT_FLAGS  8	/* flags */
    #define AT_ENTRY  9	/* entry point of program */
    #define AT_NOTELF 10	/* program is not ELF */
    #define AT_UID    11	/* real uid */
    #define AT_EUID   12	/* effective uid */
    #define AT_GID    13	/* real gid */
    #define AT_EGID   14	/* effective gid */
    #define AT_PLATFORM 15  /* string identifying CPU for optimizations */
    #define AT_HWCAP  16    /* arch dependent hints at CPU capabilities */
    #define AT_CLKTCK 17	/* frequency at which times() increments */
    /* AT_* values 18 through 22 are reserved */
    #define AT_SECURE 23   /* secure mode boolean */
    #define AT_BASE_PLATFORM 24	/* string identifying real platform, may differ from AT_PLATFORM. */
    #define AT_RANDOM 25	/* address of 16 random bytes */
    #define AT_HWCAP2 26	/* extension of AT_HWCAP */
    #define AT_RSEQ_FEATURE_SIZE	27	/* rseq supported feature size */
    #define AT_RSEQ_ALIGN		28	/* rseq allocation alignment */
    #define AT_HWCAP3 29	/* extension of AT_HWCAP */
    #define AT_HWCAP4 30	/* extension of AT_HWCAP */

    #define AT_EXECFN  31	/* filename of program */


    #define HWCAP_X86_FPU        (1 << 0)
    #define HWCAP_X86_VME        (1 << 1)
    #define HWCAP_X86_DE         (1 << 2)
    #define HWCAP_X86_PSE        (1 << 3)
    #define HWCAP_X86_TSC        (1 << 4)
    #define HWCAP_X86_MSR        (1 << 5)
    #define HWCAP_X86_PAE        (1 << 6)
    #define HWCAP_X86_MCE        (1 << 7)
    #define HWCAP_X86_CX8        (1 << 8)
    #define HWCAP_X86_APIC       (1 << 9)
    #define HWCAP_X86_SEP        (1 << 11)
    #define HWCAP_X86_MTRR       (1 << 12)
    #define HWCAP_X86_PGE        (1 << 13)
    #define HWCAP_X86_MCA        (1 << 14)
    #define HWCAP_X86_CMOV       (1 << 15)
    #define HWCAP_X86_PAT        (1 << 16)
    #define HWCAP_X86_PSE        (1 << 17)
    #define HWCAP_X86_PSN        (1 << 18)
    #define HWCAP_X86_CLFSH      (1 << 19)
    #define HWCAP_X86_DS         (1 << 21)
    #define HWCAP_X86_ACPI       (1 << 22)
    #define HWCAP_X86_MMX        (1 << 23)
    #define HWCAP_X86_FXSR       (1 << 24)
    #define HWCAP_X86_SSE        (1 << 25)
    #define HWCAP_X86_SSE2       (1 << 26)
    #define HWCAP_X86_SS         (1 << 27)
    #define HWCAP_X86_HTT        (1 << 28)
    #define HWCAP_X86_TM         (1 << 29)
    #define HWCAP_X86_PBE        (1 << 30)

    #define PF_R		0x4
    #define PF_W		0x2
    #define PF_X		0x1

    typedef struct {
        uint64_t r_offset;   // Where to write
        uint64_t r_info;     // Symbol + type
        int64_t  r_addend;   // Add this to the symbol value
    } Elf64_Rela;

    typedef struct {
        int64_t d_tag;     // What kind of info this is
        union {
            uint64_t d_val;
            uint64_t d_ptr;
        };
    } Elf64_Dyn;

    typedef struct {
        uint64_t    st_name;   // Index into string table for symbol name
        unsigned char st_info;   // Symbol type and binding
        unsigned char st_other;  // Visibility (usually 0)
        uint32_t    st_shndx;  // Section index (special values or real section index)
        uint64_t    st_value;  // Symbol value (e.g., function address or variable address)
        uint64_t   st_size;   // Size of the object (if known)
    } Elf64_Sym;

    struct Elf32_Ehdr{
        uint8_t e_ident[EI_NIDENT];
        uint8_t ei_class;
        uint8_t ei_data;
        uint8_t ei_version;
        uint8_t ei_osabi;
        uint8_t ei_abiversion;
        uint8_t rsv[7];
        uint16_t e_type; // (1 = relocatable, 2 = executable, 3 = shared, 4 = core) 
        uint16_t e_machine;
        uint32_t e_version;
        uint32_t e_entry;
        uint32_t e_phoff;
        uint32_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehsize;
        uint16_t e_phentsize;
        uint16_t e_phum; //In header
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    } __attribute__((packed));

    struct Elf64_Ehdr{
        uint8_t e_ident[EI_NIDENT];
        uint8_t ei_class;
        uint8_t ei_data;
        uint8_t ei_version;
        uint8_t ei_osabi;
        uint8_t ei_abiversion;
        uint8_t rsv[7];
        uint16_t e_type; // (1 = relocatable, 2 = executable, 3 = shared, 4 = core) 
        uint16_t e_machine;
        uint32_t e_version;
        uint64_t e_entry;
        uint64_t e_phoff;
        uint64_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehsize;
        uint16_t e_phentsize;
        uint16_t e_phnum; //In header
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    } __attribute__((packed));

    struct ProgramHdr32{
        uint32_t p_type;
        uint32_t p_offset;
        uint32_t p_vaddr;
        uint32_t p_paddr;
        uint32_t p_filesz;
        uint32_t p_memsz;
        uint32_t p_flags;
        uint32_t p_align;
    };

    struct ProgramHdr64{
        uint32_t p_type;
        uint32_t p_flags;
        uint64_t p_offset;
        uint64_t p_vaddr;
        uint64_t p_paddr;
        uint64_t p_filesz;
        uint64_t p_memsz;
        uint64_t p_align;
    };

    struct SectionHdr32{
        uint32_t sh_name;
        uint32_t sh_type;
        uint32_t sh_flags;
        uint32_t sh_addr;
        uint32_t sh_offset;
        uint32_t sh_size;
        uint32_t sh_link;
        uint32_t sh_info;
        uint32_t sh_addralign;
        uint32_t sh_entsize;
    };

    struct SectionHdr64{
        uint32_t sh_name;
        uint32_t sh_type;
        uint64_t sh_flags;
        uint64_t sh_addr;
        uint64_t sh_offset;
        uint64_t sh_size;
        uint32_t sh_link;
        uint32_t sh_info;
        uint64_t sh_addralign;
        uint64_t sh_entsize;
    };


    int Load(vnode_t* file, int priority, user_t* user, vnode_t* tty, session::session_t* session = nullptr, task_t* parent = nullptr);
    int kexecve(vnode_t* node, int argc_src, char** argv_src, char** envp_src);
}

