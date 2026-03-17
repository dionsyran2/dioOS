#include <acpi.h>
#include <kstdio.h>
extern "C"{
#include <acpica/aclocal.h>
}

uint32_t AcpiPowerButtonHandler(void* Context) {
    power_off();
    
    return ACPI_INTERRUPT_HANDLED;
}

void ACPI_SYSTEM_XFACE AcpiButtonNotify(ACPI_HANDLE Device, UINT32 Value, void *Context) {
    if (Value == 0x80) {
        kprintf("[PWR] Power Button Pressed via Control Method!\n");
        //AcpiPowerButtonHandler(nullptr);
    }
}

ACPI_STATUS SetupButtonDevice(ACPI_HANDLE ObjHandle, UINT32 Level, void *Ctx, void **Ret) {
    AcpiInstallNotifyHandler(ObjHandle, ACPI_DEVICE_NOTIFY, AcpiButtonNotify, NULL);
    return AE_OK;
}

void setup_acpi_kernel_events(){
    ACPI_STATUS status = AcpiInstallFixedEventHandler(
        ACPI_EVENT_POWER_BUTTON, 
        AcpiPowerButtonHandler, 
        NULL
    );

    if (ACPI_FAILURE(status)) {
        kprintf("[ACPI] Failed to install Power Button handler: %s\n", AcpiFormatException(status));
    } else {
        AcpiEnableEvent(ACPI_EVENT_POWER_BUTTON, 0);
    }

    AcpiGetDevices("PNP0C0C", SetupButtonDevice, NULL, NULL);
    AcpiGetDevices("PNP0C0E", SetupButtonDevice, NULL, NULL);
}