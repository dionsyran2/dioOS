#include <helpers/elf.h>
#include <helpers/elf_defs.h>
#include <memory.h>
#include <memory/heap.h>
#include <cstr.h>
#include <paging/PageFrameAllocator.h>
#include <kstdio.h>
#include <alloca.h>
#include <kerrno.h>
#include <math.h>

#define random() 0

const char ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};

uint32_t get_hwcap_x86() {
    uint32_t eax, ebx, ecx, edx;
    uint32_t hwcap = 0;

    // CPUID leaf 0x00000001: Feature Information
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1), "c"(0));

    if (edx & (1 << 0))  hwcap |= HWCAP_X86_FPU;
    if (edx & (1 << 1))  hwcap |= HWCAP_X86_VME;
    if (edx & (1 << 2))  hwcap |= HWCAP_X86_DE;
    if (edx & (1 << 3))  hwcap |= HWCAP_X86_PSE;
    if (edx & (1 << 4))  hwcap |= HWCAP_X86_TSC;
    if (edx & (1 << 5))  hwcap |= HWCAP_X86_MSR;
    if (edx & (1 << 6))  hwcap |= HWCAP_X86_PAE;
    if (edx & (1 << 7))  hwcap |= HWCAP_X86_MCE;
    if (edx & (1 << 8))  hwcap |= HWCAP_X86_CX8;
    if (edx & (1 << 9))  hwcap |= HWCAP_X86_APIC;
    if (edx & (1 << 11)) hwcap |= HWCAP_X86_SEP;
    if (edx & (1 << 12)) hwcap |= HWCAP_X86_MTRR;
    if (edx & (1 << 13)) hwcap |= HWCAP_X86_PGE;
    if (edx & (1 << 14)) hwcap |= HWCAP_X86_MCA;
    if (edx & (1 << 15)) hwcap |= HWCAP_X86_CMOV;
    if (edx & (1 << 16)) hwcap |= HWCAP_X86_PAT;
    if (edx & (1 << 18)) hwcap |= HWCAP_X86_PSN;
    if (edx & (1 << 19)) hwcap |= HWCAP_X86_CLFSH;
    if (edx & (1 << 21)) hwcap |= HWCAP_X86_DS;
    if (edx & (1 << 22)) hwcap |= HWCAP_X86_ACPI;
    if (edx & (1 << 23)) hwcap |= HWCAP_X86_MMX;
    if (edx & (1 << 24)) hwcap |= HWCAP_X86_FXSR;
    if (edx & (1 << 25)) hwcap |= HWCAP_X86_SSE;
    if (edx & (1 << 26)) hwcap |= HWCAP_X86_SSE2;
    if (edx & (1 << 27)) hwcap |= HWCAP_X86_SS;
    if (edx & (1 << 28)) hwcap |= HWCAP_X86_HTT;
    if (edx & (1 << 29)) hwcap |= HWCAP_X86_TM;
    if (edx & (1 << 31)) hwcap |= HWCAP_X86_PBE;

    return hwcap;
}

void setup_stack(task_t* task, int argc, char* argv[], char* envp[], auxv_t auxv_entries[]){
    if (task->registers.rsp == 0) task->registers.rsp = task->user_stack_top;
    task->registers.rsp &= ~0xFULL;
    // --- Phase 1: Count and Copy String Data onto the User Stack (Downward) ---
    // Note: The stack pointer (task->registers.rsp) moves down during this phase.
    
    int num_of_env_strings = 0;
    for (int i = 0; envp[i] != nullptr; i++) num_of_env_strings++;
    
    char** copied_args = (char**)alloca(sizeof(char*) * argc); 
    char** copied_envs = (char**)alloca(sizeof(char*) * num_of_env_strings); 

    // A. Copy Arguments (argv)
    for (int i = 0; i < argc; i++){
        int size = strlen(argv[i]);
        task->registers.rsp -= size + 1;
        task->registers.rsp &= ~0xFUL;
        
        task->write_memory((void *)task->registers.rsp, argv[i], size + 1);
        copied_args[i] = (char*)task->registers.rsp;
    }

    // B. Copy Environment Variables (envp)
    for (int i = 0; i < num_of_env_strings; i++){
        int size = strlen(envp[i]);
        task->registers.rsp -= size + 1;
        task->registers.rsp &= ~0xFUL;

        task->write_memory((void *)task->registers.rsp, envp[i], size + 1);
        copied_envs[i] = (char*)task->registers.rsp;
    }

    task->registers.rsp &= ~0xFUL; 
    
    uint8_t* sp = (uint8_t*)task->registers.rsp;
    
    // 1. Count AUXV entries
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

    // B. Push ENVP pointers
    PUSH_TO_STACK(task, sp, uint64_t, 0);
    for (int i = num_of_env_strings - 1; i >= 0; i--){
        PUSH_TO_STACK(task, sp, uint64_t, (uint64_t)copied_envs[i]);
    }
    task->registers.rdx = (uint64_t)sp; // RDX points to the first ENV pointer (Expected by ABI)

    // C. Push ARGV pointers
    PUSH_TO_STACK(task, sp, uint64_t, 0);
    for (int i = argc - 1; i >= 0; i--){
        PUSH_TO_STACK(task, sp, uint64_t, (uint64_t)copied_args[i]);
    }
    task->registers.rsi = (uint64_t)sp; // RSI points to the first ARGV pointer (Expected by ABI)

    // D. Push ARGC
    PUSH_TO_STACK(task, sp, uint64_t, argc); 
    task->registers.rdi = argc;

    // E. Set Final RSP
    task->registers.rsp = (uint64_t)sp;
}

