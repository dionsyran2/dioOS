#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../../../memory.h"
#include "../../../paging/PageFrameAllocator.h"
#include "../ahci/ahci.h"
#include "../../../BasicRenderer.h"
#include "../main/main.h"
#include "../../../filesystem/FAT32.h"
class Volume;


struct DirEntry {
    char name[255];
    uint8_t attributes;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t LastAccessed;
    uint16_t lastModificationTime;
    uint16_t lastModificationDate;
    uint32_t size;
    Volume* volume;
    void* fs_entry;
} __attribute__((packed));

struct DirectoryList{
    size_t numOfEntries = 0;
    DirEntry entries[256];
};

namespace VolumeSpace{
    enum Filesystem{
        FS_FAT32 = 1,
        UNKNOWN = 0
    };

    Filesystem FindFS(DISK_DRIVE* drv, uint64_t lbaOffset);
    bool CreateVolume(DISK_DRIVE* drv, uint64_t lbaOffset);
    extern Volume volumes[255];
    extern size_t numOfVolumes;
}


struct Volume{
    uint8_t volumeID;
    char volumeName[12];
    char assignedLetter;
    DISK_DRIVE* drive;
    uint64_t lbaStart;
    void* fs;
    VolumeSpace::Filesystem FSType;
    DirectoryList* listDir(DirEntry* dir);
    DirEntry *OpenFile(char* filename, DirEntry* dir);
    void *LoadFile(DirEntry* file);
    void CloseFile(DirEntry* file);
};
