#include "apic.h"

volatile uint32_t* io_apic;
uint64_t LAPICAddress;
uint64_t APICticsSinceBoot;
uint64_t Ticks = 0;

uint8_t TIME_DAY;
uint8_t TIME_MONTH;
uint16_t TIME_YEAR;
uint8_t TIME_HOUR;
uint8_t TIME_MINUTES;
uint8_t TIME_SECONDS;


int is_leap_year(int year) {
    // Leap year calculation: divisible by 4, but not by 100 unless divisible by 400
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        return 1; // Leap year
    }
    return 0; // Not a leap year
}

int get_days_in_month(int month, int year) {
    // Array for days in months: Jan, Mar, May, Jul, Aug, Oct, Dec (31 days), and Apr, Jun, Sep, Nov (30 days)
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Check if the month is February and if it's a leap year
    if (month == 2 && is_leap_year(year)) {
        return 29; // February has 29 days in a leap year
    }
    
    return days_in_month[month - 1]; // Return the number of days in the month
}

uint64_t to_unix_timestamp(int sec, int min, int hour, int day, int month, int year) {
    // Year must be full (e.g., 2025)
    int y = year;
    int m = month;
    int d = day;

    // Days since epoch
    uint64_t days = 0;

    // Add days for each year
    for (int i = 1970; i < y; ++i)
        days += is_leap_year(i) ? 366 : 365;

    // Add days for each month
    for (int i = 1; i < m; ++i) {
        days += get_days_in_month(i - 1, year);
        if (i == 2 && is_leap_year(y))
            days += 1; // February in leap year
    }

    days += d - 1; // Current month days

    // Convert to seconds
    return ((days * 24 + hour) * 60 + min) * 60 + sec;
}
RTC::rtc_time_t* c_time;
void update_time(){
    //need to fix the lapic timer calibration so it doesnt drift off
    RTC::rtc_time_t time = RTC::read_rtc_time();
    if (c_time != nullptr){
        memcpy(c_time, &time, sizeof(RTC::rtc_time_t));
    }
    
    return;
    TIME_SECONDS++;
    if (TIME_SECONDS > 59){
        TIME_SECONDS = 0;
        TIME_MINUTES++;
    }

    if (TIME_MINUTES > 59){
        TIME_MINUTES = 0;
        TIME_HOUR++;
    }

    if (TIME_HOUR > 23){
        TIME_HOUR = 0;
        TIME_DAY++;
    }

    if (TIME_DAY > get_days_in_month(TIME_MONTH - 1, TIME_YEAR)){
        TIME_DAY = 1;
        TIME_MONTH++;
    }

    if (TIME_MONTH > 12){
        TIME_MONTH = 1;
        TIME_YEAR++;
    }
}


inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile ("wrmsr" : : "a"(low), "d"(high), "c"(msr));
}


void IOApicWrite(uint8_t reg, uint32_t value){
    io_apic[IOREGSEL / sizeof(uint32_t)] = reg;
    io_apic[IOWIN / sizeof(uint32_t)] = value;
}

uint32_t IOApicRead(uint8_t reg){
    io_apic[IOREGSEL / sizeof(uint32_t)] = reg;
    return io_apic[IOWIN / sizeof(uint32_t)];
}
static inline void write_apic_register(uint32_t offset, uint32_t value) {
    volatile uint32_t* apic_base = (volatile uint32_t*)(read_msr(IA32_APIC_BASE) & ~0xFFF);
    apic_base[offset / 4] = value;
}

static inline uint32_t read_apic_register(uint32_t offset) {
    volatile uint32_t* apic_base = (volatile uint32_t*)(read_msr(IA32_APIC_BASE) & ~0xFFF);
    return apic_base[offset / 4];
}

void SetIrq(uint8_t irq, uint8_t vector, uint8_t mask){
    uint32_t low = 0;
    low |= vector; // Vector
    low |= 0 << 8; // Delivery Mode
    low |= 0 << 11; // Destination Mode
    low |= 0 << 12; // Delivery Status
    low |= 0 << 13; // Pin Polarity
    low |= 0 << 14; // IRR
    low |= 0 << 15; // Trigger Mode
    low |= mask << 16; // Mask
    uint32_t high = mask;
    high |= 0 << 24; // Destination

    IOApicWrite(IOAPIC_REDIRECTION_BASE + (irq * 2), low);
    IOApicWrite(IOAPIC_REDIRECTION_BASE + (irq * 2) + 1, high);
}
void InitIOAPIC(){
    void* entryAddress = (void*)madt->FindIOApic();
    IOAPIC* IOApic = (IOAPIC*)((uint64_t)entryAddress + sizeof(Entry));
    uint32_t apicAddress = IOApic->APICAddress; // See specifications or link in apic.h
    globalRenderer->printf("APIC GSI: %d\n", IOApic->GSIBase);
    globalPTM.MapMemory((void*)apicAddress, (void*)apicAddress);

    io_apic = (uint32_t*)apicAddress;

    uint32_t version = IOApicRead(0x01);
    uint8_t ioApicVersion = version & 0xFF;
    uint8_t maxRedirectionEntry = (version >> 16) & 0xFF;
    

    SetIrq(12, 0x2C, 0); // mouse

    uint8_t REDIR_HPET = 0;
    //Loop through ISO
    ISOEntryTable ISO = madt->FindAPICISO();
    for (uint i = 0; i < ISO.count; i++){
        void* ISOentry = ISO.entries[i];
        APICISO* iso = (APICISO*)((uint64_t)ISOentry + sizeof(Entry));
        globalRenderer->printf("REDIR: GSI: %d, BUS: %d, IRQ: %d cnt %d\n", iso->GSI, iso->BusSource, iso->IRQSource, ISO.count);
        if (iso->GSI > maxRedirectionEntry) {
            globalRenderer->printf("Skipping GSI %d as it exceeds max entries.\n", iso->GSI);
            continue;
        }

    }


}

