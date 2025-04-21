#include "XHCI.h"

namespace XHCI{
    bool initInst = false;
    XHCI::Driver* Instances[20];

    void GenIntHndlr(){
        uint8_t offset = 0;
        while (Instances[offset] != nullptr && offset != 20){
            Instances[offset]->IntHandler();
            offset++;
        }
    }

    void Driver::Prefix(){
        globalRenderer->printf(0xF0F000, "[XHCI] ");
    }

    void Driver::IntHandler(){
        Prefix();
        globalRenderer->printf("Interrupt Triggered!\n");
    }
    
    void Driver::ResetHC(){
        uint32_t* cmd = (uint32_t*)(((uint64_t)opbase) + XUSBCMD);
        uint32_t* sts = (uint32_t*)(((uint64_t)opbase) + XUSBSTS);
        uint32_t* cfg = (uint32_t*)(((uint64_t)opbase) + XUSBCFG);
        uint64_t address = (uint64_t)hdr->BAR0 & 0xFFFFFFF0;
        uint32_t* hcs1 = base + XHCSPARAMS1;
        uint8_t caplen = (*(uint32_t*)base) & 0xFF;

        Prefix();
        globalRenderer->printf("CMD 0x%x STS 0x%x CFG 0x%x ADDR 0x%llx Max Slots: %d Length: %d\n", *cmd, *sts, *cfg, address, (*hcs1) & 0xFF, caplen);
        *cmd &= ~USBCMD_RS;
        while(!(*sts & USBSTS_HCH));

        Prefix();
        globalRenderer->printf("Reset Complete!\n");
    }

    void Driver::ConfigureMaxSlots(){
        uint32_t* hcs1 = base + XHCSPARAMS1;
        uint32_t* cmd = (uint32_t*)(((uint64_t)opbase) + XUSBCMD);
        uint32_t* sts = (uint32_t*)(((uint64_t)opbase) + XUSBSTS);
        uint32_t* cfg = (uint32_t*)(((uint64_t)opbase) + XUSBCFG);

        /* Configure Max Slots */
        uint8_t maxSlots = 40;
        if (((*hcs1) & 0xFF) < 40){
            maxSlots = (*hcs1) & 0xFF;
        }

        *cfg &= ~0xFF; // Erase any previous value (first byte - max slots)
        *cfg |= maxSlots; // Write the new value

        Prefix();
        globalRenderer->printf("Max slots set to: %d | RUN: %d | HLT: %d\n", *cfg & 0xFF, ((*cmd) & USBCMD_RS), (*sts) & USBSTS_HCH);
    }

    void Driver::ConfigureDeviceContext(){
        uint32_t* cfg = (uint32_t*)(((uint64_t)opbase) + XUSBCFG);
        /* Device Context */
        size_t SizeOfDCs = sizeof(DeviceContext) * (*cfg & 0xFF);
        uint32_t PagesForDCBAAP = (SizeOfDCs / 0x1000) + 1;
        void* DCBAAP = GlobalAllocator.RequestPages(PagesForDCBAAP);
        memset(DCBAAP, 0, PagesForDCBAAP * 0x1000);
        Prefix();
        globalRenderer->printf(0xFF0000, "Size of Device Contexts: %d bytes / %d pages\n", SizeOfDCs, PagesForDCBAAP);

        *(uint64_t*)(((uint64_t)opbase) + BCBAREG) = (uint64_t)DCBAAP;
    }

