#include <stdint.h>
#include <cstr.h>

namespace PCI {
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

    const char* GetVendorName(uint16_t vendorID){
        switch (vendorID){
            case 0x8086:
                return "Intel Corp";
            case 0x1022:
                return "AMD";
            case 0x10DE:
                return "NVIDIA Corporation";
            case 0x15AD:
                return "VMware";
            case 0x1274:
                return "Ensoniq";
            case 0x1002:
                return "ATI";
        }
        return toHString(vendorID);
    }

    const char* GetDeviceName(uint16_t vendorID, uint16_t deviceID){
        switch (vendorID){
            case 0x8086: // Intel
                switch(deviceID){
                    case 0x29C0:
                        return "Express DRAM Controller";
                    case 0x2918:
                        return "LPC Interface Controller";
                    case 0x2922:
                        return "6 port SATA Controller [AHCI mode]";
                    case 0x2930:
                        return "SMBus Controller";
                    case 0x100F:
                        return "82545EM Gigabit Ethernet Controller (Copper)";
                    case 0x7190:
                        return "440BX/ZX/DX - 82443BX/ZX/DX Host bridge";
                    case 0x7191:
                        return "440BX/ZX/DX - 82443BX/ZX/DX AGP bridge";
                    case 0x7110:
                        return "82371AB/EB/MB PIIX4 ISA";
                    case 0x7111:
                        return "82371AB/EB/MB PIIX4 IDE";
                    case 0x7113:
                        return "82371AB/EB/MB PIIX4 ACPI";
                    case 0x0150:
                        return "Xeon E3-1200 v2/3rd Gen Core processor DRAM Controller";
                    case 0x0151:
                        return "Xeon E3-1200 v2/3rd Gen Core processor PCI Express Root Port";
                    case 0x1E31:
                        return "7 Series/C210 Series Chipset Family USB xHCI Host Controller";
                    case 0x1E3A:
                        return "7 Series/C216 Chipset Family MEI Controller #1";
                    case 0x1E3D:
                        return "7 Series/C210 Series Chipset Family KT Controller";
                    case 0x1502:
                        return "82579LM Gigabit Network Connection (Lewisville)";
                    case 0x1E2D:
                        return "7 Series/C216 Chipset Family USB Enhanced Host Controller #2";
                    case 0x1E20:
                        return "7 Series/C216 Chipset Family High Definition Audio Controller";
                    case 0x1E26:
                        return "7 Series/C216 Chipset Family USB Enhanced Host Controller #1";
                    case 0x244E:
                        return "82801 PCI Bridge";
                    case 0x1E48:
                        return "Q75 Express Chipset LPC Controller";
                    case 0x1E02:
                        return "7 Series/C210 Series Chipset Family 6-port SATA Controller [AHCI mode]";
                    case 0x1E22:
                        return "7 Series/C216 Chipset Family SMBus Controller";
                    case 0x4641:
                        return "12th Gen Core Processor Host Bridge/DRAM Registers";
                    case 0x46A6:
                        return "Alder Lake-P GT2 [Iris Xe Graphics]";
                    case 0x461D:
                        return "Alder Lake Innovation Platform Framework Processor Participant";
                    case 0x09AB:
                        return "RST VMD Managed Controller";
                    case 0x464F:
                        return "12th Gen Core Processor Gaussian & Neural Accelerator";
                    case 0x467D:
                        return "Platform Monitoring Technology";
                    case 0x467F:
                        return "Volume Management Device NVMe RAID Controller";
                    case 0x51ED:
                        return "Alder Lake PCH USB 3.2 xHCI Host Controller";
                    case 0x51EF:
                        return "Alder Lake PCH Shared SRAM";
                    case 0x51E8:
                        return "Alder Lake PCH Serial IO I2C Controller #0";
                    case 0x51E9:
                        return "Alder Lake PCH Serial IO I2C Controller #1";
                    case 0x51E0:
                        return "Alder Lake PCH HECI Controller";
                    case 0x51A8:
                        return "Alder Lake PCH UART #0";
                    case 0x51AA:
                        return "Alder Lake SPI Controller";
                    case 0x5182:
                        return "Alder Lake PCH eSPI Controller";
                    case 0x51C8:
                        return "Alder Lake PCH-P High Definition Audio Controller";
                    case 0x2668:
                        return "82801FB/FBM/FR/FW/FRW (ICH6 Family) High Definition Audio Controller";
                }
            case 0x15AD: //VMware
                switch (deviceID)
                {
                    case 0x0740:
                        return "Virtual Machine Communication Interface";
                    case 0x0405:
                        return "SVGA II Adapter";
                    case 0x0790:
                        return "PCI bridge";
                    case 0x07A0:
                        return "PCI Express Root Port";
                    case 0x0774:
                        return "USB1.1 UHCI Controller";
                    case 0x07E0:
                        return "SATA AHCI controller";
                }
            case 0x1274: //Ensoniq
                switch (deviceID)
                {
                    case 0x1371:
                        return "ES1371/ES1373 / Creative Labs CT2518"; 
                }
            case 0x1002: //ATI
                switch (deviceID)
                {
                    case 0x1478:
                        return "Navi 10 XL Upstream Port of PCI Express Switch";
                    case 0x1479:
                        return "Navi 10 XL Downstream Port of PCI Express Switch";
                    case 0x743F:
                        return "Navi 24 [Radeon RX 6400/6500 XT/6500M]";
                    case 0xAB28:
                        return "Navi 21/23 HDMI/DP Audio Controller";
                }
            case 0x1022: //AMD
                switch(deviceID){
                    case 0x1480:
                        return "Starship/Matisse Root Complex";
                    case 0x1481:
                        return "Starship/Matisse IOMMU";
                    case 0x1482:
                        return "Starship/Matisse PCIe Dummy Host Bridge";
                    case 0x1483:
                        return "Starship/Matisse GPP Bridge";
                    case 0x1484:
                        return "Starship/Matisse Internal PCIe GPP Bridge 0 to bus[E:B]";
                    case 0x1440:
                        return "Matisse Device 24: Function 0";
                    case 0x1441:
                        return "Matisse Device 24: Function 1";
                    case 0x1442:
                        return "Matisse Device 24: Function 2";
                    case 0x1443:
                        return "Matisse Device 24: Function 3";
                    case 0x1444:
                        return "Matisse Device 24: Function 4";
                    case 0x1445:
                        return "Matisse Device 24: Function 5";
                    case 0x1446:
                        return "Matisse Device 24: Function 6";
                    case 0x1447:
                        return "Matisse Device 24: Function 7";
                    case 0x43EE:
                        return "500 Series Chipset USB 3.1 XHCI Controller";
                    case 0x43EB:
                        return "500 Series Chipset SATA Controller";
                    case 0x43E9:
                        return "500 Series Chipset Switch Upstream Port";
                    case 0x148A:
                        return "Starship/Matisse PCIe Dummy Function";
                    case 0x1485:
                        return "Starship/Matisse Reserved SPP";
                    case 0x1486:
                        return "Starship/Matisse Cryptographic Coprocessor PSPCPP";
                    case 0x149C:
                        return "Matisse USB 3.0 Host Controller";
                    case 0x1487:
                        return "Starship/Matisse HD Audio Controller";
                }
                case 0x10EC:{ //Realtek
                    switch(deviceID){
                        case 0x8168:
                            return "RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller";
                    }
                }
        }
        return toHString(deviceID);
    }

