#include <pci.h>
#include <cstr.h>

/* Device Class */
const char* DeviceClasses[] {
    "Unclassified",
    "Mass Storage Controller",
    "Network Controller",
    "Display Controller",
    "Multimedia Controller",
    "Memory Controller",
    "Bridge Device",
    "Simple Communication Controller",
    "Base System Peripheral",
    "Input Device Controller",
    "Docking Station", 
    "Processor",
    "Serial Bus Controller",
    "Wireless Controller",
    "Intelligent Controller",
    "Satellite Communication Controller",
    "Encryption Controller",
    "Signal Processing Controller",
    "Processing Accelerator",
    "Non Essential Instrumentation"
};

/* Vendor Name Entry */
typedef struct {
    uint16_t id;
    const char* name;
} PciVendorEntry;

static const PciVendorEntry pci_vendors[] = {
    { 0x8086, "Intel Corporation" },
    { 0x10DE, "NVIDIA Corporation" },
    { 0x1002, "Advanced Micro Devices, Inc." },
    { 0x1022, "Advanced Micro Devices, Inc. [Chipsets]" },
    { 0x10EC, "Realtek Semiconductor Co., Ltd." },
    { 0x1AF4, "Red Hat, Inc. (VirtIO)" },
    { 0x1234, "QEMU Virtual Device" },
    { 0x15AD, "VMware" },
    { 0x80EE, "VirtualBox" },
    { 0, NULL }
};

/* Device Names */

typedef struct {
    uint16_t device_id;
    const char* name;
} PciDeviceEntry;

typedef struct {
    uint16_t vendor_id;
    const char* vendor_name;
    const PciDeviceEntry* devices; // Pointer to a device list for this vendor
} PciVendorCollection;

// --- Intel Devices ---
static const PciDeviceEntry intel_devices[] = {
    { 0x0150, "Xeon E3-1200 v2/3rd Gen Core processor DRAM Controller" },
    { 0x0151, "Xeon E3-1200 v2/3rd Gen Core processor PCI Express Root Port" },
    { 0x100F, "PRO/1000 Gigabit Ethernet Controller (Copper)" },
    { 0x10D3, "PRO/1000 Gigabit Ethernet" },
    { 0x1E31, "7 Series/C210 Series Chipset Family USB xHCI Host Controller" },
    { 0x1E3A, "7 Series/C216 Chipset Family MEI Controller #1" },
    { 0x1E3D, "7 Series/C210 Series Chipset Family KT Controller" },
    { 0x1502, "82579LM Gigabit Network Connection (Lewisville)" },
    { 0x2918, "LPC Interface Controller" },
    { 0x2922, "6 port SATA Controller [AHCI mode]" },
    { 0x2930, "SMBus Controller" },
    { 0x29C0, "Express DRAM Controller" },
    { 0x7110, "82371AB/EB/MB PIIX4 ISA" },
    { 0x7111, "82371AB/EB/MB PIIX4 IDE" },
    { 0x7113, "82371AB/EB/MB PIIX4 ACPI" },
    { 0x7190, "440BX/ZX/DX - 82443BX/ZX/DX Host bridge" },
    { 0x7191, "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge" },
    { 0x1E2D, "7 Series/C216 Chipset Family USB Enhanced Host Controller #2" },
    { 0x1E20, "7 Series/C216 Chipset Family High Definition Audio Controller" },
    { 0x1E26, "7 Series/C216 Chipset Family USB Enhanced Host Controller #1" },
    { 0x244E, "82801 PCI Bridge" },
    { 0x1E48, "Q75 Express Chipset LPC Controller" },
    { 0x1E02, "7 Series/C210 Series Chipset Family 6-port SATA Controller [AHCI mode]" },
    { 0x1E22, "7 Series/C216 Chipset Family SMBus Controller" },
    { 0x4641, "12th Gen Core Processor Host Bridge/DRAM Registers" },
    { 0x46A6, "Alder Lake-P GT2 [Iris Xe Graphics]" },
    { 0x461D, "Alder Lake Innovation Platform Framework Processor Participant" },
    { 0x09AB, "RST VMD Managed Controller" },
    { 0x464F, "12th Gen Core Processor Gaussian & Neural Accelerator" },
    { 0x467D, "Platform Monitoring Technology" },
    { 0x467F, "Volume Management Device NVMe RAID Controller" },
    { 0x51ED, "Alder Lake PCH USB 3.2 xHCI Host Controller" },
    { 0x51EF, "Alder Lake PCH Shared SRAM" },
    { 0x51E8, "Alder Lake PCH Serial IO I2C Controller #0" },
    { 0x51E9, "Alder Lake PCH Serial IO I2C Controller #1" },
    { 0x51E0, "Alder Lake PCH HECI Controller" },
    { 0x51A8, "Alder Lake PCH UART #0" },
    { 0x51AA, "Alder Lake SPI Controller" },
    { 0x5182, "Alder Lake PCH eSPI Controller" },
    { 0x51C8, "Alder Lake PCH-P High Definition Audio Controller" },
    { 0x2668, "82801FB/FBM/FR/FW/FRW (ICH6 Family) High Definition Audio Controller" },
    { 0, NULL }
};

