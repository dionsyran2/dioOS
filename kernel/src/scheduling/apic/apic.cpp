#include "apic.h"
#include "../pit/pit.h"

volatile uint32_t* apic = (uint32_t*)APIC_BASE_ADDRESS;


void InitLocalAPIC() {

    apic[ICR_LOW_OFFSET / 4] = 0x00000000;

    apic[APIC_SPURIOUS_INT_VECTOR / 4] = 0x000000FF | (1 << 8); // Enable APIC and set spurious vector
}


void SendIPI(uint8_t vector, uint8_t destinationAPICID) {

    while (apic[ICR_LOW_OFFSET / 4] & (1 << 12)) { }


    apic[ICR_HIGH_OFFSET / 4] = (destinationAPICID << 24);

    apic[ICR_LOW_OFFSET / 4] = vector | (1 << 14) | (0 << 8);

    while (apic[ICR_LOW_OFFSET / 4] & (1 << 12)) { }
}

void InitAllCPUs() {
    CPU cpu_info = get_cpu_info(); // Function to retrieve CPU info
    BootSecondaryCPUs();
    for (int apicID = 1; apicID < cpu_info.coresPerPackage; apicID++) {
        SendIPI(0x32, apicID);
    }
}


void SendEndOfInterrupt() {
    if (apic) {
        apic[0xB0 / sizeof(uint32_t)] = 0;
    }
}