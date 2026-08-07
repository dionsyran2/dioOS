/* A simple acpi implementation */
#include <acpi.h>
#include <memory.h>
#include <paging/PageTableManager.h>
#include <acpica/embedded_controller.h>

ACPI::RSDP2* rsdp;
ACPI::SDTHeader* xsdt;

namespace ACPI{
    void* FindTable(SDTHeader* sdtHeader, char* signature){
        int entries = (sdtHeader->Length - sizeof(ACPI::SDTHeader)) / 8;
        for(int t = 0; t < entries; t++){
            uint64_t entryAddress = *(uint64_t*)((uint64_t)sdtHeader + sizeof(SDTHeader) + (t * sizeof(uint64_t)));
            void* vaddr = (void*)physical_to_virtual(entryAddress);
            void* physical = (void*)entryAddress;
            globalPTM.MapMemory(vaddr, physical);
            
            ACPI::SDTHeader* newSDTHeader = (ACPI::SDTHeader*)vaddr;
            for (int i=0;i<4;i++){
                if (newSDTHeader->Signature[i] != signature[i]){
                    break;
                }
                
                if (i == 3) {
                    return vaddr;
                }
            }
        }
        return nullptr;
    }


    void _acpi_set_apic_mode() {
        ACPI_OBJECT_LIST arg_list;
        ACPI_OBJECT arg[1];

        arg_list.Count = 1;
        arg_list.Pointer = arg;
        arg[0].Type = ACPI_TYPE_INTEGER;
        arg[0].Integer.Value = 1;

        ACPI_STATUS status = AcpiEvaluateObject(ACPI_ROOT_OBJECT, (ACPI_STRING)"_PIC", &arg_list, NULL);
        
        if (ACPI_SUCCESS(status)) {
            kprintf("[ACPI] Switched to IOAPIC mode via _PIC\n");
        } else {
            kprintf("[ACPI] _PIC method not found or failed: %s\n", AcpiFormatException(status));
        }
    }
    
    void InitializeACPICA() {
        ACPI_STATUS status;

        kprintf("Initializing ACPI...\n");

        status = AcpiInitializeSubsystem();
        if (ACPI_FAILURE(status)) {
            kprintf("AcpiInitializeSubsystem failed: %s\n", AcpiFormatException(status));
            return;
        }

        status = AcpiInitializeTables(NULL, 16, FALSE);

        if (ACPI_FAILURE(status)) {
            kprintf("AcpiInitializeTables failed: %s\n", AcpiFormatException(status));
            return;
        }

        status = AcpiLoadTables();

        if (ACPI_FAILURE(status)) {
            kprintf("AcpiLoadTables failed: %s\n", AcpiFormatException(status));
            return;
        }

        
        status = acpi_intialize_embedded_controller();

        if (ACPI_FAILURE(status)) {
            kprintf("Failed to install EC handler: %s\n", AcpiFormatException(status));
        }
        
        status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

        if (ACPI_FAILURE(status)) {
            kprintf("AcpiEnableSubsystem failed: %s\n", AcpiFormatException(status));
        }

        status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);

        if (ACPI_FAILURE(status)) {
            kprintf("AcpiInitializeObjects failed: %s\n", AcpiFormatException(status));
        }

        _acpi_set_apic_mode();

        embedded_controler_enable_gpes();

        setup_acpi_kernel_events();

        AcpiUpdateAllGpes();
        
        kprintf("ACPICA Initialized Successfully!\n");
        
        return;
    }

}


