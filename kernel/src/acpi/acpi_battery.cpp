/* A battery driver (For portable devices only!) */
#include <acpi.h>
#include <kstdio.h>
#define BATTERY_DEV_NAME "PNP0C0A"
#define AC_DEV_NAME "ACPI0003"

void ACPI_SYSTEM_XFACE AcpiBatteryHandler(ACPI_HANDLE Device, UINT32 Value, void *Context) {
    if (Value == 0x80){
        ACPI_BUFFER buffer = { ACPI_ALLOCATE_BUFFER, NULL };
        ACPI_HANDLE battery_handle;
        AcpiGetDevice(BATTERY_DEV_NAME, &battery_handle);
        ACPI_STATUS status = AcpiEvaluateObject(battery_handle, "_BST", NULL, &buffer);

        if (ACPI_FAILURE(status)) return;

        ACPI_OBJECT *package = (ACPI_OBJECT *)buffer.Pointer;

        if (package->Type == ACPI_TYPE_PACKAGE && package->Package.Count == 4) {
            // Element 0: Battery State (Bitmask)
            // 1 = Discharging, 2 = Charging, 4 = Critical
            ACPI_INTEGER state = package->Package.Elements[0].Integer.Value;
            
            // Element 1: Present Rate (mA or mW)
            ACPI_INTEGER rate = package->Package.Elements[1].Integer.Value;
            
            // Element 2: Remaining Capacity (mAh or mWh)
            ACPI_INTEGER remaining = package->Package.Elements[2].Integer.Value;
            
            // Element 3: Present Voltage (mV)
            ACPI_INTEGER voltage = package->Package.Elements[3].Integer.Value;

            kprintf("Battery Status:\n");
            kprintf(" - State: %d (1=Discharge, 2=Charge)\n", state);
            kprintf(" - Rate: %d\n", rate);
            kprintf(" - Remaining: %d\n", remaining);
            kprintf(" - Voltage: %d\n", voltage);

        }

        AcpiOsFree(buffer.Pointer);
    } else {
        kprintf("Error: _BST returned unexpected type or count.\n");
    }
    
    
    return;
}

void SetBatteryTripPoint(ACPI_HANDLE BatHandle, UINT32 TripLevel) {
    ACPI_OBJECT Arg1;
    ACPI_OBJECT_LIST Args;
    ACPI_STATUS Status;

    Arg1.Type = ACPI_TYPE_INTEGER;
    Arg1.Integer.Value = TripLevel;

    Args.Count = 1;
    Args.Pointer = &Arg1;

    // Note: _BTP usually returns nothing, so ReturnBuffer is NULL
    Status = AcpiEvaluateObject(BatHandle, "_BTP", &Args, NULL);

    if (ACPI_FAILURE(Status)) {
        kprintf("Failed to set _BTP: %s\n", AcpiFormatException(Status));
    }
}

ACPI_STATUS AcpiDeviceProbe(ACPI_HANDLE ObjHandle, UINT32 NestingLevel, void *Context, void **ReturnValue) {
    ACPI_STATUS status;
    ACPI_INTEGER sta_result = 0;

    // Check _STA to see if the device is actually present
    status = AcpiEvaluateInteger(ObjHandle, "_STA", NULL, &sta_result);
    
    // If _STA exists and bit 0 is 0 (0 = Not Present), skip this device
    if (ACPI_SUCCESS(status) && !(sta_result & 0x01)) {
        return AE_OK; 
    }

    AcpiInstallNotifyHandler(ObjHandle, ACPI_ALL_NOTIFY, AcpiBatteryHandler, NULL);

    return AE_OK;
}

void acpi_initialize_battery() {
    /*
    AcpiGetDevices(AC_DEV_NAME, AcpiDeviceProbe, NULL, NULL);
    // Search for ALL Batteries (ID: PNP0C0A)
    AcpiGetDevices(BATTERY_DEV_NAME, AcpiDeviceProbe, NULL, NULL);
    */
}