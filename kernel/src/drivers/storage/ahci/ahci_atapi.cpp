#include "../../../memory.h"
#include "ahci.h"
#include "../../../BasicRenderer.h"
#include "../../../paging/PageTableManager.h"
#include "../../../memory/heap.h"
#include "../../../paging/PageFrameAllocator.h"
#include "../../../cstr.h"
#include "../../../filesystem/FAT32.h"

namespace AHCI{

    bool Port::ATAPICMD(uint8_t atapiCmd[12], void* DBA, size_t bytecnt) {
        uint16_t spin = 0;
    
        // Clear interrupt status
        hbaPort->interruptStatus = (uint32_t)0;
    
        // Get Command Header
        HBACommandHeader* cmdHeader = (HBACommandHeader*)hbaPort->commandListBase;
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
        cmdHeader->write = 0;  // Read operation (not actually writing)
        cmdHeader->prdtLength = 1;  // No data transfer needed for eject
        cmdHeader->atapi = 1;  // ATAPI command
    
        // Get Command Table
        HBACommandTable* commandTable = (HBACommandTable*)(cmdHeader->commandTableBaseAddress);
        memset(commandTable, 0, sizeof(HBACommandTable));
    
        memcpy(commandTable->atapiCommand, atapiCmd, 12);

        // Set up the PRDT entry (Physical Region Descriptor Table)
        commandTable->prdtEntry[0].dataBaseAddress = (uint32_t)(uint64_t)DBA;
        commandTable->prdtEntry[0].dataBaseAddressUpper = (uint32_t)((uint64_t)DBA >> 32);
        commandTable->prdtEntry[0].byteCount = bytecnt;  // Inquiry response buffer size (36 bytes)
        commandTable->prdtEntry[0].interruptOnCompletion = 1;  // Interrupt on completion
    
        // Fill Command FIS
        FIS_REG_H2D* cmdFIS = (FIS_REG_H2D*)(&commandTable->commandFIS);
        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1; // Is Command
        cmdFIS->command = ATA_CMD_PACKET; // ATAPI Packet command (0xA0)
        cmdFIS->deviceRegister = 1 << 6; // LBA Mode
        cmdFIS->countLow = 0x1; // Always 1 for ATAPI

        uint32_t tfd = hbaPort->taskFileData;
        if (hbaPort->sataError){
            hbaPort->sataError |= 1 << 16;
        }
        hbaPort->taskFileData = 0;

        while(tfd & (1 << 7)){
            tfd = hbaPort->taskFileData;
        }
        while(tfd & (1 << 3)){
            tfd = hbaPort->taskFileData;
        }

        // Issue command
        hbaPort->commandIssue = 0;
        while(hbaPort->commandIssue);
        hbaPort->commandIssue = 1;
    
        // Wait for completion
        while (true) {
            if (hbaPort->commandIssue == 0) break; // Command completed
    
            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                return false; // Command failed
            }
        }
    