    void Driver::ConfigureCRCR(){
        /* CRCR */
        void* CRCRAlloc = GlobalAllocator.RequestPage();
        memset(CRCRAlloc, 0, 0x1000);
        //Fill CRCR with invalid trd and set the last one to a link
        cr.trb = (TRB_Generic*)CRCRAlloc;
        cr.WP = 0;
        cr.size = 0x1000 / sizeof(TRB_Generic);

        LinkTRB* lastEntry = ((LinkTRB*)(cr.trb + ((cr.size - 1) * sizeof(TRB_Generic))));
        lastEntry->RingSegPtr = ((uint64_t)cr.trb) >> 4;
        lastEntry->IntTarget = 0;
        lastEntry->Cycle = 1;
        lastEntry->IOC = 0;
        lastEntry->TRBType = 6;

        *(uint64_t*)(((uint64_t)opbase) + XCRCR) = ((uint64_t)cr.trb) | 1;

        globalRenderer->printf(0xFF00000, "CRCR Address: 0x%llx | Link Set Address: 0x%llx\n", (uint64_t)cr.trb, lastEntry->RingSegPtr);
    }

    void Driver::ConfigureEventRing(){
        void* EventRingSegmentTable = GlobalAllocator.RequestPage();
        memset(EventRingSegmentTable, 0, 0x1000);

        size_t SizeOfEventRingSegmentTable = 0x1000 / sizeof(EventRingSegTableEntry); // ERST is always gonna be one page long in this case!
        size_t TRB_Per_Segment = 0x1000 / sizeof(TRB_Generic); // each segment is always gonna be one page long in this case!

        /*******************/
        /* Set up the ERST */
        /*******************/

        for (int i = 0; i < SizeOfEventRingSegmentTable; i++){
            EventRingSegTableEntry* entry = (EventRingSegTableEntry*)((uint64_t)EventRingSegmentTable + (i * sizeof(EventRingSegTableEntry)));
            uint64_t allocAddress = (uint64_t)GlobalAllocator.RequestPage();
            entry->RingSegBaseAddressLo = (uint32_t)(allocAddress & 0xFFFFFFFF);
            entry->RingSegBaseAddressHi = (uint32_t)((allocAddress >> 32) & 0xFFFFFFFF);
            entry->RingSegSize = TRB_Per_Segment;
        }


        /**************************/
        /* Set up the ER Segments */
        /**************************/

        for (int i = 0; i < SizeOfEventRingSegmentTable; i++){ // Loop through all of the segments
            EventRingSegTableEntry* entry = (EventRingSegTableEntry*)((uint64_t)EventRingSegmentTable + (i * sizeof(EventRingSegTableEntry)));
            EventRingSegTableEntry* NextEntry = (EventRingSegTableEntry*)((uint64_t)EventRingSegmentTable + ((i + 1) * sizeof(EventRingSegTableEntry)));
            void* seg = (void*)((uint64_t)((entry->RingSegBaseAddressHi << 32) | entry->RingSegBaseAddressLo));
            void* nextSeg = (void*)((uint64_t)((NextEntry->RingSegBaseAddressHi << 32) | NextEntry->RingSegBaseAddressLo));

            //Set up the segments with empty and a link trb
            memset(seg, 0, 0x1000);
            /*LinkTRB* lastTRB = (LinkTRB*)((uint64_t)seg + ((entry->RingSegSize -1)* sizeof(TRB_Generic)));
            lastTRB->Cycle = 1;
            lastTRB->RingSegPtr = ((uint64_t)nextSeg) >> 4;
            lastTRB->TC = 1;
            lastTRB->TRBType = 6;*/
        }

        uint32_t* rt = base + RTSOFF;
        uint32_t* IMAN = rt + 0x20 + (32 * 0); //1st Interrupter
        uint32_t* IMOD = rt + 0x24 + (32 * 0);
        uint32_t* ERSTSZ = rt + 0x28 + (32 * 0); // Table Size
        uint64_t* ERSTBA = (uint64_t*)(rt + 0x30 + (32 * 0)); // Table Base Address
        uint64_t* ERDP = (uint64_t*)(rt + 0x38 + (32 * 0));

        EventRingSegTableEntry* firstEntry = (EventRingSegTableEntry*)((uint64_t)EventRingSegmentTable);

        *ERSTSZ = SizeOfEventRingSegmentTable;
        *ERSTBA = (uint64_t)EventRingSegmentTable;
        *ERDP = (firstEntry->RingSegBaseAddressHi << 32) | firstEntry->RingSegBaseAddressLo;
        *IMAN |= 1 << 1;

        *(opbase + XUSBCMD) |= USBCMD_INTE;

        


        globalRenderer->printf(0x0FF000, "EventRingSegTableEntry size: %d, In a page: %d\n", sizeof(EventRingSegTableEntry), 0x1000 / sizeof(EventRingSegTableEntry));
    }

