#include <acpica/platform/dioos.h>
#include <scheduling/apic/ioapic.h>
#include <drivers/timers/common.h>
#include <paging/PageTableManager.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/spinlock/spinlock.h>
#include <scheduling/mutex/mutex.h>
#include <scheduling/semaphore/semaphore.h>
#include <interrupts/interrupts.h>
#include <memory/heap.h>
#include <memory.h>
#include <acpi.h>
#include <kstdio.h>
#include <stdarg.h>
#include <IO.h>


extern "C" {
#include <acpica/acpi.h>
}
extern "C" {

ACPI_STATUS AcpiOsInitialize(){
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(){
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(){
    return (ACPI_PHYSICAL_ADDRESS)virtual_to_physical((UINT64)rsdp);
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue){
    *NewValue = nullptr;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *PredefinedObject, ACPI_TABLE_HEADER **NewValue){
    *NewValue = nullptr;
    return AE_OK;
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length){
    UINT64 offset = PhysicalAddress % PAGE_SIZE;

    uint64_t phys = PhysicalAddress & ((uint64_t)~(PAGE_SIZE - 1));
    uint64_t len = DIV_ROUND_UP(Length, PAGE_SIZE);

    UINT64 virt = physical_to_virtual(phys);

    for (ACPI_SIZE i = 0; i < len; i += 0x1000){
        globalPTM.MapMemory((void*)(virt + i), (void*)(phys + i));
    }

    //kprintf("MAPPED %p to %p (%x)\n", phys, virt, len);
    return (void *)(virt + offset);
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length){
    return;
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress){
    if (LogicalAddress == nullptr || PhysicalAddress == nullptr) return AE_BAD_PARAMETER;
    *PhysicalAddress = globalPTM.getPhysicalAddress(LogicalAddress);

    return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size){
    return malloc(Size);
}

void AcpiOsFree(void *Memory){
    free(Memory);
}

BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length){
    Length = DIV_ROUND_UP(Length, PAGE_SIZE);
    UINT64 mem = (UINT64)Memory & ~(PAGE_SIZE - 1);
    for (ACPI_SIZE i = 0; i < Length; i += 0x1000){
        if (globalPTM.getPhysicalAddress((void*)(mem + i)) == 0) return false;
    }
    return true;
}

BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length){
    Length = DIV_ROUND_UP(Length, PAGE_SIZE);
    UINT64 mem = (UINT64)Memory & ~(PAGE_SIZE - 1);
    for (ACPI_SIZE i = 0; i < Length; i += 0x1000){
        if (globalPTM.GetFlag((void*)(mem + i), PT_Flag::Write) == false) return false;
    }
    return true;
}

ACPI_THREAD_ID AcpiOsGetThreadId(){
    task_t* task = task_scheduler::get_current_task();
    if (task == nullptr) return -1;
    
    return task->pid;
}

void AcpiOsExecuteTaskWrapper(ACPI_OSD_EXEC_CALLBACK Function, void *Context){
    task_t* self = task_scheduler::get_current_task();
    Function(Context);
    self->exit(0);

    while(1) __asm__("pause"); // Safety
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context){
    task_t* task = task_scheduler::create_process("ACPICA Thread", (function)AcpiOsExecuteTaskWrapper, false, false);
    task->registers.rdi = (UINT64)Function;
    task->registers.rsi = (UINT64)Context;
    task_scheduler::mark_ready(task);
    return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds){
    task_t* task = task_scheduler::get_current_task();
    if (task){
        task->ScheduleFor(Milliseconds, BLOCKED);
    }else{
        Sleep(Milliseconds);
    }
}

void AcpiOsStall(UINT32 Microseconds){
    UINT64 ns = Microseconds * 1000;

    nanosleep(ns);
}

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle){
    mutex_t* mutex = new mutex_t;
    mutex->count = 0;
    mutex->spinlock = 0;

    //kprintf("Create Mutex! %p\n", mutex);
    *OutHandle = mutex;
    return AE_OK;
}

void AcpiOsDeleteMutex(void *Handle){
    delete (mutex_t*)Handle;
}

