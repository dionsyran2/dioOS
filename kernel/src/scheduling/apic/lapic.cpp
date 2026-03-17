#include <scheduling/apic/lapic.h>
#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <interrupts/interrupts.h>
#include <gdt/gdt.h>
#include <kstdio.h>
#include <memory.h>
#include <cpu.h>
#include <local.h>
#include <drivers/timers/common.h>

int local_cpu_cnt = 0;
local_apic_t* local_apic_list = nullptr;

void add_to_local_apic_list(local_apic_t* local_apic){
    if (local_apic_list == nullptr) {
        local_apic_list = local_apic;
        return;
    }

    local_apic_t* c = local_apic_list;
    while (c->next) c = c->next;
    c->next = local_apic;
}
bool is_apic_supported(){
    uint32_t edx = 0;
    asm volatile (
        "mov $1, %%eax\n"
        "cpuid\n"
        : "=c"(edx)
        : 
        : "eax", "ebx", "edx"
    );

    return edx & (1 << 9);
}


void cpu_set_apic_base(uint64_t apic) {
   uint64_t write = apic | IA32_APIC_BASE_MSR_ENABLE;
   write_msr(IA32_APIC_BASE_MSR, write);
}


uintptr_t cpu_get_apic_base() {
   uint64_t ret = read_msr(IA32_APIC_BASE_MSR);
   return ret & ~0xFFFUL;
}

local_apic_t* init_local_apic(){

    if (!is_apic_supported()){
        kprintf("\e[0;31m[LAPIC]\e[0m APIC not supported by the cpu!\n");
        return nullptr;
    }

    /* Hardware enable the Local APIC if it wasn't enabled */
    uint64_t base = cpu_get_apic_base();
    uint64_t virtual_base = physical_to_virtual(base);
    globalPTM.MapMemory((void*)virtual_base, (void*)base);
    cpu_set_apic_base(base);


    local_apic_t* lapic = new local_apic_t;

    lapic->address = virtual_base;
    lapic->next = nullptr;

    uint64_t id_reg = lapic->read_register(LAPIC_ID_REG);
    lapic->lapic_id = (id_reg >> 24) & 0xFF;
    lapic->id = local_cpu_cnt;
    local_cpu_cnt++;

    /* Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts */
    lapic->write_register(0xF0, SPURIOUS_INTERRUPT_VECTOR | 0x100);

    // Set the TPR
    lapic->write_register(0x080, 0);

    add_to_local_apic_list(lapic);
    return lapic;
}

void local_apic_t::write_register(uint32_t reg, uint32_t value){
    *(volatile uint32_t*)(this->address + reg) = value;
}

uint32_t local_apic_t::read_register(uint32_t reg){
    return *(volatile uint32_t*)(this->address + reg);
}

void local_apic_t::EOI(){
    write_register(LAPIC_EOI_REG, 0);
}

void init_core(){
    local_apic_t* lapic = init_local_apic();

    cpu_local_data* local_data = (cpu_local_data*)GlobalAllocator.RequestPage();
    memset(local_data, 0, sizeof(cpu_local_data));
    local_data->self_reference = local_data;
    local_data->lapic = lapic;
    local_data->cpu_id = lapic->id;
    local_data->apic_timer_initialized = false;
    local_data->disable_scheduling = true;
    local_data->next = nullptr;

    uint64_t rflags = spin_lock(&_cpu_local_data_chain_lock);

    if (_last_local_data_entry == nullptr){
        _last_local_data_entry = local_data;
    } else {
        _last_local_data_entry->next = local_data;
        _last_local_data_entry = local_data;
    }

    spin_unlock(&_cpu_local_data_chain_lock, rflags);
    
    void* idt = GlobalAllocator.RequestPages(DIV_ROUND_UP(sizeof(interrupt_descriptor_table_t), PAGE_SIZE));
    local_data->interrupt_descriptor_table = (interrupt_descriptor_table_t*)idt;

    // Initialize it (Since we use RequestPage we cannot call the constructor)
    local_data->interrupt_descriptor_table->address = (uint64_t)local_data->interrupt_descriptor_table->interrupt_table;
    local_data->interrupt_descriptor_table->limit = 0xFFF;
    
    memset(local_data->interrupt_descriptor_table->interrupt_table, 0, sizeof(local_data->interrupt_descriptor_table->interrupt_table));

    write_msr(IA32_GS_BASE, (uint64_t)local_data);

    setup_gdt();
    
    local_data->interrupt_descriptor_table->load();

    //kprintf("\e[0;32m[CORE]\e[0m Initialized core #%d successfully.\n", local_data->cpu_id);
}

void wait_delivery() {
    cpu_local_data* data = get_cpu_local_data();
    uint32_t low;
    uint32_t cnt = 0;

    do {
        low = data->lapic->read_register(ICR_LOW);
        
        // Check Delivery Status (Bit 12)
        if ((low & IPI_DELIVERY_STATUS_PENDING) == 0) {
            break;
        }

        cnt++;
        nanosleep(10); 
    } while(cnt < 1000); 
}

void send_sipi(uint16_t apic_id, uint32_t address) {
    cpu_local_data* data = get_cpu_local_data();

    uint8_t vector = (address / PAGE_SIZE) & 0xFF;

    wait_delivery();

    data->lapic->write_register(ICR_HIGH, ((uint32_t)apic_id) << 24);

    // Intel SDM Vol 3A: SIPI requires Level bit to be 1 (Assert).
    uint32_t low = IPI_DELIVERY_MODE_SIPI | IPI_LEVEL_ASSERT | vector;
    
    data->lapic->write_register(ICR_LOW, low);

    wait_delivery();

    Sleep(1); 
}

void send_init_ipi(uint16_t apic_id) {
    cpu_local_data* data = get_cpu_local_data();

    wait_delivery();

    data->lapic->write_register(ICR_HIGH, (uint32_t)apic_id << 24);
    
    // Standard INIT (Reset): Assert (1) and Edge Trigger (0)
    uint32_t low = IPI_DELIVERY_MODE_INIT | IPI_LEVEL_ASSERT | IPI_TRIGGER_MODE_EDGE;

    data->lapic->write_register(ICR_LOW, low);
    
    wait_delivery(); 
}

void send_ipi(uint8_t vector, uint16_t destination, ipi_destination_type_t type) {
    uint64_t gs = read_msr(IA32_GS_BASE);
    if (gs == 0) return; // Don't go in an infinite exception loop
    
    cpu_local_data* data = get_cpu_local_data();

    wait_delivery();

    data->lapic->write_register(ICR_HIGH, ((uint32_t)destination) << 24);
    
    uint32_t low = (type << 18) | 
                   IPI_DESTINATION_MODE_PHYSICAL | 
                   IPI_LEVEL_ASSERT |
                   IPI_DELIVERY_MODE_FIXED | 
                   vector;
                   
    data->lapic->write_register(ICR_LOW, low);
    
    wait_delivery();
}