    void Driver::ConfigureInterrupts(){
        PCI::PCIDeviceHeader* dev = (PCI::PCIDeviceHeader*)hdr;
        PCI::MSIX_Capability* MSIX = (PCI::MSIX_Capability*)PCI::findCap(dev, 0x11);
        if (MSIX == nullptr){
            //USE LEGACY PIC IRQS!
            Prefix();
            globalRenderer->printf("Legacy pic irqs must be used as the device does not support msi(-x)\n");
            return;
        }
        Prefix();
        globalRenderer->printf("Find Cap returned: %llx | DEV: %llx | DIFF: %llx\n", (uint64_t)MSIX, (uint64_t)dev, (uint64_t)MSIX - (uint64_t)dev);
        void* bar_msix = nullptr;

        switch (MSIX->TableOffset & 0x7){
            case 0:
                bar_msix = (void*)hdr->BAR0;
                break;
            case 1:
                bar_msix = (void*)hdr->BAR1;
                break;
            case 2:
                bar_msix = (void*)hdr->BAR2;
                break;
            case 3:
                bar_msix = (void*)hdr->BAR3;
                break;
        }


        
        globalRenderer->printf(0xFF00FF, "MSI-X = %llx | MSI-X Table located at: BAR%d + %x. Table size: %d\n", (uint64_t)MSIX, MSIX->TableOffset & 0x7, MSIX->TableOffset & ~0x7, MSIX->MsgCtrl.tableSize + 1);
        uint32_t ApicLo = (uint32_t)(LAPICAddress & 0xFFFFFFFF);
        uint32_t ApicHi = (uint32_t)((LAPICAddress >> 32) & 0xFFFFFFFF);
        uint32_t MsgData = 0; //Delivery = 0 (fixed), level = 0 (edge) trigger = 0 (active high)
        MsgData |= 0x51; // Interrupt vector for the xhci handler.


        PCI::MSIX_TableEntry* msix_table = (PCI::MSIX_TableEntry*)((uint64_t)bar_msix + (MSIX->TableOffset & ~0x7));
        msix_table[0].MsgAddressLo = ApicLo;
        msix_table[0].MsgAddressHi = ApicHi;
        msix_table[0].MsgData = MsgData;
        msix_table[0].VectorCtrl = 0;

        void* bar_msix_pba = nullptr;

        switch (MSIX->PBAOffset & 0x7){
            case 0:
                bar_msix_pba = (void*)hdr->BAR0;
                break;
            case 1:
                bar_msix_pba = (void*)hdr->BAR1;
                break;
            case 2:
                bar_msix_pba = (void*)hdr->BAR2;
                break;
            case 3:
                bar_msix_pba = (void*)hdr->BAR3;
                break;
        }
    
        MSIX_PBA = (uint8_t*)((uint64_t)bar_msix_pba + (MSIX->PBAOffset & ~0x7));
        globalRenderer->printf(0xFF00FF, "PBA: %d PBA ADDR: %llx\n", *MSIX_PBA, (uint64_t)MSIX_PBA);

        MSIX->MsgCtrl.enable = 1;
        MSIX->MsgCtrl.functionMask = 0;
    }
    Driver::Driver(PCI::PCIDeviceHeader* dev){
        return;
        if (initInst == false){
            for (int i = 0; i < 20; i++){
                Instances[i] = nullptr;
            }
        }
        initInst = true;

        bool InstSet = false;
        for (int i = 0; i < 20; i++){
            if (Instances[i] == nullptr){
                Instances[i] = this;
                InstSet = true;
                break;
            }
        }
        if (InstSet == false){
            Prefix();
            globalRenderer->printf(0xFF0000, "Could not add this instance of the XHCI driver to the Instances array!\n");
            return;
        }

        hdr = (PCI::PCIHeader0*)dev;
        uint64_t address = (uint64_t)hdr->BAR0 & 0xFFFFFFF0;
        address |= (uint64_t)hdr->BAR1 << 32;
        globalPTM.MapMemory((void*)address, (void*)address);
        base = (uint32_t*)address;

        /* CAP */
        uint8_t caplen = (*(uint32_t*)base) & 0xFF;
        uint32_t* hcs1 = base + XHCSPARAMS1;
        opbase = (uint32_t*)(((uint64_t)base) + caplen);

        /* OP */
        uint32_t* cmd = (uint32_t*)(((uint64_t)opbase) + XUSBCMD);
        uint32_t* dnctrl = (uint32_t*)(((uint64_t)opbase) + DNCTRL);
        uint32_t* sts = (uint32_t*)(((uint64_t)opbase) + XUSBSTS);
        uint32_t* cfg = (uint32_t*)(((uint64_t)opbase) + XUSBCFG);

        ResetHC();

        ConfigureMaxSlots();

        ConfigureDeviceContext();

        ConfigureCRCR();

        ConfigureInterrupts();
        
        ConfigureEventRing();

        *dnctrl |= 0xFFFF;
        *cmd |= USBCMD_RS;
        Prefix();
        globalRenderer->printf("RUN: %d | HLT: %d\n", *cfg & 0xFF, ((*cmd) & USBCMD_RS), (*sts) & USBSTS_HCH);
        while(*cmd & USBCMD_RS != 1);
        Test();
    };

