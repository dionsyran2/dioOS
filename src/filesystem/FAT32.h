#pragma once
#include "../ahci/ahci.h"
#define MAX_PARTITIONS 4

struct FAT32_BPB {
    uint8_t  jumpBoot[3];            // Jump instruction
    char     OEMName[8];             // OEM name
    uint16_t bytesPerSector;         // Bytes per sector
    uint8_t  sectorsPerCluster;      // Sectors per cluster
    uint16_t reservedSectors;        // Reserved sectors count
    uint8_t  numberOfFATs;           // Number of FAT copies
    uint16_t rootEntryCount;         // FAT12/16 specific
    uint16_t totalSectors16;         // FAT12/16 specific
    uint8_t  mediaType;              // Media descriptor
    uint16_t sectorsPerFAT16;        // FAT12/16 specific        IF 0 >-------|          
    uint16_t sectorsPerTrack;        // Sectors per track (BIOS specific)     |
    uint16_t numberOfHeads;          // Number of heads (BIOS specific)       |
    uint32_t hiddenSectors;          // Hidden sectors count                  |
    uint32_t totalSectors32;         // Total sectors (for FAT32) <-----------|
    
    // FAT32-specific fields
    uint32_t sectorsPerFAT32;        // Sectors per FAT (FAT32)
    uint16_t extFlags;               // Extended flags
    uint16_t fatVersion;              // Filesystem version
    uint32_t rootCluster;            // First cluster of root directory
    uint16_t fsInfo;                 // FSInfo sector number
    uint16_t backupBootSector;       // Backup boot sector
    uint8_t  reserved[12];           // Reserved
    uint8_t  driveNumber;            // Drive number
    uint8_t  WinNTFlags;              // NT4 and up i suppose?
    uint8_t  signature;          // Boot signature (0x29 if next three are valid)
    uint32_t volumeID;               // Volume ID
    char     volumeLabel[11];        // Volume label
    char     fileSystemType[8];      // File system type (FAT32)
    uint8_t bootCode[420];
    uint16_t bootSignature;
} __attribute__((packed));

struct FAT32_DirectoryEntry {
    char name[11];           // File name (8.3 format)
    uint8_t attr;           // Attribute byte
    uint8_t reserved[10];    // Reserved
    uint16_t createTime;    // Create time
    uint16_t createDate;    // Create date
    uint16_t lastAccessDate; // Last access date
    uint16_t firstClusterHigh; // High 16 bits of the first cluster number
    uint16_t writeTime;     // Last write time
    uint16_t writeDate;     // Last write date
    uint16_t firstClusterLow; // Low 16 bits of the first cluster number
    uint32_t fileSize;      // Size of the file in bytes
};

struct MBR {
    uint8_t bootCode[446];        // Boot code area
    struct PartitionEntry{
        uint8_t status;            // Bootable flag (0x80 for bootable)
        uint8_t chsFirst[3];       // CHS address of the first sector
        uint8_t type;              // Partition type
        uint8_t chsLast[3];        // CHS address of the last sector
        uint32_t lbaFirst;         // LBA of the first sector
        uint32_t totalSectors;     // Number of sectors in this partition
    } partitions[MAX_PARTITIONS];
    uint16_t signature;            // MBR signature (0x55AA)
}__attribute__((packed));

struct FAT32{
    void InitDisk(AHCI::Port* port);
    FAT32_BPB* bpb;
    bool fat32;
    void LoadPartition(MBR::PartitionEntry partition);
    bool LoadDirectoryEntries(uint32_t cluster, FAT32_DirectoryEntry* entries, uint32_t maxEntries);
    FAT32_DirectoryEntry* LoadDirectoryEntries(const char* filename);
    uint32_t GetNextCluster(uint32_t cluster);
    uint32_t dataStartSector;
    AHCI::Port* port;
    FAT32_DirectoryEntry* FindFile(const char* filename, FAT32_DirectoryEntry* directory);
    void LoadFileData(FAT32_DirectoryEntry* file, void* buffer);
    void LoadFile(const char* filename, FAT32_DirectoryEntry* directory, uint8_t* fileBuffer);
    uint32_t GetSectorForCluster(uint32_t cluster);
    uint32_t ReadFATEntry(uint32_t cluster);
};