bool verify_header(elf64_ehdr* hdr){
    if (hdr == nullptr || memcmp(hdr->e_ident, ELF_MAGIC, EI_NIDENT)) return false;

    return true;
}

char* get_interpreter(void* file){
    elf64_ehdr* ehdr = (elf64_ehdr*)file;

    for (int i = 0; i < ehdr->e_phnum; i++){
        program_header64* phdr = &(((program_header64*)((uint64_t)file + ehdr->e_phoff))[i]);
        if (phdr->p_type == PT_INTERP){
            return (char*)((uint64_t)file + phdr->p_offset);
        }
    }
    return nullptr;
}


void load_header(task_t* task, uint64_t base, void* file, program_header64* hdr){
    size_t align = hdr->p_align >= 0x1000 ? hdr->p_align : 0x1000;
    uint64_t seg_start = hdr->p_vaddr & ~(align - 1);
    size_t offset = hdr->p_vaddr - seg_start;
    size_t memsz = hdr->p_memsz + offset;
    size_t page_count = DIV_ROUND_UP(memsz, 0x1000);

    void* m = GlobalAllocator.RequestPages(page_count);
    memset(m, 0, page_count * 0x1000);

    uint64_t virt_base = base + seg_start;
    //serialf("Mapping [%llx, %llx) to [%llx, %llx)\n", virt_base, virt_base + memsz, virtual_to_physical((uint64_t)m), virtual_to_physical((uint64_t)m) + memsz);

    for (size_t o = 0; o < page_count; o++) {
        uint64_t v_addr = virt_base + (o * 0x1000);
        uint64_t page_address = (uint64_t)m + (o * 0x1000);
        uint64_t physical = virtual_to_physical(page_address);

        task->ptm->MapMemory((void*)v_addr, (void*)physical);
        task->ptm->SetFlag((void*)v_addr, PT_Flag::User, true);
        if ((hdr->p_flags & PF_R) && (hdr->p_flags & PF_W) == 0) {
            task->ptm->SetFlag((void*)v_addr, PT_Flag::Write, false);
        }else{
            task->ptm->SetFlag((void*)v_addr, PT_Flag::Write, true);
        }


        if ((hdr->p_flags & PF_X) == 0) {
            task->ptm->SetFlag((void*)v_addr, PT_Flag::NX, true);
        }else{
            task->ptm->SetFlag((void*)v_addr, PT_Flag::NX, false);
        }

        int flags = hdr->p_flags & PF_W ? VM_FLAG_RW : 0;
        flags |= hdr->p_flags & PF_W ? VM_FLAG_RW : 0;
        flags |= hdr->p_flags & PF_X ? 0 : VM_FLAG_NX;
        flags |= VM_FLAG_US;
        flags |= VM_FLAG_COW;

        task->vm_tracker.mark_allocation(v_addr, PAGE_SIZE, flags);
    }

    uint8_t* dest = (uint8_t*)m + offset;
    memcpy(dest, (void*)((uint64_t)file + hdr->p_offset), hdr->p_filesz);
}

uint64_t load_in_memory(task_t* task, uint64_t base, void* file, uint64_t* lowest_address){
    elf64_ehdr* ehdr = (elf64_ehdr*)file;
    uint64_t lowest = UINT64_MAX;

    for (int i = 0; i < ehdr->e_phnum; i++){
        program_header64* phdr = &(((program_header64*)((uint64_t)file + ehdr->e_phoff))[i]);
        switch (phdr->p_type){
            case PT_LOAD: {
                if ((phdr->p_vaddr + base) < lowest) lowest = phdr->p_vaddr + base;
                load_header(task, base, file, phdr);
                break;
            }
        }
    }

    if (lowest_address != nullptr) *lowest_address = lowest;
    return base + ehdr->e_entry;
}

