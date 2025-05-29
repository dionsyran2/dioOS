#pragma once
#include <stdint.h>
#include "../BasicRenderer.h"
#include "../cstr.h"
#include "../drivers/storage/ahci/ahci.h"
#include "../paging/PageFrameAllocator.h"
#include "../drivers/serial.h"
#include "../drivers/storage/main/main.h"

namespace AHCI {
    class Port; // Forward declaration
}
class FSInfo{
    public:
    uint32_t signature; //Lead signature (must be 0x41615252 to indicate a valid FSInfo structure) 
    uint8_t resv0[480]; //MUST NEVER BE USED!
    uint32_t signature2; //Another signature (must be 0x61417272)
    uint32_t lastKnownFreeCluster;//Contains the last known free cluster count on the volume. If the value is 0xFFFFFFFF, then the free count is unknown and must be computed. However, this value might be incorrect and should at least be range checked (<= volume cluster count) 
    uint32_t StartLookingCluster; //Indicates the cluster number at which the filesystem driver should start looking for available clusters. If the value is 0xFFFFFFFF, then there is no hint and the driver should start searching at 2. Typically this value is set to the last allocated cluster number. As the previous field, this value should be range checked. 
    uint8_t rsv1[12];
    uint32_t trailSignature; //Trail signature (0xAA550000) 
}__attribute__((aligned(512)));


struct DirectoryEntry {
    char name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creationTimeSec; //Creation time in hundredths of a second, although the official FAT Specification from Microsoft says it is tenths of a second.
    uint16_t creationTime; //The time that the file was created. Multiply Seconds by 2.
    uint16_t creationDate;
    uint16_t LastAccessed;
    uint16_t firstCluster;//16 High bits
    uint16_t lastModificationTime;
    uint16_t lastModificationDate;
    uint16_t firstClusterLow; //16 Low bits
    uint32_t size;
} __attribute__((packed));

struct LNEntry{
    uint8_t order; //The order of this entry in the sequence of long file name entries. This value helps you to know where in the file's name the characters from this entry should be placed. 
    uint8_t first5Char[10];
    uint8_t attributes;
    uint8_t longEntryType; //0 for name entries
    uint8_t checksum;
    uint8_t next6Char[12];
    uint16_t rsv;
    uint8_t last2Char[4];
};

struct DirEntries{
    uint16_t numOfEntries = 0;
    DirectoryEntry* entries[256];
} __attribute__((packed));



class FAT32{
    public:
    void Init(DISK_DRIVE* port_, uint64_t offset);
    uint64_t offset;
    uint8_t* FindFile(const char* filename, uint8_t* dir);
    uint8_t* LoadFile(uint8_t* DirEntry);
    uint32_t calculateSector(uint32_t cluster);
    uint64_t getNextCluster(uint64_t cluster);
    DirEntries* ListDir(uint8_t* dir);
    char* partName();
    bool isBoot = false;
    private:
    DISK_DRIVE* port;
    uint8_t* bpb;
    void* fatBuffer;
    uint32_t rootClusterSector;
    uint8_t* rootDir;
};
extern FAT32* FS;