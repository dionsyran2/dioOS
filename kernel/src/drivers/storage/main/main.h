#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../../../memory.h"
#include "../../../paging/PageFrameAllocator.h"
#include "../ahci/ahci.h"
#include "../../../BasicRenderer.h"
#include "../gpt/gpt.h"

enum DISK_DRIVER{
    AHCI_ATA = 1,
    AHCI_ATAPI = 2,
    UNKNOWN = 0
};

struct DISK_DRIVE{
    DISK_DRIVER drv_type = UNKNOWN;
    void* dsk_drv;
    bool Read(uint64_t sector, uint32_t sectorCount, void* buffer);
};

DISK_DRIVE* registerDisk(void* drv, DISK_DRIVER type);
DISK_DRIVE* getDisk(uint8_t diskID);
void InitializeDisk(DISK_DRIVE* dsk);


extern size_t numOfDisks;
extern DISK_DRIVE Disks[25];