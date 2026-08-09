#include <elf/elf.h>
#include <elf/headers.h>
#include <filepath.h>
#include <kerrno.h>
#include <elf/hwcap.h>
#include <alloca.h>


const char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};

void setup_stack(task_t* task, int argc, char* argv[], char* envp[], auxv_t auxv_entries[]){
    if (task->registers.rsp == 0) task->registers.rsp = task->userspace_stack;
    task->registers.rsp &= ~0xFULL;
    // --- Count and Copy String Data onto the User Stack ---

    int num_of_env_strings = 0;
    for (int i = 0; envp[i] != nullptr; i++) num_of_env_strings++;

    char** copied_args = (char**)alloca(sizeof(char*) * argc);
    char** copied_envs = (char**)alloca(sizeof(char*) * num_of_env_strings);

    // Copy Arguments (argv)
    for (int i = 0; i < argc; i++){
        int size = strlen(argv[i]);
        task->registers.rsp -= size + 1;
        task->registers.rsp &= ~0xFUL;

        task->write_to_userspace((void *)task->registers.rsp, argv[i], size + 1);
        copied_args[i] = (char*)task->registers.rsp;
    }

    // Copy Environment Variables (envp)
    for (int i = 0; i < num_of_env_strings; i++){
        int size = strlen(envp[i]);
        task->registers.rsp -= size + 1;
        task->registers.rsp &= ~0xFUL;

        task->write_to_userspace((void *)task->registers.rsp, envp[i], size + 1);
        copied_envs[i] = (char*)task->registers.rsp;
    }

    task->registers.rsp &= ~0xFUL;

    uint8_t* sp = (uint8_t*)task->registers.rsp;

    // Count AUXV entries
    int num_of_auxv_entries = 0;
    for (int i = 0; auxv_entries[i].a_type != AT_NULL; i++) num_of_auxv_entries++;

    uint64_t total_size =   ((num_of_auxv_entries + 1) * sizeof(uint64_t) * 2)/*AUXV*/ +
                            ((num_of_env_strings + 1) * sizeof(uint64_t))/*ENVP*/ +
                            ((argc + 1) * sizeof(uint64_t))/*ARGV*/ +
                            sizeof(uint64_t)/*ARGC*/;


    if (total_size & 0xF) sp -= sizeof(uint64_t); // Push 8 bytes to the stack to ensure the final rsp is aligned

    PUSH_TO_STACK(task, sp, uint64_t, 0);
    PUSH_TO_STACK(task, sp, uint64_t, AT_NULL);

    for (int i = num_of_auxv_entries - 1; i >= 0; i--){
        auxv_t auxv = auxv_entries[i];
        PUSH_TO_STACK(task, sp, uint64_t, auxv.a_val);
        PUSH_TO_STACK(task, sp, uint64_t, auxv.a_type);
    }

    // Push ENVP pointers
    PUSH_TO_STACK(task, sp, uint64_t, 0);
    for (int i = num_of_env_strings - 1; i >= 0; i--){
        PUSH_TO_STACK(task, sp, uint64_t, (uint64_t)copied_envs[i]);
    }
    task->registers.rdx = (uint64_t)sp; // RDX points to the first ENV pointer (Expected by ABI)

    // Push ARGV pointers
    PUSH_TO_STACK(task, sp, uint64_t, 0);
    for (int i = argc - 1; i >= 0; i--){
        PUSH_TO_STACK(task, sp, uint64_t, (uint64_t)copied_args[i]);
    }
    task->registers.rsi = (uint64_t)sp; // RSI points to the first ARGV pointer (Expected by ABI)

    // Push ARGC
    PUSH_TO_STACK(task, sp, uint64_t, argc);
    task->registers.rdi = argc;

    // Set Final RSP
    task->registers.rsp = (uint64_t)sp;
}


bool verify_header(elf64_ehdr* hdr){
    if (hdr == nullptr || memcmp(hdr->e_ident, ELF_MAGIC, EI_NIDENT)) return false;
    return true;
}

