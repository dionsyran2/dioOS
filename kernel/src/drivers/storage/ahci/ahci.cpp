#include <memory.h>
#include <drivers/storage/ahci/ahci.h>
#include <BasicRenderer.h>
#include <paging/PageTableManager.h>
#include <memory/heap.h>
#include <paging/PageFrameAllocator.h>
#include <cstr.h>
#include <scheduling/apic/apic.h>
#include <VFS/vfs.h>

const char* charToConstCharPtr_(char c) {
    static char buffer[2]; // 1 char + 1 for null terminator
    buffer[0] = c;         // Set the character
    buffer[1] = '\0';      // Null terminate the string
    return buffer;         // Return the pointer to the buffer
}


namespace AHCI{
    #define HBA_PORT_DEV_PRESENT 0x3
    #define HBA_PORT_IPM_ACTIVE 0x1
    #define SATA_SIG_ATAPI 0xEB140101
    #define SATA_SIG_ATA 0x00000101
    #define SATA_SIG_SEMB 0xC33C0101
    #define SATA_SIG_PM 0x96690101
    #define HBA_PxCMD_CR 0x8000
    #define HBA_PxCMD_FRE 0x0010
    #define HBA_PxCMD_ST 0x0001
    #define HBA_PxCMD_FR 0x4000
    #define ATA_CMD_IDENTIFY 0xEC
    #define SECTOR_SIZE 512

    /* VFS */
    bool read_op(uint64_t block, uint32_t block_count, void* buffer, vblk* blk){
        Port* port = (Port*)blk->drv_data;

        return port->Read(block, block_count, buffer);
    }

    bool write_op(uint64_t block, uint32_t block_count, const void* buffer, vblk* blk){
        Port* port = (Port*)blk->drv_data;

        return port->Write(block, block_count, buffer);
    }


    char* mount_names[] = {
        "sda",
        "sdb",
        "sdc",
        "sdd",
        "sde",
        "sdf",
        "sdg",
        "sdh",
        "sdi",
        "sdj",
        "sdk",
        "sdl",
        "sdm",
        "sdn",
        "sdo",
        "sdp",
        "sdq",
        "sdr",
        "sds",
        "sdt",
        "sdy",
        "sdv",
        "sdw",
        "sdx",
        "sdy",
        "sdz"
    };

    int offset_in_mount_names = 0;

    PortType CheckPortType(HBAPort* port){
        uint32_t sataStatus = port->sataStatus;
        uint8_t interfacePowerManagement = (sataStatus >> 8) & 0b111;
        uint8_t deviceDetection = sataStatus & 0b111;

        /*// Perform a port reset
        port->cmdSts &= ~(1 << 0);  // Disable the port (clear ST bit)
        while (port->cmdSts & (1 << 15));  // Wait for CR to clear

        // Issue COMRESET
        port->sataControl |= (1 << 0);  // Set DET = 1 (COMRESET)
        for (volatile int i = 0; i < 100000; i++);  // Small delay
        port->sataControl &= ~(1 << 0);  // Clear DET

        // Wait for device detection
        int i = 0;
        while(i < 10000 || deviceDetection == 1 || deviceDetection == 2 || (port->signature == 0xFFFFFFFF && deviceDetection != 0)){
            sataStatus = port->sataStatus;
            interfacePowerManagement = (sataStatus >> 8) & 0b111;
            deviceDetection = sataStatus & 0b111;
            
            if (deviceDetection == 3 && port->signature != 0xFFFFFFFF) {  // 3 = Device detected & communicating
                kprintf("Device detected!\n");
                break;
            }
            i++;
        }

        kprintf("DEV_P: %hh | IPM: %hh | sig: %x\n", deviceDetection, interfacePowerManagement, port->signature);*/

        
        if (deviceDetection != HBA_PORT_DEV_PRESENT){
            return PortType::None;
        }
        if (interfacePowerManagement != HBA_PORT_IPM_ACTIVE){
            return PortType::None;
        }

        switch (port->signature){
            case SATA_SIG_ATAPI:
                return PortType::SATAPI;
            case SATA_SIG_ATA:
                return PortType::SATA;
            case SATA_SIG_PM:
                return PortType::PM;
            case SATA_SIG_SEMB:
                return PortType::SEMB;
            default:
                return PortType::None;
        }
    }


