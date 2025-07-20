#define UACPI_FORMATTED_LOGGING

#include <uacpi/kernel_api.h>
#include <memory/heap.h>
#include <acpi.h>
#include <BasicRenderer.h>
#include <paging/PageTableManager.h>
#include <scheduling/apic/apic.h>
#include <scheduling/task/scheduler.h>

extern "C" uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address){
    *out_rsdp_address = (uacpi_phys_addr)rsdp;
    return UACPI_STATUS_OK;
}

extern "C" void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len){
    return (void*)addr;
}

extern "C" void uacpi_kernel_unmap(void *addr, uacpi_size len){
    return;
}

extern "C" void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* str, ...){
    va_list args;
    va_start(args, str);
    
    uint32_t clr = 0xFFFFFF;

    switch (level){
        case UACPI_LOG_ERROR:
            clr = 0xFF0000;
            break;
        case UACPI_LOG_WARN:
            clr = 0xFFD60A;
            break;
        case UACPI_LOG_INFO:
            clr = 0x00FF00;
            break;
        case UACPI_LOG_TRACE:
            clr = 0xFFF000;
            break;
        case UACPI_LOG_DEBUG:
            clr = 0xF00F80;
            break;
    }

    kprintf(clr, "[uACPI] ");

    globalRenderer->printfva(0xFFFFFF, str, args);
    va_end(args);
}
extern "C" void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char* str, uacpi_va_list args){
    uint32_t clr = 0xFFFFFF;

    switch (level){
        case UACPI_LOG_ERROR:
            clr = 0xFF0000;
            break;
        case UACPI_LOG_WARN:
            clr = 0xFFD60A;
            break;
        case UACPI_LOG_INFO:
            clr = 0x00FF00;
            break;
        case UACPI_LOG_TRACE:
            clr = 0xFFF000;
            break;
        case UACPI_LOG_DEBUG:
            clr = 0xF00F80;
            break;
    }

    kprintf(clr, "[uACPI] ");

    globalRenderer->printfva(0xFFFFFF, str, args);
}

extern "C" uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle){
    uint64_t addr = 0;

    int entries = ((mcfg->Header.Length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);
    
    if (address.segment > entries) {
        kprintf(0xFFF000, "Couldnt find device segment: %d (%d), bus: %d, device: %d, function: %d\n", address.segment, entries, address.bus, address.device, address.function);
        return UACPI_STATUS_NOT_FOUND;
    };

    ACPI::DeviceConfig* seg = (ACPI::DeviceConfig*)((uint64_t)mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * address.segment));

    addr = seg->BaseAddress;

    uint64_t offset = (address.bus << 20) + (address.device << 15) + (address.function << 12);
    addr += offset;
    globalPTM.MapMemory((void*)addr, (void*)addr);

    *out_handle = (uacpi_handle)addr;

    return UACPI_STATUS_OK;
}

extern "C" void uacpi_kernel_pci_device_close(uacpi_handle){
    return;
}

extern "C" uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *value = *(uint8_t*)address;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *value = *(uint16_t*)address;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *value = *(uint32_t*)address;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *(uint8_t*)address = value;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *(uint16_t*)address = value;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value){
    uint64_t address = (uint64_t)device;
    address += offset;

    *(uint32_t*)address = value;
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle){
    *out_handle = (uacpi_handle)base;
    return UACPI_STATUS_OK;
}

extern "C" void uacpi_kernel_io_unmap(uacpi_handle handle){
    return;
}

extern "C" uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value){
    *out_value = inb((uint16_t)(uint32_t)(uint64_t)handle + offset);
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value){
    *out_value = inw((uint16_t)(uint32_t)(uint64_t)handle + offset);
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value){
    *out_value = ind((uint16_t)(uint32_t)(uint64_t)handle + offset);
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value){
    outb((uint16_t)(uint32_t)(uint64_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value){
    outw((uint16_t)(uint32_t)(uint64_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value){
    outd((uint16_t)(uint32_t)(uint64_t)handle + offset, in_value);
    return UACPI_STATUS_OK;
}

extern "C" void *uacpi_kernel_alloc(uacpi_size size){
    return malloc(size);
}

extern "C" void uacpi_kernel_free(void *mem){
    free(mem);
}

extern "C" uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void){
    return APICticsSinceBoot * 1000 * 1000;
}

extern "C" void uacpi_kernel_stall(uacpi_u8 usec){
    Sleep(1);
}

extern "C" void uacpi_kernel_sleep(uacpi_u64 msec){
    Sleep(msec);
}

extern "C" uacpi_handle uacpi_kernel_create_mutex(void){
    return (uacpi_handle)10;
}

extern "C" void uacpi_kernel_free_mutex(uacpi_handle){

}

extern "C" uacpi_handle uacpi_kernel_create_event(void){
    return (uacpi_handle)10;
}

extern "C" void uacpi_kernel_free_event(uacpi_handle){
    return;
}

extern "C" uacpi_thread_id uacpi_kernel_get_thread_id(void){
    /*if (taskScheduler::currentTask != nullptr){
        return (uacpi_thread_id)taskScheduler::currentTask->pid;
    }*/

    return (uacpi_thread_id)1;
}


extern "C" uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle, uacpi_u16){
    return UACPI_STATUS_OK;
}

extern "C" void uacpi_kernel_release_mutex(uacpi_handle){
    return;
}

extern "C" uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16){
    return true;
}

extern "C" void uacpi_kernel_signal_event(uacpi_handle){
    return;
}

extern "C" void uacpi_kernel_reset_event(uacpi_handle){
    return;
}

extern "C" uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*){
    return UACPI_STATUS_OK;
}

int Vector = 0x40;

extern "C" uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle *out_irq_handle){
    SetIrq(irq, Vector, 0);
    uacpi_kernel_log(UACPI_LOG_DEBUG, "installed interrupt handler for irq %d at vector %d\n", irq, Vector);
    addIsrWithContext(Vector, (void*)handler, ctx);
    *out_irq_handle = (uacpi_handle)handler;

    Vector++;

    return UACPI_STATUS_OK;
}

extern "C" uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler, uacpi_handle irq_handle){
    removeISR(irq_handle);

    return UACPI_STATUS_OK;
}

extern "C" uacpi_handle uacpi_kernel_create_spinlock(void){
    return (uacpi_handle)10;
}

extern "C" void uacpi_kernel_free_spinlock(uacpi_handle){

}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle){
    return (uacpi_cpu_flags)4;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle, uacpi_cpu_flags){

}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler, uacpi_handle ctx){
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void){
    return UACPI_STATUS_OK;
}