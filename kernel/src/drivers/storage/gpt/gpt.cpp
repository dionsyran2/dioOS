#include "gpt.h"
#include "../volume/volume.h"

namespace GPT{
    //DEFINITIONS
    uint8_t BasicDataPartitionTypeGUID[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};


    //FUNCTIONS
    bool isDiskGPT(DISK_DRIVE* drv){
        //Perform a test read at the second sector of the disk (sector 1) and check the first 8 bytes (Signature)
        char signature[9] = "EFI PART";
        char* buffer = (char*)GlobalAllocator.RequestPage();
        memset(buffer, 0, 0x1000);
        drv->Read(1, 1, (void*)buffer);

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

    bool initDisk(DISK_DRIVE* dsk){
        PTHdr* hdr = (PTHdr*)GlobalAllocator.RequestPage();

        dsk->Read(1, 1, hdr);
        globalRenderer->printf(0xFF0000, "SIGNATURE: %s, LBA_CONTAINING_THIS_HDR: %d, STARTING_LBA_OF_PEA: %d, NUMBER_OF_PARTITION_ENTRIES: %d\n", hdr->sig, hdr->LBA_CONTAINING_THIS_HDR, hdr->STARTING_LBA_OF_PEA, hdr->NUMBER_OF_PARTITION_ENTRIES);


        void* buffer = GlobalAllocator.RequestPages(((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
        memset(buffer, 0, (hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) + 0x1000);
        dsk->Read(hdr->STARTING_LBA_OF_PEA, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 512) + 1, buffer);

        size_t numOfPartitions = 0;
        for (int i = 0; i < hdr->NUMBER_OF_PARTITION_ENTRIES; i++){
            PartEntry* entry = (PartEntry*)((uint64_t)buffer + (i * hdr->SIZE_OF_PE));
            if (entry->PARTITION_TYPE_GUID[0] || entry->PARTITION_TYPE_GUID[1] || entry->PARTITION_TYPE_GUID[2]){
                int x = 0;
                while (x < 72){
                    //globalRenderer->printf("%s", charToConstCharPtr((uint8_t)entry->PARTITION_NAME[x]));
                    x += 2;
                }
                //globalRenderer->printf("\n");
            }
            if (memcmp(entry->PARTITION_TYPE_GUID, BasicDataPartitionTypeGUID, 16) == 0){
                globalRenderer->printf(0xFFFF00, "PARTITION[%d] IS A BASIC DATA PARTITION! (STARTING_LBA: %d, ENDING_LBA: %d, HEADER SIZE: %d)\n", i, entry->STARTING_LBA, entry->ENDING_LBA);
                VolumeSpace::CreateVolume(dsk, entry->STARTING_LBA);
            }
        }

        GlobalAllocator.FreePage(hdr);
        GlobalAllocator.FreePages(buffer, ((hdr->SIZE_OF_PE * hdr->NUMBER_OF_PARTITION_ENTRIES) / 0x1000) + 1);
    }
}