    void AHCIDriver::ProbePorts(){
        uint32_t portsImpemented = ABAR->portsImplemented;
        for (int i = 0; i < 32; i++){
            if (portsImpemented & (1 << i)){
                PortType portType_ = CheckPortType(&ABAR->ports[i]);
                if (portType_ == PortType::SATA || portType_ == PortType::SATAPI){
                    Ports[portCount] = (Port*)GlobalAllocator.RequestPage();
                    Ports[portCount]->portType = portType_;
                    Ports[portCount]->hbaPort = &ABAR->ports[i];
                    Ports[portCount]->portNumber = portCount;
                    portCount++;
                }
            }
        }
    }
    void Port::Configure(){
        StopCMD();

        void* newBase = GlobalAllocator.RequestPage();
        hbaPort->commandListBase = (uint32_t)(uint64_t)newBase;
        hbaPort->commandListBaseUpper = (uint32_t)((uint64_t)newBase >> 32);

        memset((void*)hbaPort->commandListBase, 0, 1024);

        void* fisBase = GlobalAllocator.RequestPage();
        hbaPort->fisBaseAddress = (uint32_t)(uint64_t)fisBase;
        hbaPort->fisBaseAddressUpper = (uint32_t)((uint64_t)fisBase >> 32);

        memset(fisBase, 0, 256);

        HBACommandHeader* cmdHeader = (HBACommandHeader*)((uint64_t)hbaPort->commandListBase + ((uint64_t)hbaPort->commandListBaseUpper << 32));

        for (int i = 0; i < 32; i++){
            cmdHeader[i].prdtLength = 8;

            void* cmdTableAddress = GlobalAllocator.RequestPage();
            uint64_t address = (uint64_t)cmdTableAddress + (i << 8);
            cmdHeader[i].commandTableBaseAddress = (uint32_t)address;
            cmdHeader[i].commandTableBaseAddressUpper = (uint32_t)((uint64_t)address >> 32);
            memset(cmdTableAddress, 0, 256);
        }


        StartCMD();
    }

    void Port::StopCMD(){
        hbaPort->cmdSts &= ~HBA_PxCMD_ST;
        hbaPort->cmdSts &= ~HBA_PxCMD_FRE;


        while (true){
            if (hbaPort->cmdSts & HBA_PxCMD_FR) continue;
            if (hbaPort->cmdSts & HBA_PxCMD_CR) continue;
            break;
        }
    }

    void Port::StartCMD(){
        while (hbaPort->cmdSts & HBA_PxCMD_CR);

        hbaPort->cmdSts |= HBA_PxCMD_FRE;
        hbaPort->cmdSts |= HBA_PxCMD_ST;
    }
    bool Port::Write(uint64_t sector, uint32_t sectorCount, const void* buffer) {
        if (sectorCount > ((4 * 1024 * 1024) / 512)){
            uint32_t SectorCnt = sectorCount;
            uint32_t max_sectors = ((4 * 1024 * 1024) / 512);
            uint32_t offset = 0;
            while (SectorCnt){
                uint32_t to_read = SectorCnt > max_sectors ? max_sectors : SectorCnt;
                bool ret = Write(sector + offset, to_read, (uint8_t*)buffer + offset);
                if (ret == false) return false;
                offset += to_read * 512;
                SectorCnt -= to_read;
            }
            return true;
        }
        uint16_t spin = 0;
        while ((hbaPort->taskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
            spin++;
        }

        uint32_t sectorL = (uint32_t)sector;
        uint32_t sectorH = (uint32_t)(sector >> 32);

        hbaPort->interruptStatus = (uint32_t)-1;

        HBACommandHeader* cmdHeader = (HBACommandHeader*)hbaPort->commandListBase;
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t); // COMMAND FIS SIZE

        cmdHeader->write = 1; // Set to 1 for Write
        cmdHeader->prdtLength = 1;

        HBACommandTable* commandTable = (HBACommandTable*)(cmdHeader->commandTableBaseAddress);
        memset(commandTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        commandTable->prdtEntry[0].dataBaseAddress = (uint32_t)(uint64_t)buffer;
        commandTable->prdtEntry[0].dataBaseAddressUpper = (uint32_t)((uint64_t)buffer >> 32);
        commandTable->prdtEntry[0].byteCount = (sectorCount << 9) - 1; // 512 bytes per sector
        commandTable->prdtEntry[0].interruptOnCompletion = 1; // Will interrupt on completion

        FIS_REG_H2D* cmdFIS = (FIS_REG_H2D*)(&commandTable->commandFIS);

        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1; // Is Command!
        cmdFIS->command = ATA_CMD_WRITE_DMA_EX; // Use the write command

        cmdFIS->lba0 = (uint8_t)sectorL;
        cmdFIS->lba1 = (uint8_t)(sectorL >> 8);
        cmdFIS->lba2 = (uint8_t)(sectorL >> 16);
        cmdFIS->lba3 = (uint8_t)(sectorH);
        cmdFIS->lba4 = (uint8_t)(sectorH >> 8);
        cmdFIS->lba5 = (uint8_t)(sectorH >> 16); // Correct the indexing here

        cmdFIS->deviceRegister = 1 << 6; // LBA Mode

        cmdFIS->countLow = sectorCount & 0xFF;
        cmdFIS->countHigh = (sectorCount >> 8) & 0xFF;

        if (spin == 1000000) {
            return false;
        }

        hbaPort->commandIssue = 1; // Issue the command

        while (true) {
            if (hbaPort->commandIssue == 0) break;

            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                return false;
            }
        }

        return true;
    }