        return true;
    }

    bool Port::EjectDisk(){
        // ATAPI Command (Eject / Load CD)
        uint8_t atapiCmd[12] = {0};
        atapiCmd[0] = 0x1B;  // START STOP UNIT command
        atapiCmd[1] = 0;      // Reserved
        atapiCmd[2] = 0;      // Reserved
        atapiCmd[3] = 0;      // Reserved
        atapiCmd[4] = 2;      // 2 = Eject (use 3 to load)
        atapiCmd[5] = 0;      // Control


        ATAPICMD(atapiCmd, 0, 0);
    }

    uint8_t* Port::GetMediumSize(){
        uint8_t atapiCmd[12] = {
            0x25,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00
        };

        uint8_t buffer[8];

        ATAPICMD(atapiCmd, &buffer, 8);

        uint8_t* cpy = (uint8_t*)malloc(8);
        memcpy((void*)cpy, (void*)&buffer, 8);
        return cpy;

        /*
        uint32_t* data = (uint32_t*)port->GetMediumSize();
        uint32_t lba = *data;
        uint32_t len = *((uint32_t*)((uint64_t)data + 4));
        uint32_t capacity = ((lba + 1) * len) / (1024 * 1024); //In MB
        */
    }

    bool Port::SetSpeedPI(uint8_t speed){
        uint8_t setSpeedCmd[12] = {
            0xBB, 0x00,  // Command + Reserved
            0x1C, 0x20,  // Read Speed (7200 KB/s = 0x1C20)
            0x00, 0x00,  // Write Speed (ignored)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Reserved
        };
        
        ATAPICMD(setSpeedCmd, 0, 0);
    }

    bool Port::ATAPI_READ_TOC(void* memory) {
        uint8_t atapi_command[12] = {0x43, 0, 0, 0, 0, 0, 0, 0, 252, 0, 0, 0};
        return ATAPICMD(atapi_command, memory, 252);
    }


    bool Port::ReadPI(uint64_t sector, uint32_t sectorCount, void* buffer) {
        uint8_t atapiCmd[12] = {
            0x25,  // Command code (Inquiry)
            0x00,  // Reserved
            0x00,  // LUN (Logical Unit Number)
            0x00,  // Reserved
            0x00,  // Allocation length (e.g., 36 bytes)
            0x00,  // Control (no control flags)
            0x00,  // Reserved
            0x00,  // Reserved
            0x00,  // Reserved
            0x00,  // Reserved
            0x00,  // Reserved
            0x00   // Reserved
        };
        uint16_t spin = 0;
    
        // Clear interrupt status
        hbaPort->interruptStatus = (uint32_t)0;
    
        // Get Command Header
        HBACommandHeader* cmdHeader = (HBACommandHeader*)hbaPort->commandListBase;
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // Command FIS size
        cmdHeader->write = 0;  // Read operation (not actually writing)
        cmdHeader->prdtLength = 1;  // No data transfer needed for eject
        cmdHeader->atapi = 1;  // ATAPI command
    
        // Get Command Table
        HBACommandTable* commandTable = (HBACommandTable*)(cmdHeader->commandTableBaseAddress);
        memset(commandTable, 0, sizeof(HBACommandTable));
    
        memcpy(commandTable->atapiCommand, atapiCmd, 12);

        // Set up the PRDT entry (Physical Region Descriptor Table)
        commandTable->prdtEntry[0].dataBaseAddress = (uint32_t)(uint64_t)buffer;
        commandTable->prdtEntry[0].dataBaseAddressUpper = (uint32_t)((uint64_t)buffer >> 32);
        commandTable->prdtEntry[0].byteCount = 36 - 1;  // Inquiry response buffer size (36 bytes)
        commandTable->prdtEntry[0].interruptOnCompletion = 1;  // Interrupt on completion
    
        // Fill Command FIS
        FIS_REG_H2D* cmdFIS = (FIS_REG_H2D*)(&commandTable->commandFIS);
        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1; // Is Command
        cmdFIS->command = ATA_CMD_PACKET; // ATAPI Packet command (0xA0)
        cmdFIS->deviceRegister = 1 << 6; // LBA Mode
        cmdFIS->countLow = 0x1; // Always 1 for ATAPI

        uint32_t tfd = hbaPort->taskFileData;
        if (hbaPort->sataError){
            hbaPort->sataError |= 1 << 16;
        }
        hbaPort->taskFileData = 0;

        while(tfd & (1 << 7)){
            tfd = hbaPort->taskFileData;
        }
        while(tfd & (1 << 3)){
            tfd = hbaPort->taskFileData;
        }

        // Issue command
        hbaPort->commandIssue = 0;
        while(hbaPort->commandIssue);
        hbaPort->commandIssue = 1;
    
        // Wait for completion
        while (true) {
            if (hbaPort->commandIssue == 0) break; // Command completed
    
            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                return false; // Command failed
            }
        }
    
        return true;
        
    }
    
    
}