ACPI_STATUS AcpiOsAcquireMutex(void *Handle, uint16_t Timeout){
    mutex_t* mutex = (mutex_t*)Handle;
    mutex->spinlock = 0;
    mutex->count = 0;
    mutex->owner_pid = -1;
    if (!mutex->lock(Timeout == -1 ? (UINT64)-1 : Timeout)) return AE_TIME;

    return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle){
    //kprintf("Release Mutex! %p\n", Handle);

    mutex_t* mutex = (mutex_t*)Handle;
    mutex->unlock();
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, void **OutHandle){
    semaphore_t* sem = new semaphore_t;
    sem->lock = 0;
    sem->count = InitialUnits;
    *OutHandle = sem;
    //kprintf("Create Semaphore %d, %p\n", InitialUnits, sem);

    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(void *Handle){
    delete (semaphore_t*)Handle;
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(void *Handle, UINT32 Units, uint16_t Timeout){
    //kprintf("Sem Wait! %p\n", Handle);
    semaphore_t* sem = (semaphore_t*)Handle;

    for (int i = 0; i < Units; i++ ) if (!sem->wait(Timeout)) return AE_TIME;
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(void *Handle, UINT32 Units){
    //kprintf("Sig Sem! %p\n", Handle);

    semaphore_t* sem = (semaphore_t*)Handle;

    for (int i = 0; i < Units; i++ ) sem->post();
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle){
    spinlock_t* lock = new spinlock_t;
    *lock = 0;
    //kprintf("Create Spinlock! %p\n", lock);

    *OutHandle = (ACPI_SPINLOCK)lock;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_HANDLE Handle){
    delete (spinlock_t*)Handle;
}

UINT64 AcpiOsAcquireLock(ACPI_SPINLOCK Handle){
    //kprintf("spinlock! %p\n", Handle);

    return spin_lock((spinlock_t*)Handle);
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, UINT64 Flags){
    //kprintf("Spinunlock! %p\n", Handle);
    
    return spin_unlock((spinlock_t*)Handle, Flags);
}


ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context){
    UINT8 vector = FreeVector;
    FreeVector++;


    _add_dynamic_isr(vector, (dynamic_isr_handler_t)Handler, Context);

    set_apic_irq(InterruptLevel, vector, false, true, true);

    //kprintf("Installing interrupt Handler (Vector: %d, irq: %d)\n", vector, InterruptLevel);
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler){
    // TODO
    return AE_OK;
}

UINT64 AcpiOsGetTimer(){
    UINT64 ms = GetTicks() * GetTicksPerMS();
    
    // It needs in 100 nanosecond units
    UINT64 ns = ms * 10;
    ns += TSC::get_time_ns();

    return ns;
}

UINT32 AcpiOsSignal (UINT32 Function, void *Info){
    if (Function == 0 /*ACPI_SIGNAL_FATAL*/){
        kprintf("\e[0;31m[ACPICA Fatal]\e[0m");
    } else { /* ACPI_SIGNAL_BREAKPOINT */
        kprintf("\e[0;35m[ACPICA breakpoint]\e[0m");
    }

    kprintf("%s\n", Info);

    return AE_OK;
}

void* acpica_memcpy(void* dest, const void* src, size_t n){
    return memcpy(dest, src, n);
}

void AcpiOsVprintf(const char *Format, va_list Args) {
    kprintfva(Format, Args);
}

void AcpiOsPrintf(const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);

    AcpiOsVprintf(Format, Args);

    va_end(Args);
}



void AcpiOsWaitEventsComplete(void) {
    return;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Register, UINT64 *Value, UINT32 Width){
    // Bit 31: Enable Bit (1)
    // Bits 23-16: Bus Number
    // Bits 15-11: Device Number
    // Bits 10-8: Function Number
    // Bits 7-2: Register ID (Aligned to 4 bytes)
    UINT32 address = 0x80000000 
                     | ((UINT32)PciId->Bus << 16) 
                     | ((UINT32)PciId->Device << 11) 
                     | ((UINT32)PciId->Function << 8) 
                     | (Register & 0xFC);

    // Write Address to 0xCF8
    outd(0xCF8, address);

    // Read Data from 0xCFC
    // We add (Register & 3) to the port to handle offset access (e.g. reading 1 byte at offset 1)
    switch (Width) {
        case 8:{
            *Value = inb(0xCFC + (Register & 3));
            break;
        }
        case 16:{
            *Value = inw(0xCFC + (Register & 3));
            break;
        }
        case 32:{
            *Value = ind(0xCFC);
            break;
        }
        case 64:{
            // 64-bit read requires two 32-bit reads
            UINT32 low = ind(0xCFC);
            
            // Prepare for high dword
            outd(0xCF8, address + 4);
            UINT32 high = ind(0xCFC);
            
            *Value = (UINT64)low | ((UINT64)high << 32);
            break;
        }
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Register, UINT64 Value, UINT32 Width) {
    UINT32 address = 0x80000000 
                     | ((UINT32)PciId->Bus << 16) 
                     | ((UINT32)PciId->Device << 11) 
                     | ((UINT32)PciId->Function << 8) 
                     | (Register & 0xFC);

    outd(0xCF8, address);

    switch (Width) {
        case 8:{
            outb(0xCFC + (Register & 3), (UINT8)Value);
            break;
        }
        case 16:{
            outw(0xCFC + (Register & 3), (uint16_t)Value);
            break;
        }
        case 32:{
            outd(0xCFC, (UINT32)Value);
            break;
        }
        case 64:{
            outd(0xCFC, (UINT32)Value);
            outd(0xCF8, address + 4);
            outd(0xCFC, (UINT32)(Value >> 32));
            break;
        }
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegAValue, UINT32 RegBValue) {
    
    // 1. Get the I/O port addresses from the global FADT
    // ACPICA parses the FADT automatically, we just read the struct.
    ACPI_GENERIC_ADDRESS *Pm1aControl = &AcpiGbl_FADT.XPm1aControlBlock;
    ACPI_GENERIC_ADDRESS *Pm1bControl = &AcpiGbl_FADT.XPm1bControlBlock;

    // 2. Write to PM1A Control Register (Almost always exists)
    // Note: PM1 control registers are 16-bit.
    if (Pm1aControl->Address) {
        outw((uint16_t)Pm1aControl->Address, (uint16_t)RegAValue);
    }

    // 3. Write to PM1B Control Register (Optional, often 0 on modern boards)
    if (Pm1bControl->Address) {
        outw((uint16_t)Pm1bControl->Address, (uint16_t)RegBValue);
    }

    // 4. Halt the CPU
    // If we are shutting down (S5), the power usually cuts immediately after the writes above.
    // If it takes a few milliseconds, we don't want the CPU executing garbage.
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    void* virtual_ptr = AcpiOsMapMemory(Address, Width / 8);
    
    if (!virtual_ptr) {
        return AE_NO_MEMORY;
    }

    switch (Width) {
        case 8:
            *Value = *(volatile uint8_t*)virtual_ptr;
            break;
        case 16:
            *Value = *(volatile uint16_t*)virtual_ptr;
            break;
        case 32:
            *Value = *(volatile uint32_t*)virtual_ptr;
            break;
        case 64:
            *Value = *(volatile uint64_t*)virtual_ptr;
            break;
        default:
            AcpiOsUnmapMemory(virtual_ptr, Width / 8);
            return AE_BAD_PARAMETER;
    }

    AcpiOsUnmapMemory(virtual_ptr, Width / 8);

    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    void* virtual_ptr = AcpiOsMapMemory(Address, Width / 8);
    
    if (!virtual_ptr) return AE_NO_MEMORY;

    switch (Width) {
        case 8:
            *(volatile uint8_t*)virtual_ptr = (uint8_t)Value;
            break;
        case 16:
            *(volatile uint16_t*)virtual_ptr = (uint16_t)Value;
            break;
        case 32:
            *(volatile uint32_t*)virtual_ptr = (uint32_t)Value;
            break;
        case 64:
            *(volatile uint64_t*)virtual_ptr = (uint64_t)Value;
            break;
        default:
            AcpiOsUnmapMemory(virtual_ptr, Width / 8);
            return AE_BAD_PARAMETER;
    }

    AcpiOsUnmapMemory(virtual_ptr, Width / 8);
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {

    switch (Width) {
        case 8:
            *Value = inb((uint16_t)Address);
            break;
        case 16:
            *Value = inw((uint16_t)Address);
            break;
        case 32:
            *Value = ind((uint16_t)Address);
            break;
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    switch (Width) {
        case 8:
            outb((uint16_t)Address, (uint8_t)Value);
            break;
        case 16:
            outw((uint16_t)Address, (uint16_t)Value);
            break;
        case 32:
            outd((uint16_t)Address, (uint32_t)Value);
            break;
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength) {
    // We do not want to override any tables.
    
    *NewAddress = 0;
    *NewTableLength = 0;
    
    return AE_OK;
}
}