void InitLAPIC() {
    // Disable the PICs
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    uint32_t* apic_base = (uint32_t*)(read_msr(IA32_APIC_BASE) & ~0xFFF);
    LAPICAddress = (uint64_t)apic_base;
    globalRenderer->printf("APIC ADDR %llx\n", (uint64_t)apic_base);
    globalPTM.MapMemory((void*)apic_base, (void*)apic_base);
    write_apic_register(0xF0, 0xFF | (1 << 8));
    write_apic_register(0x80, 0x00);
    write_apic_register(0xE0, 0xffffffff);
    write_apic_register(0xD0, 0x01000000);
    write_apic_register(0x370, 0x1F);
}

void SetupAPICTimer(){
    //TIMER    
    write_apic_register(0x3E0, 0x3); // Divider
    write_apic_register(0x320, 0x23 | (1 << 17) | (0 << 16));
    write_apic_register(0x380, 0xFFFFFFFF); // Initial count

    HPET::Sleep(1);
    uint32_t ticksIn1ms = 0xFFFFFFFF - read_apic_register(0x390); //read ticks
    write_apic_register(0x320, 0x23 | (1 << 17) | (1 << 16)); //mask
    write_apic_register(0x380, ticksIn1ms); //Set the initial tick so it ticks every 10ms
    write_apic_register(0x320, 0x23 | (1 << 17) | (0 << 16)); //unmask
    globalRenderer->printf("TICKS: %d\n", ticksIn1ms);
}

void SendIPI(uint8_t vector, uint16_t APICID){
    write_apic_register(ICR_HIGH, ((uint32_t)APICID) << 24);
    uint32_t low = 0;
    low |= (uint32_t)vector;
    low |= 0b000 << 8;
    low |= 0 << 11;
    low |= 1 << 12;
    low |= 1 << 14;
    write_apic_register(ICR_LOW, low);
}

void wait_delivery(){
    uint32_t low;
    uint16_t cnt = 0; 
    do{
        low = read_apic_register(ICR_LOW);
        cnt++;
    }while(((low & (1 << 12)) != 0) && cnt > 8888888);
}

volatile uint8_t started = 0;
void SendStartupIPI(uint16_t APICID) {
    uint32_t apCodeBase = (uint32_t)(uint64_t)lowestMemLoc;
    uint8_t startPage = ((uint64_t)apCodeBase) / 0x1000;
    globalRenderer->printf("\nStart Page: %d", startPage);
    
    write_apic_register(ICR_HIGH, ((uint32_t)APICID) << 24);
    uint32_t low = 0;
    low |= 0b110 << 8;  // Delivery mode: Startup
    low |= 1 << 14;
    low |= startPage;

    write_apic_register(ICR_LOW, 0x00005610);

    wait_delivery();
    Sleep(100);
    write_apic_register(ICR_LOW, 0x00005610);  // Send second Startup IPI
    globalRenderer->printf("\nStarted->%d\nAPIC ID->%d\nEntry->0x%llx", started, APICID, (uint64_t)apCodeBase);
}

void SendInitIPI(uint16_t APICID){
    write_apic_register(ICR_HIGH, (uint32_t)APICID << 24);
    volatile uint32_t low = 0;
    low |= 0b101 << 8;
    low |= 1 << 13;
    low |= 1 << 14;
    write_apic_register(ICR_LOW, 0x00005500);
    globalRenderer->printf("\nlow->0x%h", low);
    globalRenderer->delay(1000);
    wait_delivery();
    write_apic_register(ICR_HIGH, (uint32_t)APICID << 24);
    low |= 0 << 13;
    low |= 0 << 14;
    write_apic_register(ICR_LOW, 0x00009500);
}


void EOI(){
    write_apic_register(0xB0, 0);
}


void Sleep(uint32_t ms){
    uint64_t initialTicks = Ticks;
    
    while (Ticks < (initialTicks + ms));
}

uint64_t GetAPICTick(){
    return Ticks;
}

void TickAPIC(){
    APICticsSinceBoot++;
    Ticks++;
    if (Ticks % 1000 == 0){
        update_time(); // update every 1 second
    }
}