    bool Port::IdentifyDevice(uint16_t* outBuffer512) {
        // Wait for device to be ready
        uint16_t spin = 0;
        while ((hbaPort->taskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000) {
            spin++;
        }
        if (spin == 1000000) return false;

        hbaPort->interruptStatus = (uint32_t)-1; // Clear interrupt status

        HBACommandHeader* cmdHeader = (HBACommandHeader*)hbaPort->commandListBase;
        cmdHeader->commandFISLength = sizeof(FIS_REG_H2D) / sizeof(uint32_t);
        cmdHeader->write = 0;  // Read from device
        cmdHeader->prdtLength = 1;

        HBACommandTable* cmdTable = (HBACommandTable*)(cmdHeader->commandTableBaseAddress);
        memset(cmdTable, 0, sizeof(HBACommandTable) + (cmdHeader->prdtLength - 1) * sizeof(HBAPRDTEntry));

        // Use your outBuffer512 as data buffer for PRDT
        cmdTable->prdtEntry[0].dataBaseAddress = (uint32_t)(uint64_t)outBuffer512;
        cmdTable->prdtEntry[0].dataBaseAddressUpper = (uint32_t)((uint64_t)outBuffer512 >> 32);
        cmdTable->prdtEntry[0].byteCount = SECTOR_SIZE - 1; // 512 bytes - 1
        cmdTable->prdtEntry[0].interruptOnCompletion = 1;

        FIS_REG_H2D* cmdFIS = (FIS_REG_H2D*)(&cmdTable->commandFIS);

        cmdFIS->fisType = FIS_TYPE_REG_H2D;
        cmdFIS->commandControl = 1; // Command
        cmdFIS->command = ATA_CMD_IDENTIFY;

        cmdFIS->deviceRegister = 0; // Clear for IDENTIFY device

        hbaPort->commandIssue = 1; // Issue the command

        // Wait for command completion
        while (true) {
            if ((hbaPort->commandIssue & 1) == 0) break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES) {
                return false; // Error
            }
        }

        return true;
    }

    // Find a free command list slot
    int find_cmdslot(Port *port)
    {
        // If not set in SACT and CI, the slot is free
        uint32_t slots = (port->hbaPort->sataActive | port->hbaPort->commandIssue);
        for (int i=0; i < 32; i++)
        {
            if ((slots&1) == 0)
                return i;
            slots >>= 1;
        }
        return -1;
    }

