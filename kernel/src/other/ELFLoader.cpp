#include <other/ELFLoader.h>
#include <scheduling/task/scheduler.h>

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
        size_t offset = hdr->p_vaddr & 0xFFF;
        size_t memsz = hdr->p_memsz + offset;
        size_t page_count = (memsz + 0xFFF) / 0x1000;

        void* m = GlobalAllocator.RequestPages(page_count);
        memset(m, 0, memsz);

        uint64_t virt_base = base + (hdr->p_vaddr & ~0xFFF);
        //kprintf("Loading hdr at %llx, length: %llx\n", hdr->p_align, base + hdr->p_vaddr, hdr->p_memsz);

        for (size_t o = 0; o < page_count; o++) {
            uint64_t v_addr = virt_base + (o * 0x1000);
            uint64_t p_addr = (uint64_t)m + (o * 0x1000);
            ptm->MapMemory((void*)v_addr, (void*)p_addr);
        }

        uint8_t* dest = (uint8_t*)m + offset;
        memcpy(dest, file + hdr->p_offset, hdr->p_filesz);
    }


    uint64_t LoadInMemory(PageTableManager* ptm, uint64_t base, uint8_t* file){
        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        for (int i = 0; i < ehdr->e_phnum; i++){
            ProgramHdr64* phdr = &(((ProgramHdr64*)(file + ehdr->e_phoff))[i]);
            switch (phdr->p_type){
                case PT_LOAD: {
                    LoadHeader(ptm, base, file, phdr);
                    break;
                }
            }
        }

        //kprintf("LOADED\n");
        return base + ehdr->e_entry;
    }

    void tst(){
        serialPrint(COM1, "TEST!!\n\r");
        while(1);
    }

    int Load(vnode_t *node, int priority){
        globalRenderer->Set(true);

        size_t cnt = 0;
        uint8_t* file = (uint8_t*)node->ops.load(&cnt, node);


        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        // Verify the elf
        if (!verify_elf(ehdr)){
            //kprintf("Not a valid ELF64 file\n");
            return 0;
        }

        // Create a new ptm for the process
        PageTableManager* ptm = new PageTableManager((PageTable*)GlobalAllocator.RequestPage());
        memset(ptm->PML4, 0, 0x1000);
        ptm->MapMemory(ptm->PML4, ptm->PML4);
        
        // Create the process
        taskScheduler::task_t* task = taskScheduler::CreateTask(nullptr, 0, true);
        task->ptm = ptm;
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
            uint8_t* interp = (uint8_t*)interp_node->ops.load(&cnt, interp_node);
            Elf64_Ehdr* interp_ehdr = (Elf64_Ehdr*)interp;
            uint64_t interp_entry = LoadInMemory(ptm, 0x100000000000, interp);
            uint64_t main_entry = LoadInMemory(ptm, 0x80000000000, file);
            //kprintf("main: %llx\n", main_entry);
            task->function = (taskScheduler::func_ptr)interp_entry;
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

            size_t lowestThing = 0;
            for (int i = 0; i < ehdr->e_phnum; i++) {
                ProgramHdr64 *elf_phdr = (ProgramHdr64 *)((size_t)file + ehdr->e_phoff + i * ehdr->e_phentsize);
                if (elf_phdr->p_type != PT_LOAD) continue;
                if (!lowestThing || lowestThing > elf_phdr->p_vaddr)
                    lowestThing = elf_phdr->p_vaddr;
            }
            size_t phdrThing = lowestThing;
            phdrThing += 0x80000000000;

            auxv_t auxv_entries[] = {
                {AT_PHDR, 0x80000000000 + ehdr->e_phoff},
                {AT_PLATFORM, (uint64_t)arch},
                {AT_FLAGS, 0},
                {AT_BASE, 0x100000000000},
                {AT_ENTRY, main_entry},
                {AT_PHENT, ehdr->e_phentsize},
                {AT_PHNUM, ehdr->e_phnum},
                {AT_SECURE, 0},
                {AT_PAGESZ, 4096},
                {AT_RANDOM, (uint64_t)ptr},
                {AT_NULL, 0}
            };

            char* argv[] = { interp_path, "/applications/test.elf", NULL };
            char* envp[] = { "PATH=/usr/bin", NULL };
            setup_stack(task, 2, argv, envp, auxv_entries);

            //kprintf("INSPECT: %llx\n", task->rsp);

            task->valid = true;

        }else{
            uint64_t entry = LoadInMemory(ptm, ehdr->e_type == 1 ? 0x80000000 : 0, file);
            task->function = (taskScheduler::func_ptr)entry;
            task->valid = true;
        }

    }
}