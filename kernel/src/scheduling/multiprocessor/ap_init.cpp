#include <scheduling/multiprocessor/ap_init.h>
#include <scheduling/apic/lapic.h>
#include <scheduling/apic/ioapic.h>
#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <drivers/timers/common.h>
#include <local.h>
#include <memory.h>
#include <kstdio.h>

#define AP_STACK_SIZE 0x2000
volatile bool ap_flag = false;
int total_woken_aps = 0;
void ap_entry(){
    ap_flag = true;
    
    ap_setup();
}



void copy_trampoline(){
    size_t blob_size = (size_t)(trampoline_blob_end - trampoline_blob_start);

    // Identity map it
    for (size_t i = 0; i < blob_size; i += 0x1000){
        globalPTM.MapMemory((void*)(ap_trampoline_base + i), (void*)(ap_trampoline_base + i));
        globalPTM.SetFlag((void*)(ap_trampoline_base + i), PT_Flag::CacheDisable, true);
    }

    
    memcpy((void*)ap_trampoline_base, trampoline_blob_start, blob_size);
    volatile uint64_t* data = (volatile uint64_t*)(ap_trampoline_base + 0x10);
    data[0] = virtual_to_physical((uint64_t)globalPTM.PML4); // Page Table
    data[2] = (uint64_t)ap_entry; // Entry
}

void unmap_trampoline(){
    size_t blob_size = (size_t)(trampoline_blob_end - trampoline_blob_start);

    // Unmap it
    for (size_t i = 0; i < blob_size; i += 0x1000){
        globalPTM.Unmap((void*)(ap_trampoline_base + i));
    }
}

void start_all_aps(){
    cpu_local_data* local = get_cpu_local_data();

    copy_trampoline();
    volatile uint64_t* data = (volatile uint64_t*)(ap_trampoline_base + 0x10);

    // Loop through the MADT
    uint16_t offset = sizeof(madt_header_t);
    int32_t remaining = madt_header->Length - offset;

    while(remaining > 0){
        madt_entry_t* entry = (madt_entry_t*)((uint64_t)madt_header + offset);
        madt_lapic_entry* lapic = (madt_lapic_entry*)entry;
        if (entry->Type == 0){
            if (lapic->Flags != 0 && lapic->APICID != local->lapic->lapic_id){
                //kprintf("Starting %d\n", lapic->APICID);
                data[1] = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(AP_STACK_SIZE, 0x1000)) + AP_STACK_SIZE; // Stack (Allocate it first)
                
                ap_flag = false;
                
                send_init_ipi(lapic->APICID); // Init IPI

                Sleep(15);

                send_sipi(lapic->APICID, ap_trampoline_base); // Start IPI
                
                nanosleep(15);
                if (!ap_flag) send_sipi(lapic->APICID, ap_trampoline_base); // Send the second sipi

                uint64_t timeout = 100;
                while (!ap_flag && timeout > 0) {
                    timeout--;
                    Sleep(10);
                }
                if (timeout != 0) total_woken_aps++;
            }
        }

        offset += entry->Length;
        remaining -= entry->Length;
    }

    int timeout = 1000;
    while (total_woken_aps != (local_cpu_cnt - 1) && timeout > 0){
        timeout--;
        Sleep(10);
    }
}