    const char* MassStorageControllerSubclassName(uint8_t subclassCode){
        switch (subclassCode){
            case 0x00:
                return "SCSI Bus Controller";
            case 0x01:
                return "IDE Controller";
            case 0x02:
                return "Floppy Disk Controller";
            case 0x03:
                return "IPI Bus Controller";
            case 0x04:
                return "RAID Controller";
            case 0x05:
                return "ATA Controller";
            case 0x06:
                return "Serial ATA";
            case 0x07:
                return "Serial Attached SCSI";
            case 0x08:
                return "Non-Volatile Memory Controller";
            case 0x80:
                return "Other";
        }
        return toHString(subclassCode);
    }

    const char* SerialBusControllerSubclassName(uint8_t subclassCode){
        switch (subclassCode){
            case 0x00:
                return "FireWire (IEEE 1394) Controller";
            case 0x01:
                return "ACCESS Bus";
            case 0x02:
                return "SSA";
            case 0x03:
                return "USB Controller";
            case 0x04:
                return "Fibre Channel";
            case 0x05:
                return "SMBus";
            case 0x06:
                return "Infiniband";
            case 0x07:
                return "IPMI Interface";
            case 0x08:
                return "SERCOS Interface (IEC 61491)";
            case 0x09:
                return "CANbus";
            case 0x80:
                return "SerialBusController - Other";
        }
        return toHString(subclassCode);
    }

    const char* BridgeDeviceSubclassName(uint8_t subclassCode){
        switch (subclassCode){
            case 0x00:
                return "Host Bridge";
            case 0x01:
                return "ISA Bridge";
            case 0x02:
                return "EISA Bridge";
            case 0x03:
                return "MCA Bridge";
            case 0x04:
                return "PCI-to-PCI Bridge";
            case 0x05:
                return "PCMCIA Bridge";
            case 0x06:
                return "NuBus Bridge";
            case 0x07:
                return "CardBus Bridge";
            case 0x08:
                return "RACEway Bridge";
            case 0x09:
                return "PCI-to-PCI Bridge";
            case 0x0a:
                return "InfiniBand-to-PCI Host Bridge";
            case 0x80:
                return "Other";
        }
        return toHString(subclassCode);
    }

    const char* GetSubclassName(uint8_t classCode, uint8_t subclassCode){
        switch (classCode){
            case 0x01:
                return MassStorageControllerSubclassName(subclassCode);
            case 0x03:
                switch (subclassCode){
                    case 0x00:
                        return "VGA Compatible Controller";
                }
            case 0x06:
                return BridgeDeviceSubclassName(subclassCode);
            case 0x0C:
                return SerialBusControllerSubclassName(subclassCode);
        }
        return toHString(subclassCode);
    }

    const char* GetProgIFName(uint8_t classCode, uint8_t subclassCode, uint8_t progIF){
        switch (classCode){
            case 0x01:
                switch (subclassCode){
                    case 0x06:
                        switch (progIF){
                            case 0x00:
                                return "Vendor Specific Interface";
                            case 0x01:
                                return "AHCI 1.0";
                            case 0x02:
                                return "Serial Storage Bus";
                        }
                }
            case 0x03:
                switch (subclassCode){
                    case 0x00:
                        switch (progIF){
                            case 0x00:
                                return "VGA Controller";
                            case 0x01:
                                return "8514-Compatible Controller";
                        }
                }
            case 0x0C:
                switch (subclassCode){
                    case 0x03:
                        switch (progIF){
                            case 0x00:
                                return "UHCI Controller";
                            case 0x10:
                                return "OHCI Controller";
                            case 0x20:
                                return "EHCI (USB2) Controller";
                            case 0x30:
                                return "XHCI (USB3) Controller";
                            case 0x80:
                                return "Unspecified";
                            case 0xFE:
                                return "USB Device (Not a Host Controller)";
                        }
                }    
        }
        return toHString(progIF);
    }
}