#pragma once
#include <stdint.h>
#include <stddef.h>
#define AHCI_COMMAND_LIST_ENTRY_COUNT 	32
#define PRDT_ENTRY_COUNT 				8
enum fis_type_t
{
	FIS_TYPE_REG_H2D	= 0x27,	// Register FIS - host to device
	FIS_TYPE_REG_D2H	= 0x34,	// Register FIS - device to host
	FIS_TYPE_DMA_ACT	= 0x39,	// DMA activate FIS - device to host
	FIS_TYPE_DMA_SETUP	= 0x41,	// DMA setup FIS - bidirectional
	FIS_TYPE_DATA		= 0x46,	// Data FIS - bidirectional
	FIS_TYPE_BIST		= 0x58,	// BIST activate FIS - bidirectional
	FIS_TYPE_PIO_SETUP	= 0x5F,	// PIO setup FIS - device to host
	FIS_TYPE_DEV_BITS	= 0xA1,	// Set device bits FIS - device to host
};


struct ahci_port_register_t{
    uint32_t command_list_base_lo;
    uint32_t command_list_base_hi;

    uint32_t fis_base_lo;
    uint32_t fis_base_hi;

    uint32_t interrupt_status;
    uint32_t interrupt_enable;

    uint32_t command;
    uint32_t rsv0;
    uint32_t task_file_data;
    uint32_t signature;

    uint32_t sata_status;
    uint32_t sata_control;
    uint32_t sata_error;
    uint32_t sata_active;

    uint32_t command_issue;

    uint32_t sata_notification;

    uint32_t fis_based_switch_control;

    uint32_t rsv1[11];
    uint32_t vendor[4];
} __attribute__((packed));

static_assert(sizeof(ahci_port_register_t) == 0x80, "ahci_port_register_t is not 0x80 bytes as mandated by the spec!");
 
struct ahci_hba_t{
    uint32_t capabilities;
    uint32_t global_host_control;
    uint32_t interrupt_status;
    uint32_t ports_implemented;
    uint32_t version;
    uint32_t command_completion_coalescing_control;
    uint32_t command_completion_coalescing_ports;
	uint32_t enclosure_management_location;
    uint32_t enclosure_management_control;
    uint32_t extended_capabilities;
    uint32_t bios_os_handoff;
    uint8_t rsv[116];
    uint8_t vendor_specific_registers[96];

    ahci_port_register_t ports[32]; // 1 - 32
} __attribute__((packed));

struct ahci_command_header_t{
	uint8_t  command_fis_length: 5;		// Command FIS length in DWORDS, 2 ~ 16
	uint8_t  atapi: 1;					// ATAPI
	uint8_t  write: 1;					// Write, 1: H2D, 0: D2H
	uint8_t  prefetchable: 1;			// Prefetchable

	uint8_t  reset: 1;					// Reset
	uint8_t  bist: 1;					// BIST
	uint8_t  clear: 1;					// Clear busy upon R_OK
	uint8_t  rsv0: 1;					// Reserved
	uint8_t  port_multiplier_port: 4;	// Port multiplier port

	uint16_t prdtl;						// Physical region descriptor table length in entries

	// DW1
	volatile uint32_t physical_region_descriptor_byte_count;						// Physical region descriptor byte count transferred

	// DW2, 3
	uint32_t command_table_descriptor_base_low;						// Command table descriptor base address
	uint32_t command_table_descriptor_base_high;					// Command table descriptor base address upper 32 bits

	// DW4 - 7
	uint32_t rsv1[4];					// Reserved
} __attribute__((packed));

struct ahci_physical_region_descriptor_t{
	uint32_t data_base_address_low;		// Data base address
	uint32_t data_base_address_high;		// Data base address upper 32 bits
	uint32_t rsv0;		// Reserved

	// DW3
	uint32_t byte_count: 22;		// Byte count, 4M max
	uint32_t rsv1: 9;		// Reserved
	uint32_t interrupt_on_completion: 1;		// Interrupt on completion
} __attribute__((packed));

struct ahci_fis_stub_t{
	uint8_t stub[64];
};

struct ahci_fis_reg_h2d{
	// DWORD 0
	uint8_t  fis_type;	// FIS_TYPE_REG_H2D

	uint8_t  port_multiplier: 4;	// Port multiplier
	uint8_t  rsv0: 3;		// Reserved
	uint8_t  c: 1;		// 1: Command, 0: Control

	uint8_t  command;	// Command register
	uint8_t  feature_low;	// Feature register, 7:0
	
	// DWORD 1
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register

	// DWORD 2
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  feature_high;	// Feature register, 15:8

	// DWORD 3
	uint8_t  count_low;		// Count register, 7:0
	uint8_t  count_high;		// Count register, 15:8
	uint8_t  isochronous_command_completion;		// Isochronous command completion
	uint8_t  control;	// Control register

	// DWORD 4
	uint8_t  rsv1[4];	// Reserved
} __attribute__((packed));

struct ahci_command_table_t{
	ahci_fis_stub_t command_fis;	// Command FIS

	uint8_t  atapi_command[16];	// ATAPI command, 12 or 16 bytes

	uint8_t  reserved[48];	// Reserved

	ahci_physical_region_descriptor_t prdt_entries[PRDT_ENTRY_COUNT];
} __attribute__((packed));


// A wrapper containing information about the port
struct ahci_port_t{
	spinlock_t command_slot_lock;
	uint32_t command_slot_state; // A bit mask. If the bit is masked, that slot is in use.

	volatile ahci_port_register_t* port; // The port register itself
	volatile ahci_command_header_t* command_list;
	volatile ahci_command_table_t*  command_table_list; // A list with the command tables (table entry n corresponds to command_list[n] header)

	uint64_t total_sectors;
	
	void initialize();

	void stop();
	void start();
	
	uint8_t allocate_free_slot();
	void free_slot(uint8_t slot);

	bool is_interrupt_set(uint8_t slot);
	void clear_interrupt(uint8_t slot);
	void enable_interrupts();

	void issue_command(uint8_t slot);
	bool get_issue_status(uint8_t slot);

	/* Normal ATA commands*/
	bool read(uint64_t start_sector, uint64_t sector_count, void* buffer);
	bool write(uint64_t start_sector, uint64_t sector_count, const void* buffer);
	bool identify(uint64_t* out_sector_count);
};