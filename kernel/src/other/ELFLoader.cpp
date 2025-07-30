#include <other/ELFLoader.h>
#include <kerrno.h>
#include <userspace/userspace.h>

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

namespace ELFLoader
{

    uint8_t ELF_MAGIC[] = {0x7f, 'E', 'L', 'F'};
    bool verify_elf(Elf64_Ehdr* hdr){
        if (hdr == nullptr || memcmp(hdr->e_ident, ELF_MAGIC, EI_NIDENT)) return false;

        return true;
    }

    char* GetInterpreter(uint8_t* file){
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        for (int i = 0; i < ehdr->e_phnum; i++){
            ProgramHdr64* phdr = &(((ProgramHdr64*)(file + ehdr->e_phoff))[i]);
            if (phdr->p_type == PT_INTERP){
                return (char*)(file + phdr->p_offset);
            }
        }
        return nullptr;
    }

    void LoadHeader(PageTableManager* ptm, uint64_t base, uint8_t* file, ProgramHdr64* hdr) {
        size_t align = hdr->p_align >= 0x1000 ? hdr->p_align : 0x1000;
        uint64_t seg_start = hdr->p_vaddr & ~(align - 1);
        size_t offset = hdr->p_vaddr - seg_start;
        size_t memsz = hdr->p_memsz + offset;
        size_t page_count = (memsz + 0xFFF) / 0x1000;

        void* m = GlobalAllocator.RequestPages(page_count);
        memset(m, 0, memsz);

        uint64_t virt_base = base + seg_start;

        //kprintf("p_align: %llx, v_addr: %llx, p_memsz: %d bytes\n", hdr->p_align, base + hdr->p_vaddr, hdr->p_memsz);
        serialf("\e[0;35mLoading header at v_addr: %llx, %d bytes\e[0m\n", hdr->p_vaddr + base, hdr->p_memsz);
        for (size_t o = 0; o < page_count; o++) {
            uint64_t v_addr = virt_base + (o * 0x1000);
            if (ptm->isMapped((void*)v_addr)) continue;
            uint64_t p_addr = (uint64_t)m + (o * 0x1000);
            ptm->MapMemory((void*)v_addr, (void*)p_addr);
            if ((hdr->p_flags & PF_R) && (hdr->p_flags & PF_W) == 0) {
                ptm->SetPageFlag((void*)v_addr, PT_Flag::ReadWrite, false);
            }else{
                ptm->SetPageFlag((void*)v_addr, PT_Flag::ReadWrite, true);
            }
            if ((hdr->p_flags & PF_X) == 0) ptm->SetPageFlag((void*)v_addr, PT_Flag::NX, true);
            //kprintf(0xFF0000, "%llx | %llx\n", v_addr, p_addr);
        }

        uint8_t* dest = (uint8_t*)m + offset;
        memcpy_simd(dest, file + hdr->p_offset, hdr->p_filesz);
        //kprintf("-------------------------------------------------------------\n");
    }


    uint64_t LoadInMemory(PageTableManager* ptm, uint64_t base, uint8_t* file, uint64_t* lowest_address){
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;
        uint64_t lowest = UINT64_MAX;
        for (int i = 0; i < ehdr->e_phnum; i++){
            ProgramHdr64* phdr = &(((ProgramHdr64*)(file + ehdr->e_phoff))[i]);
            switch (phdr->p_type){
                case PT_LOAD: {
                    if ((phdr->p_vaddr + base) < lowest) lowest = phdr->p_vaddr + base;
                    LoadHeader(ptm, base, file, phdr);
                    break;
                }
            }
        }
        if (lowest_address != nullptr) *lowest_address = lowest;
        //kprintf("LOADED\n");
        return base + ehdr->e_entry;
    }

