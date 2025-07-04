#include <drivers/storage/gpt/gpt.h>
#include <filesystem/FAT32/fat32.h>


namespace GPT{
    //DEFINITIONS
    uint8_t BasicDataPartitionTypeGUID[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
    

    //FUNCTIONS

    void _find_and_init_fs(vblk_t* dsk, uint64_t start){
        if (filesystem::is_fat32(dsk, start)){
            new filesystem::fat32(dsk, start);
        }
    }

    bool isDiskGPT(vblk_t* drv){
        //Perform a test read at the second sector of the disk (sector 1) and check the first 8 bytes (Signature)
        char signature[9] = "EFI PART";
        char* buffer = (char*)GlobalAllocator.RequestPage();
        memset(buffer, 0, 0x1000);
        drv->blk_ops.read(1, 1, (void*)buffer, drv);

        bool isGPT = true;
        for (int i = 0; i < 8; i++){
            if (signature[i] != *(buffer + i)){
                isGPT = false;
                break;
            }
        }

        GlobalAllocator.FreePage(buffer);
        return isGPT;
    }

    bool initDisk(vblk_t* dsk){

        if (!isDiskGPT(dsk)){
            // Not gpt
            _find_and_init_fs(dsk, 0);
            return true;
        }

        PTHdr* hdr = (PTHdr*)GlobalAllocator.RequestPage();

        dsk->blk_ops.read(1, 1, hdr, dsk);
        kprintf(0xFF0000, "SIGNATURE: %s, LBA_CONTAINING_THIS_HDR: %d, STARTING_LBA_OF_PEA: %d, NUMBER_OF_PARTITION_ENTRIES: %d\n", hdr->sig, hdr->LBA_CONTAINING_THIS_HDR, hdr->STARTING_LBA_OF_PEA, hdr->NUMBER_OF_PARTITION_ENTRIES);


        void* buffer = GlobalAllocator.RequestPages(((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
        memset(buffer, 0, (hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) + 0x1000);
        dsk->blk_ops.read(hdr->STARTING_LBA_OF_PEA, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 512) + 1, buffer, dsk);

        size_t numOfPartitions = 0;
        for (int i = 0; i < hdr->NUMBER_OF_PARTITION_ENTRIES; i++){
            PartEntry* entry = (PartEntry*)((uint64_t)buffer + (i * hdr->SIZE_OF_PE));
            if (entry->PARTITION_TYPE_GUID[0] || entry->PARTITION_TYPE_GUID[1] || entry->PARTITION_TYPE_GUID[2]){
                int x = 0;
                while (x < 72){
                    //kprintf("%s", charToConstCharPtr((uint8_t)entry->PARTITION_NAME[x]));
                    x += 2;
                }
                //kprintf("\n");
            }
            if (memcmp(entry->PARTITION_TYPE_GUID, BasicDataPartitionTypeGUID, 16) == 0){
                //kprintf(0xFFFF00, "PARTITION[%d] IS A BASIC DATA PARTITION! (STARTING_LBA: %d, ENDING_LBA: %d, HEADER SIZE: %d)\n", i, entry->STARTING_LBA, entry->ENDING_LBA);
                _find_and_init_fs(dsk, entry->STARTING_LBA);
            }
        }

        GlobalAllocator.FreePage(hdr);
        GlobalAllocator.FreePages(buffer, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
    }

}
