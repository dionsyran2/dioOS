#include <acpi.h>
#include <kstdio.h>
#include <panic.h>
#include <cpu.h>
#include <scheduling/task_scheduler/task_scheduler.h>

ACPI_STATUS acpi_get_device_cb(ACPI_HANDLE obj, UINT32 level, void *ctx, void **ret){
    *((ACPI_HANDLE*)ctx) = obj;
    return AE_CTRL_TERMINATE;
}

ACPI_STATUS AcpiGetDevice(const char* DeviceName, ACPI_HANDLE* OutHandle){
    ACPI_STATUS status = AcpiGetDevices((char*)DeviceName, acpi_get_device_cb, OutHandle, nullptr);
    return status;
}

ACPI_STATUS AcpiEvaluateInteger(ACPI_HANDLE Handle, ACPI_STRING Pathname, 
                                ACPI_OBJECT_LIST *Arguments, ACPI_INTEGER *ReturnValue)
{
    ACPI_STATUS Status;
    ACPI_OBJECT InternalResult;
    ACPI_BUFFER Buffer;

    if (!ReturnValue)
        return AE_BAD_PARAMETER;

    Buffer.Length = sizeof(ACPI_OBJECT);
    Buffer.Pointer = &InternalResult;

    Status = AcpiEvaluateObject(Handle, Pathname, Arguments, &Buffer);

    if (ACPI_FAILURE(Status))
        return Status;


    if (InternalResult.Type != ACPI_TYPE_INTEGER)
        return AE_TYPE; // Error: Type Mismatch


    *ReturnValue = InternalResult.Integer.Value;
    
    return AE_OK;
}

ACPI_STATUS AcpiSleep(UINT8 state, bool disable_intr){
    ACPI_STATUS status;
    status = AcpiEnterSleepStatePrep(state);

    if (ACPI_FAILURE(status)){
        kprintf("[ACPICA] Could not prepate for sleep state %d\n", state);
        return state;
    }

    uint32_t rflags = get_cpu_flags();
    if (disable_intr) asm ("cli");

    status = AcpiEnterSleepState(5);

    set_cpu_flags(rflags);
    if (ACPI_FAILURE(status)){
        kprintf("[ACPICA] Could not enter sleep state %d\n", state);
        return state;
    }

    return status;
}

void power_off(){
    AcpiSleep(5, true);

    kprintf("Its now safe to turn off your computer\n");
}