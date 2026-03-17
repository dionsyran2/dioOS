#include <acpica/embedded_controller.h>
#include <paging/PageTableManager.h>
#include <kstdio.h>
#include <memory.h>
#include <cstr.h>

ACPI_HANDLE EC_DEV = nullptr;
uint32_t EC_GPE = 0;

/* Let ACPICA access the EC directly */
ACPI_STATUS AcpiEcHandler(UINT32 Function, ACPI_PHYSICAL_ADDRESS Address, UINT32 BitWidth, UINT64 *Value, void *HandlerContext, void* RegionContext){
    if (Function == ACPI_WRITE){
        return AcpiOsWritePort(Address, (UINT64)Value, BitWidth);
    }else if (Function == ACPI_READ){
        return AcpiOsReadPort(Address, (UINT32*)Value, BitWidth);
    }

    return AE_BAD_PARAMETER;
}

/* Driver Helpers */
uint8_t ec_read(uint16_t port) {
    return inb(port); 
}

uint8_t ec_read_status() {
    return ec_read(EC_STS_PORT);
}

void ec_write(uint16_t port, uint8_t data) {
    while(ec_read_status() & EC_STS_IBF) { 
        io_wait(); 
    }

    outb(port, data);
}

void ec_send_command(uint32_t command){
    ec_write(EC_CMD_PORT, command);

    do{
        io_wait();
    }while(ec_read_status() & EC_STS_IBF);
}

/* The EC interrupt handler (Registered via ACPICA) */
UINT32 EcGpeHandler(ACPI_HANDLE GpeDevice, UINT32 GpeNumber, void *Context){
    UINT32 handle;
    ACPI_STATUS status;
    uint32_t ec_status = ec_read_status();
    if (!(ec_status & EC_STS_ECI_EVT)) return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;;

    AcpiAcquireGlobalLock(-1, &handle);

    ec_send_command(EC_CMD_QR);

    ec_status = ec_read_status();

    uint8_t evt = ec_read(EC_DATA_PORT);

    do {
        io_wait();
        ec_status = ec_read_status();
    } while (!(ec_status & EC_STS_OBF));

    AcpiReleaseGlobalLock(handle);

    // Construct the _Qxx method nane
    const char* hex_number = toHString(evt);
    char method_name[5] = {'_', 'Q', hex_number[0], hex_number[1], '\0'};

    status = AcpiEvaluateObject(EC_DEV, method_name, NULL, NULL);
    if (ACPI_FAILURE(status)){
        kprintf("[EC] Could not execute method %s with error: %s\n", method_name, AcpiFormatException(status));
    } else {
        kprintf("[EC] Executed %s successfully %s\n", method_name, AcpiFormatException(status));
    }
    

    return ACPI_INTERRUPT_HANDLED | ACPI_REENABLE_GPE;
}

/* Find / Enable the EC (Its not ACPICA's job to initialize and enable its gpes) */
ACPI_STATUS acpi_setup_ec(){
    EC_DEV = nullptr;

    ACPI_STATUS status;
    ACPI_BUFFER Buffer = { ACPI_ALLOCATE_BUFFER, NULL };
    ACPI_HANDLE GpeBlock = NULL; // Default is NULL (FADT GPE Block)
    ACPI_OBJECT* Obj;

    // Get the EC Device
    status = AcpiGetDevice(EC_DEV_NAME, &EC_DEV);
    if (ACPI_FAILURE(status)) goto cleanup;
    
    // Evaluate _GPE to get its gpe number
    status = AcpiEvaluateObject(EC_DEV, "_GPE", NULL, &Buffer);
    if (ACPI_FAILURE(status)) goto cleanup;

    Obj = (ACPI_OBJECT *)Buffer.Pointer;
    if (Obj->Type == ACPI_TYPE_INTEGER) {
        // Case 1: Simple Integer (Most common)
        EC_GPE = Obj->Integer.Value;
        GpeBlock = NULL; 
    }else if (Obj->Type == ACPI_TYPE_PACKAGE) {
        // Case 2: Package [DeviceReference, Number]
        // The GPE is inside a separate GPE Block Device.
        if (Obj->Package.Count >= 2 && 
            Obj->Package.Elements[0].Type == ACPI_TYPE_LOCAL_REFERENCE) {
            
            GpeBlock = Obj->Package.Elements[0].Reference.Handle;
            EC_GPE = Obj->Package.Elements[1].Integer.Value;
        }
    }

    // Install the handler
    status = AcpiInstallGpeHandler(GpeBlock, EC_GPE, ACPI_GPE_EDGE_TRIGGERED, EcGpeHandler, NULL);
    if (ACPI_FAILURE(status)) goto cleanup;
    
    // Enable the GPE
    status = AcpiEnableGpe(GpeBlock, EC_GPE);
    if (ACPI_FAILURE(status)) goto cleanup;

cleanup:
    AcpiOsFree(Buffer.Pointer);
    return status;
}

ACPI_STATUS acpi_intialize_embedded_controller(){
    ACPI_STATUS status = AcpiInstallAddressSpaceHandler(
        ACPI_ROOT_OBJECT,
        ACPI_ADR_SPACE_EC,
        AcpiEcHandler,
        NULL,
        NULL
    );
    return status;
}

void embedded_controler_enable_gpes() {
    acpi_setup_ec();
}