task_t* execute_elf(vnode_t* node, int argc, char* argv[], char* exec_path){
    node->open();

    void* exec_file = malloc(node->size);
    elf64_ehdr* exec_ehdr = (elf64_ehdr*)exec_file;

    if (node->read(0, node->size, exec_file) < 0) {free(exec_file); return nullptr;}
    
    if (!verify_header(exec_ehdr)) {free(exec_file); return nullptr;}

    task_t* task = task_scheduler::create_process(exec_path, nullptr, true, true, true);
    strcpy(task->executable_name, node->name);

    char* interp_path = get_interpreter(exec_file);

    uint64_t lowest_exec_address = 0;
    uint64_t exec_entry = load_in_memory(task, 0, exec_file, &lowest_exec_address);
    uint64_t task_entry = exec_entry;

    if (interp_path){

        vnode_t* interpreter = vfs::resolve_path(interp_path);
        if (interpreter == nullptr) {free(exec_file); return nullptr;}
        interpreter->open();

        void* interp_file = malloc(interpreter->size);
        elf64_ehdr* interp_ehdr = (elf64_ehdr*)interp_file;

        if (interpreter->read(0, interpreter->size, interp_file) < 0) {free(exec_file); free(interp_file); return nullptr;}

        if (!verify_header(interp_ehdr)) {free(exec_file); free(interp_file); return nullptr;}

        uint64_t lowest_interp_address = 0;
        task_entry = load_in_memory(task, 0x100000000000, interp_file, &lowest_interp_address);


        task->registers.rsp = task->user_stack_top;
        for (int i = 0; i < 16; i += sizeof(uint32_t)) PUSH_TO_STACK(task, task->registers.rsp, uint32_t, random());
        uint64_t random_bytes = task->registers.rsp;

        char* architecture_string = "x86_64";

        int arch_len = strlen(architecture_string) + 1;
        task->registers.rsp -= arch_len;
        char* arch = (char*)task->registers.rsp;
        task->write_memory(arch, architecture_string, arch_len);
        
        int fp_len = strlen(exec_path) + 1;
        task->registers.rsp -= fp_len;
        char* filepath = (char*)task->registers.rsp;
        task->write_memory(filepath, exec_path, fp_len);

        auxv_t auxv_entries[] = {
            {AT_HWCAP, get_hwcap_x86()},
            {AT_PAGESZ, 0X1000},
            {AT_PHDR, lowest_exec_address + exec_ehdr->e_phoff},
            {AT_PHENT, exec_ehdr->e_phentsize},
            {AT_PHNUM, exec_ehdr->e_phnum},
            {AT_BASE, 0x100000000000 },
            {AT_FLAGS, 0},
            {AT_ENTRY, exec_entry},
            {AT_UID, (uint64_t)task->ruid}, // cast to uint64_t to silence the compiler... The ids are always non negative.
            {AT_EUID, (uint64_t)task->euid},
            {AT_GID, (uint64_t)task->rgid},
            {AT_EGID, (uint64_t)task->egid},
            {AT_SECURE, 0},
            {AT_RANDOM, (uint64_t)random_bytes},
            {AT_EXECFN, (uint64_t)filepath},
            {AT_PLATFORM, (uint64_t)arch},
            {AT_NULL, 0}
        };

        const char* envp[] = { "NAME=dioOS", NULL };  
        setup_stack(task, argc, argv, (char**)envp, auxv_entries);

        interpreter->close();
        free(interp_file);
    }

    vnode_t* tty = vfs::resolve_path("/dev/tty0");
    if (tty){
        task->open_node(tty, 0);
        task->open_node(tty, 1);
        task->open_node(tty, 2);
    }

    task->entry = (function)task_entry;
    node->close();
    free(exec_file);

    vnode_t* cmdline = nullptr;
    task->proc_vfs_dir->find_file("cmdline", &cmdline);

    if (!cmdline){
        task->proc_vfs_dir->creat("cmdline");
        task->proc_vfs_dir->find_file("cmdline", &cmdline);
    }

    if (cmdline){
        cmdline->write(0, strlen(exec_path), exec_path);
        cmdline->close();
    }
    
    return task;
}

