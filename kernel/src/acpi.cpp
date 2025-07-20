#include <acpi.h>

ACPI::SDTHeader* xsdt;
ACPI::RSDP2* rsdp;
ACPI::MCFGHeader* mcfg;

namespace ACPI{
    void* FindTable(SDTHeader* sdtHeader, char* signature){
        int entries = (sdtHeader->Length - sizeof(ACPI::SDTHeader)) / 8;
        for(int t = 0; t < entries; t++){
            uint64_t entryAddress = *(uint64_t*)((uint64_t)sdtHeader + sizeof(SDTHeader) + (t * sizeof(uint64_t)));
            ACPI::SDTHeader* newSDTHeader = (ACPI::SDTHeader*)entryAddress;
            for (int i=0;i<4;i++){
                if (newSDTHeader->Signature[i] != signature[i]){
                    break;
                }
                if (i == 3) return (void*)entryAddress;
            }
        }
        return nullptr;
    }
}