    int Load(vnode_t *node, int priority, user_t* user, vnode_t* tty, session::session_t* session, task_t* parent){
        globalRenderer->Set(true);

        size_t cnt = 0;
        uint8_t* file = nullptr;
        int res = node->read(&cnt, (void**)&file);
        serialf("RES: %d %llx %d\n", res, file, cnt);
        bool b = false;
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        // Verify the elf
        if (!verify_elf(ehdr)){
            kprintf("Not a valid ELF64 file\n");
            return 0;
        }

        // Create a new ptm for the process
        PageTableManager* ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
        memset(ptm->PML4, 0, 0x1000);
        ptm->MapMemory(ptm->PML4, ptm->PML4);
        
        // Create the process
        int uid = 0;
        int gid = 0;
        if (user){
            uid = user->UID;
            gid = user->GID;
        }


        task_t* task = task_scheduler::create_process(
            node->name, nullptr, parent, false,
            tty, nullptr, uid, gid, uid, gid,
            0, 0, 0, TASK_CPL3,
            vfs::resolve_path(user->home),
            vfs::get_root_node(), session
        );
        
        task->ptm = ptm;
        if (tty){
            int* amount = task->open_file_descriptors;
            task->open_node(tty, 0);
            task->open_node(tty, 1);
            task->open_node(tty, 2);
        }

        globalPTM.ClonePTM(ptm);

        char* interp_path = GetInterpreter(file);
        

        if (interp_path != nullptr){
            //kprintf(0x00F0F0, "INTERP: %s\n", interp_path);

            vnode_t* interp_node = vfs::resolve_path(interp_path);
            
            if (interp_node == nullptr){
                //kprintf(0xFF0000, "Could not locate the interpreter: %s\n", interp_path);
                return -1;
            }

            size_t cnt = 0;
            uint8_t* interp;
            interp_node->read(&cnt, (void**)&interp);
        
            Elf64_Ehdr* interp_ehdr = (Elf64_Ehdr*)interp;
            if (!verify_elf(interp_ehdr)){
                kprintf("The interpreter is not a valid ELF64 file\n");
                return 0;
            }
            uint64_t interp_entry = LoadInMemory(ptm, 0x100000000000 , interp, nullptr);
            uint64_t lowest_header = 0x80000000000;
            uint64_t main_entry = LoadInMemory(ptm, 0x80000000000, file, &lowest_header);
            //kprintf("main: %llx\n", main_entry);
            task->entry = (function)interp_entry;
            //kprintf("entry: %llx\n", interp_entry);

            uint8_t* ptr = new uint8_t[16];
            ptr[0] = 0x10;
            ptr[1] = 0x10;
            ptr[2] = 0x10;
            ptr[3] = 0x10;
            ptr[4] = 0x10;
            ptr[5] = 0x10;
            ptr[6] = 0x10;
            ptr[7] = 0x10;
            ptr[8] = 0x10;
            ptr[9] = 0x10;
            ptr[10] = 0x10;
            ptr[11] = 0x10;
            ptr[12] = 0x10;
            ptr[13] = 0x10;
            ptr[14] = 0x10;
            ptr[15] = 0x10;

            char* arch = new char[strlen("x86_64") + 1];
            strcpy(arch, "x86_64");

            char* fp = vfs::get_full_path_name(node);
            char* execfn = new char[strlen(fp) + 1];
            strcpy(execfn, fp);

            auxv_t auxv_entries[] = {
                {AT_HWCAP, get_hwcap_x86()},
                {AT_PAGESZ, 0X1000},
                {AT_PHDR, 0x80000000000 + ehdr->e_phoff},
                {AT_PHENT, ehdr->e_phentsize},
                {AT_PHNUM, ehdr->e_phnum},
                {AT_BASE, 0x100000000000 },
                {AT_FLAGS, 0},
                {AT_ENTRY, main_entry},
                {AT_UID, uid},
                {AT_EUID, uid},
                {AT_GID, gid},
                {AT_EGID, gid},
                {AT_SECURE, 0},
                {AT_RANDOM, (uint64_t)ptr},
                {AT_EXECFN, (uint64_t)execfn},
                {AT_PLATFORM, (uint64_t)arch},
                {AT_NULL, 0}
            };

            char* argv[] = { vfs::get_full_path_name(node), NULL };
            const char* envp[] = { "PATH=/bin", "SHELL=/bin/bash", "NAME=dioOS", "TERM=ansi", "DISPLAY=:0", "PS1=\\[\\033[01;32m\\]\\u@\\h\\[\\033[00m\\]:\\[\\033[01;34m\\]\\w\\[\\033[00m\\]\\$ ", NULL };
            setup_stack(task, 1, argv, (char**)envp, auxv_entries);

            //kprintf("INSPECT: %llx\n", task->rsp);
            task_scheduler::mark_ready(task);

        }else{
            uint64_t entry = LoadInMemory(ptm, ehdr->e_type == 1 ? 0x80000000 : 0, file, nullptr);
            task->entry = (function)entry;
            task_scheduler::mark_ready(task);
        }
        return 0;
    }

    
    int kexecve(vnode_t* node, int argc, char** argv, char** envp){
        if (node == nullptr) return -1;
        
        task_t* ctask = task_scheduler::get_current_task();

        size_t cnt = 0;
        uint8_t* file;
        node->read(&cnt, (void**)&file);
        if (file == nullptr) return -ENOENT;
        if (cnt < sizeof(Elf64_Ehdr)) return -ENOENT;

        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        // Verify the elf
        if (!verify_elf(ehdr)){
            //kprintf("Not a valid ELF64 file\n");
            ctask->tty->write("Not a valid ELF64 file!\n\r", 25);
            return -EINVAL;
        }

        // Create a new ptm for the process
        PageTableManager* ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
        memset(ptm->PML4, 0, 0x1000);
        ptm->MapMemory(ptm->PML4, ptm->PML4);
        
        // Create the process


        task_t* task = task_scheduler::create_process(
            node->name, nullptr, nullptr, false,
            ctask->tty, nullptr, ctask->uid, ctask->gid, ctask->euid, ctask->egid,
            0, 0, 0, TASK_CPL3,
            ctask->nd_cwd,
            ctask->nd_root,
            ctask->session
        );

        task->ppid = ctask->ppid;
        task->pid = ctask->pid;
        task->tid = ctask->tid;
        task->pgid = ctask->pgid;

        task->ptm = ptm;
        if (task->tty){
            task->open_node(task->tty, 0);
            task->open_node(task->tty, 1);
            task->open_node(task->tty, 2);
        }

        globalPTM.ClonePTM(ptm);

        char* interp_path = GetInterpreter(file);

        if (interp_path != nullptr){
            serialf("interpreter: %s\n", interp_path);

            vnode_t* interp_node = vfs::resolve_path(interp_path);

            if (interp_node == nullptr){
                kprintf(0xFF0000, "Could not locate the interpreter: %s\n", interp_path);
                return -1;
            }

            size_t cnt = 0;
            uint8_t* interp;
            interp_node->read(&cnt, (void**)&interp);
            if (interp_node == nullptr) return -ENOENT;
            if (cnt < sizeof(Elf64_Ehdr)) return -ENOENT;
           
            Elf64_Ehdr* interp_ehdr = (Elf64_Ehdr*)interp;
            uint64_t interp_entry = LoadInMemory(ptm, 0x100000000000 , interp, nullptr);
            uint64_t lowest_header = 0x80000000000;
            uint64_t main_entry = LoadInMemory(ptm, (ehdr->e_type == 1 || ehdr->e_type == 3) ? 0x80000000000 : 0, file, &lowest_header);
            //kprintf("main: %llx\n", main_entry);
            task->entry = (function)interp_entry;
            //kprintf("entry: %llx\n", interp_entry);

            uint8_t* ptr = new uint8_t[16];
            ptr[0] = 0x10;
            ptr[1] = 0x10;
            ptr[2] = 0x10;
            ptr[3] = 0x10;
            ptr[4] = 0x10;
            ptr[5] = 0x10;
            ptr[6] = 0x10;
            ptr[7] = 0x10;
            ptr[8] = 0x10;
            ptr[9] = 0x10;
            ptr[10] = 0x10;
            ptr[11] = 0x10;
            ptr[12] = 0x10;
            ptr[13] = 0x10;
            ptr[14] = 0x10;
            ptr[15] = 0x10;

            char* arch = new char[strlen("x86_64") + 1];
            strcpy(arch, "x86_64");
            char* fp = vfs::get_full_path_name(node);
            char* execfn = new char[strlen(fp) + 1];
            strcpy(execfn, fp);

            serialf("LOWEST: %llx\n", lowest_header);

            auxv_t auxv_entries[] = {
                {AT_HWCAP, get_hwcap_x86()},
                {AT_PAGESZ, 0X1000},
                {AT_PHDR, lowest_header + ehdr->e_phoff},
                {AT_PHENT, ehdr->e_phentsize},
                {AT_PHNUM, ehdr->e_phnum},
                {AT_BASE, 0x100000000000 },
                {AT_FLAGS, 0},
                {AT_ENTRY, main_entry},
                {AT_UID, ctask->uid},
                {AT_EUID, ctask->euid},
                {AT_GID, ctask->gid},
                {AT_EGID, ctask->egid},
                {AT_SECURE, 0},
                {AT_RANDOM, (uint64_t)ptr},
                {AT_EXECFN, (uint64_t)execfn},
                {AT_PLATFORM, (uint64_t)arch},
                {AT_NULL, 0}
            };

            
            setup_stack(task, argc, argv, (char**)envp, auxv_entries);

            //kprintf("INSPECT: %llx\n", task->rsp);
            ctask->state = task_state_t::DISABLED;
            task_scheduler::remove_task(ctask);
            task_scheduler::mark_ready(task);
            asm ("sti");
            while(1);

        }else{
            uint64_t entry = LoadInMemory(ptm, ehdr->e_type == 1 ? 0x80000000 : 0, file, nullptr);
            task->entry = (function)entry;
            ctask->state = task_state_t::DISABLED;
            task_scheduler::remove_task(ctask);
            task_scheduler::mark_ready(task);
            asm ("sti");
            while(1);
        }
        return 0;
    }
}