int kexecve(const char* pathname, int argc, const char* argv[], int envc, const char* envp[]){
    task_t* self = task_scheduler::get_current_task();

    vnode_t* executable = vfs::resolve_path(pathname);
    if (!executable) return -ENOENT;
    
    // Check if we have permission to execute it
    if (vfs::vfs_check_permission(executable, 1) < 0) {
        executable->close();
        return -EPERM;
    }

    void* executable_buffer = malloc(executable->size);
    if (!executable_buffer) {
        executable->close();
        return -ENOMEM;
    }
    
    int r = executable->read(0, executable->size, executable_buffer);
    if (r != executable->size){
        free(executable_buffer);
        executable->close();
        return -EIO;
    }

    if (!verify_header((elf64_ehdr*)executable_buffer)) {
        free(executable_buffer);
        executable->close();
        return -ENOEXEC;
    }
    

    task_t* task = task_scheduler::clone_for_execve(self);
    strcpy(task->executable_name, executable->name);

    memset(task->name, 0, sizeof(task->name));
    memcpy(task->name, executable->name, min(strlen(executable->name), sizeof(task->name)-1));

    char* interpreter_path = get_interpreter(executable_buffer);

    uint64_t lowest_exec_address = 0;
    uint64_t exec_entry = load_in_memory(task, 0, executable_buffer, &lowest_exec_address);
    uint64_t task_entry = exec_entry;

    if (interpreter_path){
        vnode_t* interpreter = vfs::resolve_path(interpreter_path);

        if (interpreter == nullptr){
            free(executable_buffer);
            executable->close();
            task->exit(true);
            return -ENOENT;
        }

        // Check if we have permission to execute it
        if (vfs::vfs_check_permission(interpreter, 1) < 0) {
            free(executable_buffer);
            executable->close();
            interpreter->close();
            task->exit(true);
            return -EACCES;
        }

        void* interpreter_buffer = malloc(interpreter->size);

        r = interpreter->read(0, interpreter->size, interpreter_buffer);
        if (r != interpreter->size){
            free(interpreter_buffer);
            free(executable_buffer);
            executable->close();
            interpreter->close();
            task->exit(true);
            return -EIO;
        }

        if (!verify_header((elf64_ehdr*)interpreter_buffer)) {
            free(interpreter_buffer);
            free(executable_buffer);
            executable->close();
            interpreter->close();
            task->exit(true);
            return -ENOEXEC;
        }

        uint64_t lowest_interp_address = 0;
        task_entry = load_in_memory(task, 0x100000000000, interpreter_buffer, &lowest_interp_address);

        task->registers.rsp = task->user_stack_top;
        for (int i = 0; i < 16; i += sizeof(uint32_t)) PUSH_TO_STACK(task, task->registers.rsp, uint32_t, random());
        uint64_t random_bytes = task->registers.rsp;

        char* architecture_string = "x86_64";

        int arch_len = strlen(architecture_string) + 1;
        task->registers.rsp -= arch_len;
        char* arch = (char*)task->registers.rsp;
        task->write_memory(arch, architecture_string, arch_len);
        
        int fp_len = strlen(pathname) + 1;
        task->registers.rsp -= fp_len;
        char* filepath = (char*)task->registers.rsp;
        task->write_memory(filepath, pathname, fp_len);

        auxv_t auxv_entries[] = {
            {AT_HWCAP, get_hwcap_x86()},
            {AT_PAGESZ, 0X1000},
            {AT_PHDR, lowest_exec_address + ((elf64_ehdr*)executable_buffer)->e_phoff},
            {AT_PHENT, ((elf64_ehdr*)executable_buffer)->e_phentsize},
            {AT_PHNUM, ((elf64_ehdr*)executable_buffer)->e_phnum},
            {AT_BASE, 0x100000000000 },
            {AT_FLAGS, 0},
            {AT_ENTRY, exec_entry},
            {AT_UID, (uint64_t)task->ruid}, // cast to uint64_t to silence the compiler... The ids are always non negative.
            {AT_EUID, (uint64_t)task->euid},
            {AT_GID, (uint64_t)task->rgid},
            {AT_EGID, (uint64_t)task->egid},
            {AT_SECURE, 0},
            {AT_RANDOM, (uint64_t)random_bytes},
            {AT_EXECFN, (uint64_t)filepath},
            {AT_PLATFORM, (uint64_t)arch},
            {AT_NULL, 0}
        };

        setup_stack(task, argc, (char**)argv, (char**)envp, auxv_entries);
        interpreter->close();
        free(interpreter_buffer);
    }

    task->entry = (function)task_entry;
    executable->close();
    free(executable_buffer);
    
    serialf("%d | execve('%s', {count: %d}, {count: %d})\n", self->pid, pathname, argc, envc);
    

    vnode_t* cmdline = nullptr;
    task->proc_vfs_dir->find_file("cmdline", &cmdline);

    if (!cmdline){
        task->proc_vfs_dir->creat("cmdline");
        task->proc_vfs_dir->find_file("cmdline", &cmdline);
    }

    if (cmdline){
        cmdline->write(0, strlen(pathname), pathname);
        cmdline->close();
    }

    asm ("cli");
    task_scheduler::mark_ready(task);
    self->exit(0, true);
    asm ("sti");

    while(1) asm("hlt");
}