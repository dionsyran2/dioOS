#include <drivers/storage/ahci/ahci.h>
#include <partitions/partitions.h>
#include <kstdio.h>
#include <cstr.h>
#include <kerrno.h>

int ahci_port_count = 0; // Used for node creation (sda, sdb, ...)

bool __ahci_driver_t_supports_device(pci::pci_device_header* device){
    if (device->class_code == 0x1 && device->subclass == 0x6) return true;
    return false;
}

base_driver_t* __create_ahci_driver_t_instance(pci::pci_device_header* header){
    return (base_driver_t*)(new drivers::ahci_driver_t(header));
}

DEFINE_DRIVER(ahci_driver_class) = {
    .name = "AHCI driver",
    .supports_device = __ahci_driver_t_supports_device,
    .create_instance = __create_ahci_driver_t_instance
};

/* All of the memory structures, like the command lists, command tables etc are allocated at initialization */

namespace drivers{
    ahci_driver_t::ahci_driver_t(pci::pci_device_header* hdr) : base_driver_t(hdr) {
        this->device_header = (pci::PCIHeader0*)hdr;
    }

    ahci_driver_t::~ahci_driver_t(){

    }

    void ahci_driver_t::_initialize_drive(ahci_port_register_t* port_register){
        // Allocate the command list
        uint64_t command_list_size = sizeof(ahci_command_header_t) * AHCI_COMMAND_LIST_ENTRY_COUNT;
        ahci_command_header_t* command_list = (ahci_command_header_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(command_list_size, PAGE_SIZE));
        memset(command_list, 0, command_list_size);

        // Allocate the command tables
        uint64_t command_table_size = sizeof(ahci_command_table_t) * AHCI_COMMAND_LIST_ENTRY_COUNT; // 1 per command list entry
        ahci_command_table_t* command_table_list = (ahci_command_table_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(command_table_size, PAGE_SIZE));
        memset(command_table_list, 0, command_table_size);
  
        // Create the port struct
        ahci_port_t* port = new ahci_port_t;
        port->port = port_register;
        port->command_list = command_list;
        port->command_table_list = command_table_list;
        port->command_slot_state = 0;
        port->command_slot_lock = 0;

        port->stop();

        void* fis_base = GlobalAllocator.RequestPage();
        uint64_t fis_physical = virtual_to_physical((uint64_t)fis_base);

        port_register->fis_base_lo = fis_physical & 0xFFFFFFFF;
        port_register->fis_base_hi = fis_physical >> 32;

        // Set up the command_list
        for (int i = 0; i < AHCI_COMMAND_LIST_ENTRY_COUNT; i++){
            uint64_t physical = virtual_to_physical((uint64_t)&command_table_list[i]);

            command_list[i].command_table_descriptor_base_low = physical & 0xFFFFFFFF;
            command_list[i].command_table_descriptor_base_high = physical >> 32;
        }

        // Update the port's registers
        uint64_t command_list_physical = virtual_to_physical((uint64_t)command_list);
        port_register->command_list_base_lo = command_list_physical & 0xFFFFFFFF;
        port_register->command_list_base_hi = command_list_physical >> 32;

        port->start();
        port->enable_interrupts();

        port->initialize();
    }


    void ahci_driver_t::_initialize_port(uint8_t port_number){
        // Get the port registers
        ahci_port_register_t* port_register = &this->hba->ports[port_number];

        // Check its status
        uint8_t ipm = (port_register->sata_status >> 8) & 0x0F;
        uint8_t det = port_register->sata_status & 0x0F;

        if (det != HBA_PORT_DET_PRESENT)
            return;
        if (ipm != HBA_PORT_IPM_ACTIVE)
            return;

        // Check the drive type
        switch(port_register->signature){
            case SATA_SIG_ATAPI:
            case SATA_SIG_ATA: // A typical sata drive
                _initialize_drive(port_register);
                break;
            
            case SATA_SIG_SEMB: // Not sure what it is... but it exists
                break;

            case SATA_SIG_PM: // A port multiplier? How many ports do you need?
                break;

            default: // Unknown device
                break;
        }
    }

    void ahci_driver_t::_initialize_ports(){
        uint32_t ports_implemented = this->hba->ports_implemented;

        for (int i = 0; i < 32; i++){
            // Port exists, check if there is a device plugged in
            if ((ports_implemented >> i) & 1){
                _initialize_port(i);
            }
        }
    }


    bool ahci_driver_t::init_device(){
        uint64_t cmd = this->device_header->Header.command;
        cmd |= PCI_CMD_BUS_MASTER | PCI_CMD_MEMORY; // Enable bus mastering & memory space
        cmd &= ~PCI_CMD_INTERRUPT_DISABLE; // Enable Interrupts
        this->device_header->Header.command = cmd;

        this->base_address = pci::get_device_bar(this->pci_device_hdr, 5);
        this->hba = (ahci_hba_t*)this->base_address;

        // Enable AHCI Mode (GHC.AE)
        this->hba->global_host_control |= (1 << 31);

        _initialize_ports();

        kprintf("[AHCI] Driver successfully initialized!\n");
        return true;
    }

    bool ahci_driver_t::start_device(){
        return true;
    }

    bool ahci_driver_t::shutdown_device(){
        return true;
    }
}

/* Port vnode */
extern vnode_ops_t ata_disk_ops;

// Currently a placeholder
vnode_ops_t atapi_disk_ops = {

};

void ahci_port_t::initialize(){
    if (this->port->signature == SATA_SIG_ATA){
        void* buff = GlobalAllocator.RequestPage();
        *((uint32_t*) buff) = 0xAABBCCDD;
        *(((uint32_t*) buff)+1) = 0xAABBCCDD;
        
        // Normal hard disk
        vnode_t* node = new vnode_t(VBLK);
        node->fs_identifier = (uint64_t)this;
        node->io_block_size = 512;
        
        identify(&this->total_sectors);

        node->size = this->total_sectors * 512;

        int disk_number = __atomic_fetch_add(&ahci_port_count, 1, __ATOMIC_SEQ_CST);

        stringf(node->name, sizeof(node->name), "sd%c", 'a' + disk_number);
        memcpy(&node->file_operations, &ata_disk_ops, sizeof(vnode_ops_t));

        vnode_t* dev_dir = vfs::resolve_path("/dev");
        if (dev_dir == nullptr){
            kprintf("[AHCI] Could not resolve path '/dev'\n");
        }else{
            vfs::add_node(dev_dir, node);

            intialize_disk_partitions(node, false);
            
            // Close our reference to /dev
            dev_dir->close();
        }
    } else if (this->port->signature == SATA_SIG_ATAPI){
        vnode_t* node = new vnode_t(VBLK);
        node->fs_identifier = (uint64_t)this;

        char name[64];
        char path[128];

        int drive = 0;

        // Find a name for the node
        while(1){
            stringf(name, 64, "sr%d", drive);
            stringf(path, 128, "/dev/%s", name);
            vnode_t* res = vfs::resolve_path(path);
            
            if (!res) break;
        
            res->close();
            // continue
        }

        strcpy(node->name, name);

        vnode_t* dev_dir = vfs::resolve_path("/dev");
        if (dev_dir == nullptr){
            kprintf("[AHCI] Could not resolve path '/dev'\n");
        }else{
            vfs::add_node(dev_dir, node);
            
            // Close our reference to /dev
            dev_dir->close();
        }
    }
}