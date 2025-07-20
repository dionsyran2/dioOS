#include <scheduling/apic/ap_init.h>
#include <kernel.h>
#include <ArrayList.h>

bool copied_init_code = false;

ArrayList<ApplicationProcessor>* ApplicationProcessors = nullptr;

#define GDT_DATA_OFFSET 100
#define GDT_DESCRIPTOR_OFFSET 10

tss_entry_t* tss_entry;
void CopyGDT(){
    uint64_t code_end_offset = ((uint64_t)ap_start_end) - ((uint64_t)ap_start); 
    void* dest = (void*)((uint64_t)lowestFreeSeg + code_end_offset + GDT_DATA_OFFSET);

    memcpy(dest, &ap_gdt_start, ((uint64_t)&ap_gdt_end) - ((uint64_t)&ap_gdt_start));

    GDTDescriptor* descriptor = (GDTDescriptor*)((uint64_t)lowestFreeSeg + code_end_offset + GDT_DESCRIPTOR_OFFSET);
    descriptor->Size = (((uint64_t)&ap_gdt_end) - ((uint64_t)&ap_gdt_start)) - 1;
    descriptor->Offset = (uint64_t)dest;



    /*LONG MODE*/


    void* gdtAddress = GlobalAllocator.RequestPage();
    tss_entry = (tss_entry_t*)gdtAddress;

    GDTDescriptor* ap_gdt_long = (GDTDescriptor*)((uint64_t)tss_entry + sizeof(tss_entry_t));
    write_tss(&DefaultGDT.TSS, tss_entry);
    memcpy((void*)((uint64_t)ap_gdt_long + sizeof(GDTDescriptor)), &DefaultGDT, sizeof(GDT)); //Write the default gdt after the descriptor

    ap_gdt_long->Size = sizeof(GDT) - 1;
    ap_gdt_long->Offset = (uint64_t)ap_gdt_long + sizeof(GDTDescriptor);

    ap_long_mode_gdt_address = (uint32_t)(uint64_t)ap_gdt_long;

    ap_krnl_page_table = (uint32_t)(uint64_t)globalPTM.PML4;

    ap_krnl_stack = tss_entry->rsp0;
}

void CopyInitCode(){
    copied_init_code = true;
    
    memcpy(lowestFreeSeg, (void*)ap_start, ((uint64_t)ap_start_end) - ((uint64_t)ap_start));
}

bool VerifyInitCode(){
    bool ret = memcmp(lowestFreeSeg, (void*)ap_start, ((uint64_t)ap_start_end) - ((uint64_t)ap_start)) == 0;
    if (!ret){
        kprintf("Could not verify ap init code\n");
    }
    return ret;
}

bool ap_started = false;
bool initialized_all_aps = false;
bool retrying = false;
uint16_t active_cpus = 1; // Starts at one, since the bsp is already enabled :) 

void WaitForAPStart(){
    int timeout = 500;

    while (!ap_started){
        if (timeout == 0) break;
        timeout--;
        Sleep(1);
    }
}
bool StartAP(uint8_t APICID){
    if (copied_init_code == false) CopyInitCode();
    CopyGDT();

    ap_started = false;

    if (!VerifyInitCode()) return false;
    SendInitIPI(APICID);

    if (!VerifyInitCode()) return false;
    SendStartupIPI(APICID);

    ApplicationProcessor ap;
    ap.APICID = APICID;
    ap.CPUID = active_cpus;
    ap.idtr.Offset = (uint64_t)GlobalAllocator.RequestPage();
    ap.idtr.Limit = 0x0FFF;
    ap.tss = tss_entry; // should not have been changed, since we start one ap at a time


    ApplicationProcessors->add(ap);

    WaitForAPStart();

    if (ap_started){
        active_cpus++;
        kprintf("[SMP] Processor (APIC ID) %d has started successfully!\n", APICID);
    }else{
        kprintf("[SMP] Processor (APIC ID) %d failed to start!\n", APICID);

        // Send second SIPI
        SendStartupIPI(APICID);
        WaitForAPStart();
        if (ap_started) return true;
        
        ApplicationProcessors->remove(ap);
        return false;
    }



    return true;
}

int StartAllAPs(){
    initialized_all_aps = false;
    if (ApplicationProcessors == nullptr){
        ApplicationProcessors = new ArrayList<ApplicationProcessor>();
    }

    uint16_t offset = 0x2c;
    int32_t remaining = madt->MADTHdr->Length - offset;
    while(remaining > 0){
        Entry* entry = (Entry*)((uint64_t)madt->MADTBuffer + offset);
        offset += entry->Length;
        remaining -= entry->Length;

        if (entry->Type == 0){
            LAPIC* lapic = (LAPIC*)((uint64_t)entry + sizeof(Entry));
            
            if (lapic->APICID == get_apic_id()) continue;

            if ((lapic->Flags & 0x11) == 0) continue; // Since both bits are 0, the cpu cannot be enabled
            
            StartAP(lapic->APICID);
        }
    }

    kprintf("[SMP] Total active cpus: %d\n", active_cpus);
    initialized_all_aps = true;
    return active_cpus;
}

void SetupAP(){
    IDTR idtr;
    idtr.Limit = 0x0FFF;
    idtr.Offset = (uint64_t)GlobalAllocator.RequestPage();
    
    SetIDTGate((void*)TESTINT_Handler, 0x43, IDT_TA_InterruptGate_U, 0x08, &idtr);
    SetIDTGate((void*)DivisionError_Handler, 0x0, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)BoundRangeExceed_Handler, 0x5, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)InvalidOpcode_Handler, 0x6, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)DeviceNotAvailable_Handler, 0x7, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)InvalidTSS_Handler, 0xA, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SegmentNotPresent_Handler, 0xB, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)StackSegFault_Handler, 0xC, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)x87FloatingPointEx_Handler, 0x10, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)AlignementCheck_Handler, 0x11, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SIMDFloatPointEx_Handler, 0x13, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)VirtualizationEx_Handler, 0x14, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)CPEx_Handler, 0x15, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)HypervisorInjEx_Handler, 0x1C, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)VMMCommunicationEx_Handler, 0x1D, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)SecurityEx_Handler, 0x1E, IDT_TA_InterruptGate, 0x08, &idtr);

    SetIDTGate((void*)PageFault_Handler, 0xE, IDT_TA_InterruptGate, 0x08, &idtr); //Page Fault
    SetIDTGate((void*)DoubleFault_Handler, 0x8, IDT_TA_InterruptGate, 0x08, &idtr); //Double Fault
    SetIDTGate((void*)GPFault_Handler, 0xd, IDT_TA_InterruptGate, 0x08, &idtr); //General Protection Faul
    SetIDTGate((void*)HLT_IPI_Handler, IPI_VECTOR_HALT_EXECUTION, IDT_TA_InterruptGate, 0x08, &idtr);
    SetIDTGate((void*)APICTimerInt_ASM_Handler, 0x23, IDT_TA_InterruptGate, 0x08, &idtr);

    asm ("lidt %0" : : "m" (idtr));
}