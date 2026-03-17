#pragma once
#include <pci.h>
#include <memory.h>
#include <memory/heap.h>
#include <paging/PageFrameAllocator.h>
#include <filesystem/vfs/vfs.h>
#include <drivers/storage/ahci/ahci_defs.h>
#include <drivers/drivers.h>

#define	SATA_SIG_ATA	0x00000101	// SATA drive
#define	SATA_SIG_ATAPI	0xEB140101	// SATAPI drive
#define	SATA_SIG_SEMB	0xC33C0101	// Enclosure management bridge
#define	SATA_SIG_PM	    0x96690101	// Port multiplier

#define HBA_PORT_IPM_ACTIVE 1
#define HBA_PORT_DET_PRESENT 3

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY  0xEC

#define ATA_DEV_BUSY 0x80
#define ATA_DEV_DRQ 0x08

#define HBA_PxIS_TFES   (1 << 30)

namespace drivers{
    class ahci_driver_t : public base_driver_t {
        public:
        ahci_driver_t(pci::pci_device_header* hdr);
        ~ahci_driver_t();

        bool init_device();
        bool start_device();
        bool shutdown_device();

        private:
        void _initialize_drive(ahci_port_register_t* port_register);
        void _initialize_port(uint8_t port_number);
        void _initialize_ports();

        public:

        private:
        uint64_t base_address;
        ahci_hba_t* hba;
        pci::PCIHeader0* device_header;
    };
}