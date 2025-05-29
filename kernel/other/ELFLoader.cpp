#include "ELFLoader.h"

namespace ELFLoader{
    int Load(DirEntry* fileEntry, int priority){
        if (fileEntry == nullptr) return 1; // Invalid directory entry
        
        void* file = fileEntry->volume->LoadFile(fileEntry);
        if (file == nullptr) return 2; // Invalid file data

        Elf64_Ehdr* ehdr = (Elf64_Ehdr*)file;

        /*  The ELF e_ident (first 4 bytes of the magic / header) */
        /*  must be 0x7F 0x45 0x4C 0x46. Bytes 1-3 spell "ELF"    */

        if (strncmp((char*)&ehdr->e_ident, "\x7F""ELF", 3)) return 2; // Invalid file data

        uint64_t Address = 0xFFFFFFFFFFFFFFFF, Size;

        taskScheduler::disableSwitch = true;

        taskScheduler::task_t* task = taskScheduler::CreateTask(nullptr, priority, true);
        task->setName(fileEntry->name);
        PageTableManager* mgr = task->CreatePageTableMgr();

        for (uint64_t i = ehdr->e_phoff; i < ehdr->e_phnum * ehdr->e_phentsize; i += ehdr->e_phentsize){ // Loop through the entries (headers)
            ProgramHdr64* pHdr = (ProgramHdr64*)(file + i); // calculate the entry address
            switch (pHdr->p_type){
                case 1:{ //Load
                    void* addr = GlobalAllocator.RequestPages((pHdr->p_memsz / 0x1000) + 1);
                    memset(addr, (uint8_t)0, (pHdr->p_memsz / 0x1000) + 1);
                    memcpy(addr, file + pHdr->p_offset, pHdr->p_filesz);
                    Address = pHdr->p_vaddr;
                    Size = (pHdr->p_memsz / 0x1000) + 1;
                    for (int x = 0; x < Size; x++){
                        mgr->MapMemory((void*)(pHdr->p_vaddr + (x * 0x1000)), (void*)((uint64_t)addr + (x * 0x1000)));
                    }
                    //kprintf("Loading Header %d at %llx:%llx\n", i / ehdr->e_phentsize, addr, pHdr->p_vaddr);
                }
                
                default:
                    continue;
            }
        }

        task->function = (taskScheduler::func_ptr)ehdr->e_entry;

        if (strncmp(fileEntry->name, "WM      ELF", 12) == 0){
            taskScheduler::WM_TASK = task;
        }

        taskScheduler::disableSwitch = false;
        return 0;
    }
}