// --- AMD Devices ---
static const PciDeviceEntry amd_devices[] = {
    { 0x1480, "Starship/Matisse Root Complex" },
    { 0x1481, "Starship/Matisse IOMMU" },
    { 0x1482, "Starship/Matisse PCIe Dummy Host Bridge" },
    { 0x1483, "Starship/Matisse GPP Bridge" },
    { 0x1484, "Starship/Matisse Internal PCIe GPP Bridge 0 to bus[E:B]" },
    { 0x1440, "Matisse Device 24: Function 0" },
    { 0x1441, "Matisse Device 24: Function 1" },
    { 0x1442, "Matisse Device 24: Function 2" },
    { 0x1443, "Matisse Device 24: Function 3" },
    { 0x1444, "Matisse Device 24: Function 4" },
    { 0x1445, "Matisse Device 24: Function 5" },
    { 0x1446, "Matisse Device 24: Function 6" },
    { 0x1447, "Matisse Device 24: Function 7" },
    { 0x43EE, "500 Series Chipset USB 3.1 XHCI Controller" },
    { 0x43EB, "500 Series Chipset SATA Controller" },
    { 0x43E9, "500 Series Chipset Switch Upstream Port" },
    { 0x148A, "Starship/Matisse PCIe Dummy Function" },
    { 0x1485, "Starship/Matisse Reserved SPP" },
    { 0x1486, "Starship/Matisse Cryptographic Coprocessor PSPCPP" },
    { 0x149C, "Matisse USB 3.0 Host Controller" },
    { 0x1487, "Starship/Matisse HD Audio Controller" },
    { 0x2000, "79C970 PCnet32 LANCE" },
    { 0, NULL }
};

// -- Former ATI now AMD Devices ---
static const PciDeviceEntry ati_devices[] = {
    { 0x1478, "Navi 10 XL Upstream Port of PCI Express Switch" },
    { 0x1479, "Navi 10 XL Downstream Port of PCI Express Switch" },
    { 0x743F, "Navi 24 [Radeon RX 6400/6500 XT/6500M]" },
    { 0xAB28, "Navi 21/23 HDMI/DP Audio Controller" },
    { 0, NULL }
};

// -- VMWare Devices ---
static const PciDeviceEntry vmware_devices[] = {
    { 0x0740, "Virtual Machine Communication Interface" },
    { 0x0405, "SVGA II Adapter" },
    { 0x0790, "PCI bridge" },
    { 0x07A0, "PCI Express Root Port" },
    { 0x0770, "USB2 EHCI Controller" },
    { 0x0774, "USB1.1 UHCI Controller" },
    { 0x07E0, "SATA AHCI controller" },
    { 0x07F0, "NVMe SSD Controller" },
    { 0x1977, "HD Audio Controller" },
    { 0, NULL }
};

// --- Realtek Devices ---
static const PciDeviceEntry realtek_devices[] = {
    { 0x8168, "RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller"},
    { 0xC821, "RTL8821CE 802.11ac PCIe Wireless Network Adapter" },
    { 0, NULL }
};

// --- Main Table ---
static const PciVendorCollection pci_database[] = {
    { 0x8086, "Intel", intel_devices },
    { 0x1022, "AMD", amd_devices },
    { 0x1002, "ATI", ati_devices},
    { 0x15AD, "VMWare", vmware_devices },
    { 0x10EC, "Realtek", realtek_devices }, 
    { 0, NULL, NULL }
};
namespace pci{
    const char* get_class_name(uint8_t class_id){
        return DeviceClasses[class_id];
    }

    const char* get_vendor_name(uint16_t vendor){
        for (int i = 0; pci_vendors[i].name != NULL; i++){
            if (pci_vendors[i].id == vendor) return pci_vendors[i].name;
        }

        return toHString(vendor);
    }

    const char* get_device_name(uint16_t vendor, uint16_t device){
        for (int i = 0; pci_database[i].devices != NULL; i++){
            if (pci_database[i].vendor_id != vendor) continue;

            for (int y = 0; pci_database[i].devices[y].name != NULL; y++){
                if (pci_database[i].devices[y].device_id == device) 
                    return pci_database[i].devices[y].name;
            }
        }
        return toHString(device);
    }
}