    void Driver::Test(){
        /* OP */
        uint32_t* cmd = (uint32_t*)(((uint64_t)opbase) + XUSBCMD);
        uint32_t* dnctrl = (uint32_t*)(((uint64_t)opbase) + DNCTRL);
        uint32_t* sts = (uint32_t*)(((uint64_t)opbase) + XUSBSTS);
        uint32_t* cfg = (uint32_t*)(((uint64_t)opbase) + XUSBCFG);

        Prefix();
        globalRenderer->printf("Running test routine\n");
        /*LinkTRB* lastEntry = ((LinkTRB*)(cr.trb + (0 * sizeof(TRB_Generic))));
        lastEntry->IntTarget = 0;
        lastEntry->Cycle = 1;
        lastEntry->IOC = 1;
        lastEntry->TRBType = 23;*/
        cr.trb[0].ctrl = (23 << 10) | (1 << 5) | 1;

        uint64_t doorbellOffset = *(uint32_t*)((uint64_t)base + DBOFF);
        uint32_t* db = (uint32_t*)((uint64_t)base + doorbellOffset);
        globalPTM.MapMemory((void*)db, (void*)db);
        globalRenderer->printf("DB Offset: %llx | DB Address: %llx\n", doorbellOffset, base + doorbellOffset);
        *db = 0;
        Sleep(1000);

        Prefix();
        globalRenderer->printf("Max slots set to: %d | RUN: %d | HLT: %d\n | PBA: %d\n", *cfg & 0xFF, ((*cmd) & USBCMD_RS), (*sts) & USBSTS_HCH, *MSIX_PBA);

        uint64_t crcr = *(volatile uint64_t*)((uint64_t)opbase + XCRCR);
        globalRenderer->printf("CRCR: %llx\n", crcr);
        uint32_t usbsts = *(volatile uint32_t*)((uint64_t)opbase + XUSBSTS);
        globalRenderer->printf("USBSTS: %x\n", usbsts);
        globalRenderer->printf("FINISHED TEST \n\n");
    }
}