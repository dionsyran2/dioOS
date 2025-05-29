#include "main.h"
#include "../volume/volume.h"

size_t numOfDisks = 0;
DISK_DRIVE Disks[25];


DISK_DRIVE* registerDisk(void* drv, DISK_DRIVER type){
    if (numOfDisks >= 24) return nullptr;

    Disks[numOfDisks].drv_type = type;
    Disks[numOfDisks].dsk_drv = drv;
    numOfDisks++;
    return &(Disks[numOfDisks - 1]);
}

DISK_DRIVE* getDisk(uint8_t diskID){
    if (diskID > numOfDisks) return nullptr;
    return &(Disks[diskID]);
}

void InitializeDisk(DISK_DRIVE* dsk){
    //check for partitions/volumes and register them
    if (GPT::isDiskGPT(dsk)){
        GPT::initDisk(dsk);
    }else{
        VolumeSpace::CreateVolume(dsk, 0);
    }
}

bool DISK_DRIVE::Read(uint64_t sector, uint32_t sectorCount, void* buffer){
    switch (drv_type){
        case AHCI_ATA:{
            AHCI::Port* port = (AHCI::Port*)dsk_drv;
            return port->Read(sector, sectorCount, buffer);
        }

        case AHCI_ATAPI:{
            AHCI::Port* port = (AHCI::Port*)dsk_drv;
            return port->ReadPI(sector, sectorCount, buffer);
        }
        default:
            return false;
    }
}