void load_pheader(task_t *task, vnode_t *node, program_header64 *pheader, uint64_t base){
    uint64_t flags = (1ULL << PT_Flag::User) | (1ULL << PT_Flag::Present);
    if (pheader->p_flags & PF_W) flags |= (1ULL << PT_Flag::Write);

    uint64_t load_addr = base + pheader->p_vaddr;
    int errno;
    
    // Allocate the non-contiguous physical pages in the VMM
    task->vmm->allocate(load_addr, pheader->p_memsz, flags, errno);

    // Stream the data from the file to userspace in 4KB chunks
    char *buffer = new char[4096];
    uint64_t remaining_file = pheader->p_filesz;
    uint64_t file_offset = pheader->p_offset;
    uint64_t dest_addr = load_addr;

    while (remaining_file > 0) {
        uint64_t chunk = (remaining_file > 4096) ? 4096 : remaining_file;
        node->read(buffer, chunk, file_offset);
        task->write_to_userspace((void*)dest_addr, buffer, chunk);
        
        file_offset += chunk;
        dest_addr += chunk;
        remaining_file -= chunk;
    }

    // Zero out the BSS (memsz > filesz)
    if (pheader->p_memsz > pheader->p_filesz) {
        uint64_t bss_size = pheader->p_memsz - pheader->p_filesz;
        memset(buffer, 0, 4096); // Re-use the buffer as a zero-block

        while (bss_size > 0) {
            uint64_t chunk = (bss_size > 4096) ? 4096 : bss_size;
            task->write_to_userspace((void*)dest_addr, buffer, chunk);
            dest_addr += chunk;
            bss_size -= chunk;
        }
    }

    delete buffer;
}

int load_elf(task_t *task, vnode_t *node, int argc, char *argv[], const char *exec_path){
    // Allocate the header
    elf64_ehdr* header = new elf64_ehdr;

    // Load the header
    node->read(header, sizeof(elf64_ehdr), 0);

    // Verify the header
    if (!verify_header(header)) {
        delete header;
        return -ENOEXEC;
    }

    // Not hacky at all wym
    char parent_path[512];
    split_path(exec_path, parent_path, task->name);

    // Allocate the program headers
    program_header64 *phdrs = new program_header64[header->e_phnum];

    // Read the pheaders
    node->read(phdrs, sizeof(program_header64) * header->e_phnum, header->e_phoff);

    // Load the headers
    bool interpreter_found = false;
    char interp_path[256];

    for (int i = 0; i < header->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_pheader(task, node, &phdrs[i], 0);
        } else if (phdrs[i].p_type == PT_INTERP) {
            interpreter_found = true;
            node->read(interp_path, phdrs[i].p_filesz, phdrs[i].p_offset);
        }
    }

    delete phdrs;

    uint64_t entry_point = header->e_entry;

    // Load the interpreter
    if (interpreter_found){
        vnode_t *interpreter = vfs::resolve_path(interp_path);

        if (!interpreter) {
            delete header;
            return -ENOENT;
        }

        elf64_ehdr* iheader = new elf64_ehdr;

        // Load the header
        interpreter->read(iheader, sizeof(elf64_ehdr), 0);

        // Verify the header
        if (!verify_header(iheader)) {
            delete header;
            delete iheader;
            return -ENOEXEC;
        }

        uint64_t base = 0x100000000000;
        entry_point = base + iheader->e_entry;

        /* Load its pheaders... pretty much exactly what we did above */
        // Allocate the program headers
        program_header64 *iphdrs = new program_header64[iheader->e_phnum];

        // Read the pheaders
        interpreter->read(iphdrs, sizeof(program_header64) * iheader->e_phnum, iheader->e_phoff);

        // Load the headers
        for (int i = 0; i < iheader->e_phnum; i++) {
            if (iphdrs[i].p_type == PT_LOAD) {
                load_pheader(task, interpreter, &iphdrs[i], base);
            }
        }

        delete iphdrs;
        interpreter->close();
    }

    uint64_t phdr_vaddr = header->e_phoff;
    uint8_t random_bytes[16] = {0};
    task->registers.rsp = task->userspace_stack;
    task->registers.rsp -= 16;
    task->write_to_userspace((void*)task->registers.rsp, random_bytes, 16);
    uint64_t at_random_ptr = task->registers.rsp;

    int path_len = strlen(exec_path) + 1;
    task->registers.rsp -= path_len;
    task->write_to_userspace((void*)task->registers.rsp, (void*)exec_path, path_len);
    uint64_t at_execfn_ptr = task->registers.rsp;

    auxv_t auxv_entries[] = {
        {AT_HWCAP, get_hwcap_x86()},
        {AT_PAGESZ, 0x1000},
        {AT_PHDR, phdr_vaddr},
        {AT_PHENT, header->e_phentsize},
        {AT_PHNUM, header->e_phnum},
        {AT_BASE, 0x100000000000},
        {AT_FLAGS, 0},
        {AT_ENTRY, header->e_entry},
        /*{AT_UID, (uint64_t)task->ruid},
        {AT_EUID, (uint64_t)task->euid},
        {AT_GID, (uint64_t)task->rgid},
        {AT_EGID, (uint64_t)task->egid},*/
        {AT_SECURE, 0},
        {AT_RANDOM, at_random_ptr},
        {AT_EXECFN, at_execfn_ptr},
        {AT_NULL, 0}
    };

    const char* envp[] = { "USER=root", nullptr };  
    setup_stack(task, argc, argv, (char**)envp, auxv_entries);

    task->registers.rip = entry_point;
    return 0;
}