    #define MAX_SECTORS_PER_REQ ((256 * 8 * 1024) / 512)
    bool Port::Read(uint64_t Sector, uint32_t SectorCount, void* Buffer){
        if (SectorCount > MAX_SECTORS_PER_REQ){
            while (SectorCount > 0){
                uint32_t to_read = MAX_SECTORS_PER_REQ;
                if (SectorCount < to_read) to_read = SectorCount;
                bool ret = Read(Sector, to_read, Buffer);
                Sector += to_read;
                SectorCount -= to_read;
                Buffer = (void*)((uint64_t)Buffer + (to_read * 512));
                if (!ret) return false;
            }
            return true;
        }
        
        hbaPort->interruptStatus = (uint32_t)-1;
        int spin = 0;
        int slot = find_cmdslot(this);
        if (slot == -1) return false;

        HBACommandHeader* CommandHeader = (HBACommandHeader*)hbaPort->commandListBase;
        CommandHeader += slot;
        CommandHeader->commandFISLength = sizeof(FIS_REG_H2D)/ sizeof(uint32_t); //command FIS size;
        CommandHeader->write = 0; // this is a read command
        CommandHeader->prdtLength = (uint16_t)((SectorCount-1) >> 4) + 1;

        HBACommandTable* CommandTable = (HBACommandTable*)(CommandHeader->commandTableBaseAddress);
        memset(CommandTable, 0, sizeof(HBACommandTable) + ((CommandHeader->prdtLength-1) * sizeof(HBAPRDTEntry)));

        FIS_REG_H2D* CommandFIS = (FIS_REG_H2D*)(&CommandTable->commandFIS);
        
        CommandFIS->countLow = SectorCount & 0xFF;
        CommandFIS->countHigh = (SectorCount >> 8) & 0xFF;


        // 8K bytes (16 sectors) per PRDT
        for (int i=0; i<CommandHeader->prdtLength; i++)
        {
            uint8_t to_read = 16;
            if (SectorCount < 16) to_read = SectorCount;
            if (SectorCount == 0) break;

            CommandTable->prdtEntry[i].dataBaseAddress = (uint32_t)((uint64_t)Buffer & 0xFFFFFFFF);
            CommandTable->prdtEntry[i].dataBaseAddressUpper = (uint32_t)((uint64_t)Buffer >> 32);

            CommandTable->prdtEntry[i].byteCount = (to_read * 512) - 1;
            CommandTable->prdtEntry[i].interruptOnCompletion = 0;
            Buffer = (void*)((uint64_t)Buffer + (to_read * 512));
            SectorCount -= to_read;	// 16 sectors
        }
        CommandTable->prdtEntry[CommandHeader->prdtLength - 1].interruptOnCompletion = 1;

        CommandFIS->fisType = FIS_TYPE_REG_H2D;
        CommandFIS->commandControl = 1; // command
        CommandFIS->command = ATA_CMD_READ_DMA_EX;

        uint32_t SectorL = (uint32_t)Sector;
        uint32_t SectorH = (uint32_t)(Sector >> 32);

        CommandFIS->lba0 = (uint8_t)SectorL;
        CommandFIS->lba1 = (uint8_t)(SectorL >> 8);
        CommandFIS->lba2 = (uint8_t)(SectorL >> 16);
        CommandFIS->lba3 = (uint8_t)(SectorL >> 24);
        CommandFIS->lba4 = (uint8_t)SectorH;
        CommandFIS->lba5 = (uint8_t)(SectorH >> 8) & 0x0F; // Only lower 4 bits needed


        CommandFIS->deviceRegister = 1 << 6; // LBA mode
        while ((hbaPort->taskFileData & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
        {
            spin++;
        }

        if (spin == 1000000)
        {
            serialf("\e[0;35m[AHCI] Port is hung\e[0m\n");
            return false;
        }

        hbaPort->commandIssue = 1 << slot;
        spin = 0;
        asm ("sti");
        while (spin < 100){
            Sleep(1);
            spin++;
            if((hbaPort->commandIssue == 0)) break;
            if(hbaPort->interruptStatus & HBA_PxIS_TFES)
            {
                return false;
            }
        }

        if (spin == 100){
            serialf("\e[0;35m[AHCI] Could not issue command (Timed out).\e[0m\n");
        }

        spin = 0;
        // Wait for completion
        while (spin < 100)
        {
            Sleep(1);
            spin++;
            // In some longer duration reads, it may be helpful to spin on the DPS bit 
            // in the PxIS port field as well (1 << 5)
            if ((hbaPort->commandIssue & (1 << slot)) == 0) 
                break;
            if (hbaPort->interruptStatus & HBA_PxIS_TFES)	// Task file error
            {
                serialf("\e[0;35m[AHCI] Disk read error (Task file error)\e[0m\n");
                return false;
            }
        }
        
        if (spin == 100){
            serialf("\e[0;35m[AHCI] Disk read error (Timed out).\e[0m\n");
        }

        // Check again
        if (hbaPort->interruptStatus & HBA_PxIS_TFES)
        {
            serialf("\e[0;35m[AHCI] Disk read error (Task file error)!\e[0m\n");
            return false;
        }

        return true;
    }

    AHCIDriver::AHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress) : PCI_BASE_DRIVER(pciBaseAddress){
        this->PCIBaseAddress = pciBaseAddress;
        this->type = PCI_DRIVER_AHCI;
    }

    AHCIDriver::~AHCIDriver(){

    }

    bool AHCIDriver::init_device(){
        ABAR = (HBAMemory*)((PCI::PCIHeader0*) PCIBaseAddress)->BAR5;

        globalPTM.MapMemory(ABAR, ABAR);

        kprintf(0x00FF00, "[AHCI]");
        kprintf(" Driver instance initialized\n");
        return true;
    }

    /* Just for testing... to be removed*/
    void SwapWordsToString(const uint16_t* words, int length, char* outStr) {
        for (int i = 0; i < length; i++) {
            outStr[i * 2] = words[i] >> 8;
            outStr[i * 2 + 1] = words[i] & 0xFF;
        }
        outStr[length * 2] = '\0';
    }

    bool AHCIDriver::start_device(){
        ABAR->globalHostControl |= 1 << 31;


        ProbePorts();
        uint32_t portsImpemented = ABAR->portsImplemented;
        //kprintf("PI: %d | PS: %d\n", portsImpemented, ABAR->hostCapability & 0xF);
        
        for (int i = 0; i < portCount; i++){
            kprintf("Port: %d\n", i);
            Port* port = Ports[i];
            
            port->Configure();
            Sleep(10);
            
            if (port->portType == PortType::SATA) {
                char* name = mount_names[offset_in_mount_names];
                offset_in_mount_names++;

                vnode_t* dev = vfs::resolve_path("/dev");
                if (dev == nullptr) dev = vfs::mount_node("dev", VDIR, vfs::get_root_node());
                
                vblk_t* blk = (vblk_t*)vfs::mount_node(name, VBLK, dev);
                blk->drv_data = (void*)port;
                blk->blk_ops.read = read_op;
                blk->blk_ops.write = write_op;

                uint16_t identifyBuffer[256]; // 512 bytes = 256 * 16-bit words
                if (port->IdentifyDevice(identifyBuffer)) {
                    bool supports_48bit = (identifyBuffer[83] & (1 << 10)) != 0;
                    uint64_t sectors = 0;
                    if (supports_48bit) {
                        sectors = (uint64_t)identifyBuffer[100]
                                | ((uint64_t)identifyBuffer[101] << 16)
                                | ((uint64_t)identifyBuffer[102] << 32)
                                | ((uint64_t)identifyBuffer[103] << 48);
                    } else {
                        sectors = (uint64_t)identifyBuffer[60]
                                | ((uint64_t)identifyBuffer[61] << 16);
                    }

                    uint64_t size_bytes = sectors * SECTOR_SIZE;
                    kprintf("Sector count: %ll ... Size: %ll MB\n", sectors, size_bytes / (1024 * 1024));
                    blk->block_count = sectors;
                    blk->block_size = SECTOR_SIZE;


                    char model[41];
                    SwapWordsToString(&identifyBuffer[27], 20, model);

                    kprintf("Model: %s\n", model);

                    GPT::initDisk(blk);
                    // Typically sector size is 512 bytes, but you can check words 106-108 for logical sector size info
                } else {
                    kprintf("[AHCI] Identify command failed!\n");
                }
            };
        }
        return true;
    }

    bool AHCIDriver::shutdown_device(){
        return true;
    }
}