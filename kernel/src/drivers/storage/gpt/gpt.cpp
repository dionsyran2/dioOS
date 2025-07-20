#include <drivers/storage/gpt/gpt.h>
#include <filesystem/FAT32/fat32.h>
#include <filesystem/ext2/ext2.h>

namespace GPT{
    //DEFINITIONS
    uint8_t BasicDataPartitionTypeGUID[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};
    uint8_t LinuxFilesystemData[16] = {0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4};

    //FUNCTIONS

    void _find_and_init_fs(vblk_t* dsk, uint64_t start, uint64_t end, PartEntry* entry){
        if (filesystem::is_fat32(dsk, start)){
            new filesystem::fat32(dsk, start);
        }else if (filesystem::is_ext2(dsk, start)){
            new filesystem::ext2(dsk, start, end, entry);
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

    void print_guid(uint8_t *guid) {
        uint32_t data1 = (guid[3] << 24) | (guid[2] << 16) | (guid[1] << 8) | guid[0];
        uint16_t data2 = (guid[5] << 8) | guid[4];
        uint16_t data3 = (guid[7] << 8) | guid[6];

        kprintf("%x-%h-%h-", data1, data2, data3);

        for (int i = 8; i < 10; i++)
            kprintf("%hh", guid[i]);
        kprintf("-");

        for (int i = 10; i < 16; i++)
            kprintf("%hh", guid[i]);

        kprintf("\n");
    }

    bool initDisk(vblk_t* dsk){

        if (!isDiskGPT(dsk)){
            // Not gpt
            _find_and_init_fs(dsk, 0, dsk->block_count, nullptr);
            return true;
        }

        PTHdr* hdr = (PTHdr*)GlobalAllocator.RequestPage();

        dsk->blk_ops.read(1, 1, hdr, dsk);
        //kprintf(0xFF0000, "SIGNATURE: %s, LBA_CONTAINING_THIS_HDR: %d, STARTING_LBA_OF_PEA: %d, NUMBER_OF_PARTITION_ENTRIES: %d\n", hdr->sig, hdr->LBA_CONTAINING_THIS_HDR, hdr->STARTING_LBA_OF_PEA, hdr->NUMBER_OF_PARTITION_ENTRIES);


        void* buffer = GlobalAllocator.RequestPages(((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
        memset(buffer, 0, (hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) + 0x1000);
        dsk->blk_ops.read(hdr->STARTING_LBA_OF_PEA, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 512) + 1, buffer, dsk);

        uint8_t empty_buffer[16] = { 0 };


        size_t numOfPartitions = 0;
        for (int i = 0; i < hdr->NUMBER_OF_PARTITION_ENTRIES; i++){
            PartEntry* entry = (PartEntry*)((uint64_t)buffer + (i * hdr->SIZE_OF_PE));
            if (memcmp(entry->PARTITION_TYPE_GUID, empty_buffer, 16)){
                int x = 0;
                //while (x < 72){
                //    kprintf("%c", entry->PARTITION_NAME[x]);
                //    x += 2;
                //}
                //kprintf("\n");
                
            }
            if (memcmp(entry->PARTITION_TYPE_GUID, BasicDataPartitionTypeGUID, 16) == 0 ||
                memcmp(entry->PARTITION_TYPE_GUID, LinuxFilesystemData, 16) == 0){
                _find_and_init_fs(dsk, entry->STARTING_LBA, entry->ENDING_LBA, entry);
            }
        }

        GlobalAllocator.FreePage(hdr);
        GlobalAllocator.FreePages(buffer, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
    }

}
