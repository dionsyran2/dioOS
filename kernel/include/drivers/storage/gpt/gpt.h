#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../../../memory.h"
#include "../../../paging/PageFrameAllocator.h"
#include "../ahci/ahci.h"
#include <VFS/vfs.h>

class DISK_DRIVE;

struct PTHdr{ //Partition Table Header
    char sig[8];
    uint32_t revision;
    uint32_t HDR_SZ;
    uint32_t CRC32_Checksum;
    uint32_t rsv;
    uint64_t LBA_CONTAINING_THIS_HDR;
    uint64_t LBA_OF_ALT_GPT_HDR;
    uint64_t FIRST_USABLE_BLOCK; //The first usable block that can be contained in a GPT entry 
    uint64_t LAST_USABLE_BLOCK; //The last usable block that can be contained in a GPT entry
    uint8_t GUID[16];
    uint64_t STARTING_LBA_OF_PEA; //Starting LBA of the GUID Partition Entry array
    uint32_t NUMBER_OF_PARTITION_ENTRIES;
    uint32_t SIZE_OF_PE; //Size (in bytes) of each entry in the Partition Entry array - must be a value of 128×2ⁿ where n ≥ 0
    uint32_t CRC32_OF_PEA;
    //rest rsv
};

struct PartEntry{ //Partition Entry
    uint8_t PARTITION_TYPE_GUID[16]; //zero means unused
    uint8_t UNIQUE_PARTITION_GUID[16];
    uint64_t STARTING_LBA;
    uint64_t ENDING_LBA;
    uint64_t ATTRIBUTES;
    uint8_t PARTITION_NAME[72];
};


namespace GPT{
    bool isDiskGPT(vblk_t* dsk);
    bool initDisk(vblk_t* dsk);
}