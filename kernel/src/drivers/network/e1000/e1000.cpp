#include <drivers/network/e1000/e1000.h>
#include <drivers/network/e1000/e1000_registers.h>
#include <network/common/common.h>
#include <paging/PageFrameAllocator.h>
#include <drivers/timers/common.h>
#include <memory/heap.h>
#include <memory.h>
#include <kstdio.h>
#include <cstr.h>

#define NUM_OF_TX_DESCRIPTORS 16
#define SIZE_OF_TX_DESCRIPTOR_BUFFER 4096

#define NUM_OF_RX_DESCRIPTORS 32
#define SIZE_OF_RX_DESCRIPTOR_BUFFER 4096

uint16_t e1000_supported_ids[] = {
    0x1019, 0x101A, 0x1010, 0x1012,
    0x101D, 0x1079, 0x107A, 0x107B,
    0x100F, 0x1011, 0x1026, 0x1027,
    0x1028, 0x1107, 0x1112, 0x1013,
    0x1018
};


bool __e1000_driver_t_supports_device(pci::pci_device_header* device){
    if (device->vendor_id == 0x8086){
        for (int i = 0; i < (sizeof(e1000_supported_ids) / sizeof(uint16_t)); i++){
            if (e1000_supported_ids[i] == device->device_id) return true;
        }
    }
    return false;
}

base_driver_t* __create_e1000_driver_t_instance(pci::pci_device_header* header){
    return (base_driver_t*)(new drivers::e1000_driver_t(header));
}

DEFINE_DRIVER(e1000_driver_class) = {
    .name = "E1000 driver",
    .supports_device = __e1000_driver_t_supports_device,
    .create_instance = __create_e1000_driver_t_instance
};


void e1000_interrupt_handler(void *ctx){
    drivers::e1000_driver_t *driver = (drivers::e1000_driver_t *)ctx;
    driver->handle_interrupt();
}

int e1000_vnode_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    drivers::e1000_driver_t *driver = (drivers::e1000_driver_t *)this_node->file_identifier;
    return driver->send((void *)buffer, length);
}

namespace drivers{

    e1000_driver_t::e1000_driver_t(pci::pci_device_header* hdr) : base_driver_t(hdr){

    }

    e1000_driver_t::~e1000_driver_t(){

    }

    bool e1000_driver_t::init_device(){
        this->base_address = pci::get_device_bar(this->pci_device_hdr, 0);

        return true;
    }
    

    bool e1000_driver_t::start_device(){
        this->pci_device_hdr->command |= (PCI_CMD_BUS_MASTER | PCI_CMD_MEMORY); // Enable Bus Mastering and Memory access

        this->_reset_nic();
        this->_setup_receive_ring();
        this->_setup_transmit_ring();

        pci::register_isr(e1000_interrupt_handler, this);
        this->_enable_interrupts();


        kprintf("\e[0;32m[E1000]\e[0m Driver started!\n");

        int timeout = 3000;
        while (!(this->_read_register(E1000_REG_STATUS) & E1000_BIT_STATUS_LU) && timeout > 0) {
            Sleep(100);
            timeout -= 100;
        }
        

        int i = 0;
        char path[64];

        while (1){
            stringf(path, sizeof(path), "/dev/eth%d", i);
            vnode_t *node = vfs::resolve_path(path);

            if (!node) break;

            node->close();
            i++;
        }

        vnode_t *node = vfs::create_path(path, VCHR);
        node->file_identifier = (uint64_t)this;
        node->file_operations.write = e1000_vnode_write;


        this->id = nic_common::register_nic(node, this->mac);
        node->inode = this->id;
        
        node->close();

        nic_common::refresh_nic((this->_read_register(E1000_REG_STATUS) & E1000_BIT_STATUS_LU) != 0, this->id);
        return true;
    }

    bool e1000_driver_t::shutdown_device(){
        return true;
    }


    void e1000_driver_t::_write_register(uint16_t reg, uint32_t value){
        *(volatile uint32_t *)(this->base_address + reg) = value;
    }

    uint32_t e1000_driver_t::_read_register(uint16_t reg){
        return *(volatile uint32_t *)(this->base_address + reg);
    }

    uint16_t e1000_driver_t::_eeprom_read(uint8_t address){
        uint32_t tmp;
        uint16_t data;

        if((this->_read_register(E1000_REG_EECD) & E1000_BIT_EECD_EE_PRES) == 0) {
            kprintf("[E1000] EEPROM present bit is not set for i8254x\n");
            return 0;
        }

        /* Tell the EEPROM to start reading */
        if(this->pci_device_hdr->device_id == 0x1013
        || this->pci_device_hdr->device_id == 0x1018
        || this->pci_device_hdr->device_id == 0x1076
        || this->pci_device_hdr->device_id == 0x1077
        || this->pci_device_hdr->device_id == 0x1078) {
            /* Specification says that only 82541x devices and the
            * 82547GI/EI do 2-bit shift */
            tmp = ((uint32_t)address & 0xfff) << 2;
        } else {
            tmp = ((uint32_t)address & 0xff) << 8;
        }
        tmp |= E1000_BIT_EERD_START;
        this->_write_register(E1000_REG_EERD, tmp);

        /* Wait until the read is finished - then the DONE bit is cleared */
        int timeout = 100;
        while (timeout > 0 && (this->_read_register(E1000_REG_EERD) & E1000_BIT_EERD_DONE) == 0){
            timeout--;
            Sleep(1);
        }

        /* Obtain the data */
        data = (uint16_t)(this->_read_register(E1000_REG_EERD) >> 16);

        /* Tell EEPROM to stop reading */
        tmp = this->_read_register(E1000_REG_EERD);
        tmp &= ~(uint32_t)E1000_BIT_EERD_START;
        this->_write_register(E1000_REG_EERD, tmp);
        return data;
    }

    void e1000_driver_t::_setup_transmit_ring(){
        size_t transmit_ring_size = NUM_OF_TX_DESCRIPTORS * 16;
        this->transmit_ring = (transmit_descriptor_t *)GlobalAllocator.RequestPages(DIV_ROUND_UP(transmit_ring_size, PAGE_SIZE));
        uint64_t ring_physical = virtual_to_physical((uint64_t)this->transmit_ring);
        memset(this->transmit_ring, 0, transmit_ring_size);

        // Map it as uncachable
        for (size_t i = 0; i < transmit_ring_size; i += 0x1000){
            globalPTM.SetFlag((void*)((uint64_t)this->transmit_ring + i), PT_Flag::CacheDisable, true);
        }

        for (int i = 0; i < NUM_OF_TX_DESCRIPTORS; i++){
            transmit_descriptor_t* descriptor = transmit_ring + i;

            void *buffer = GlobalAllocator.RequestPages(DIV_ROUND_UP(SIZE_OF_TX_DESCRIPTOR_BUFFER, PAGE_SIZE));
            descriptor->buffer_address = virtual_to_physical((uint64_t)buffer);
            descriptor->status = E1000_TX_STS_DD;

            for (size_t i = 0; i < SIZE_OF_TX_DESCRIPTOR_BUFFER; i += 0x1000){
                globalPTM.SetFlag((void*)((uint64_t)buffer + i), PT_Flag::CacheDisable, true);
            }
        }

        this->_write_register(E1000_REG_TDBAL, ring_physical & 0xFFFFFFFF);
        this->_write_register(E1000_REG_TDBAH, ring_physical >> 32);
        this->_write_register(E1000_REG_TDLEN, transmit_ring_size);

        this->_write_register(E1000_REG_TDH, 0);
        this->_write_register(E1000_REG_TDT, 0);

        this->_write_register(E1000_REG_TIPG, (10) | (8 << 10) | (6 << 20));
        
        // Set the Enable (EN) and Pad Short Packets (PSP) bits
        uint32_t tctl = E1000_BIT_TCTL_EN | E1000_BIT_TCTL_PSP;
        this->_write_register(E1000_REG_TCTL, tctl);
    }

    
    void e1000_driver_t::_setup_receive_ring(){
        size_t receive_ring_size = NUM_OF_RX_DESCRIPTORS * 16;
        this->receive_ring = (receive_descriptor_t *)GlobalAllocator.RequestPages(DIV_ROUND_UP(receive_ring_size, PAGE_SIZE));
        uint64_t ring_physical = virtual_to_physical((uint64_t)this->receive_ring);
        memset(this->receive_ring, 0, receive_ring_size);

        
        // Map it as uncachable
        for (size_t i = 0; i < receive_ring_size; i += 0x1000){
            globalPTM.SetFlag((void*)((uint64_t)this->receive_ring + i), PT_Flag::CacheDisable, true);
        }

        for (int i = 0; i < NUM_OF_RX_DESCRIPTORS; i++){
            receive_descriptor_t* descriptor = receive_ring + i;
            void *buffer = GlobalAllocator.RequestPages(DIV_ROUND_UP(SIZE_OF_RX_DESCRIPTOR_BUFFER, PAGE_SIZE));
            descriptor->buffer_address = virtual_to_physical((uint64_t)buffer);
            for (size_t i = 0; i < SIZE_OF_RX_DESCRIPTOR_BUFFER; i += 0x1000){
                globalPTM.SetFlag((void*)(descriptor->buffer_address + i), PT_Flag::CacheDisable, true);
            }
        }

        this->_write_register(E1000_REG_RDBAL, ring_physical & 0xFFFFFFFF); // Base Address Low
        this->_write_register(E1000_REG_RDBAH, ring_physical >> 32); // Base Address High
        this->_write_register(E1000_REG_RDLEN, receive_ring_size); // Ring Size

        this->_write_register(E1000_REG_RDH, 0); // Set it to the first descriptor
        this->_write_register(E1000_REG_RDT, NUM_OF_RX_DESCRIPTORS - 1); // Set it to the last descriptor

        // Set the Enable, Long Packet Reception, Broadcast Accept Mode and Size Extenstion bits
        // Also set the buffer size. This configuration (BSIZE = 0b11 and BSEX = 1) means 4096 (4kB) buffers
        uint32_t rctl = E1000_BIT_RCTL_EN | E1000_BIT_RCTL_LPE | E1000_BIT_RCTL_BAM | E1000_BIT_RCTL_BSEX | E1000_BIT_RCTL_BSIZE(0b11);
        this->_write_register(E1000_REG_RCTL, rctl);
    }

    void e1000_driver_t::_enable_interrupts(){
        this->_read_register(E1000_REG_ICR);

        uint32_t ims = E1000_BIT_IM_RXT | E1000_BIT_IM_RXO | E1000_BIT_IM_LSC;
        this->_write_register(E1000_REG_IMS, ims);
    }

    void e1000_driver_t::_reset_nic(){
        uint32_t device_control = this->_read_register(E1000_REG_CTRL);
        device_control |= E1000_BIT_CTRL_RST;
        this->_write_register(E1000_REG_CTRL, device_control);

        while (this->_read_register(E1000_REG_CTRL) & E1000_BIT_CTRL_RST) __asm__ ("hlt");

        device_control = this->_read_register(E1000_REG_CTRL);
        device_control |= E1000_BIT_CTRL_ASDE | E1000_BIT_CTRL_SLU; // Enable Auto Speed Detection.
        this->_write_register(E1000_REG_CTRL, device_control);

        uint16_t b0 = _eeprom_read(0);
        uint16_t b1 = _eeprom_read(1);
        uint16_t b2 = _eeprom_read(2);

        this->mac[0] = b0 & 0xFF;
        this->mac[1] = b0 >> 8;
        this->mac[2] = b1 & 0xFF;
        this->mac[3] = b1 >> 8;
        this->mac[4] = b2 & 0xFF;
        this->mac[5] = b2 >> 8;

        uint32_t writeL = ((uint32_t)b1 << 16) | b0;
        uint32_t writeH = b2 | 0x80000000; // Also set the AV bit?

        this->_write_register(E1000_REG_RAL(0), writeL);
        this->_write_register(E1000_REG_RAH(0), writeH);

        kprintf("\e[0;33m[E1000]\e[0m Mac Address: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", 
            this->mac[0], this->mac[1], this->mac[2], this->mac[3], this->mac[4], this->mac[5]);
    }

    void e1000_driver_t::_write_data(void* data, uint32_t size, bool EOP){
        uint32_t tail = this->_read_register(E1000_REG_TDT);
        transmit_descriptor_t* tx = transmit_ring + tail; // Get the descriptor the tail is pointing at (next available descriptor)

        while (!(tx->status & E1000_TX_STS_DD)){
            __asm__ ("pause");
        }

        memcpy((void*)physical_to_virtual(tx->buffer_address), data, size); // Copy the data to the previously allocated buffer

        tx->length = size; // Set the length of the descriptor

        tx->status = 0;
        tx->command = E1000_TX_CMD_RS;
        if (EOP) tx->command |= E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS; // If its the last one, set EOP
        tail = (tail + 1) % NUM_OF_TX_DESCRIPTORS;
        this->_write_register(E1000_REG_TDT, tail); // Increment and write the tail
    }

    size_t e1000_driver_t::send(void* data, size_t length){
        uint32_t rflags = spin_lock(&this->transmit_lock);

        size_t sent = 0;
        // split the data into chunks and send them
        for (; sent < length;){
            int to_send = min(length - sent, SIZE_OF_TX_DESCRIPTOR_BUFFER);
            this->_write_data((void*)((uint64_t)data + sent), to_send, to_send == (length - sent));
            sent += to_send;
        }
        
        spin_unlock(&this->transmit_lock, rflags);
        return sent;
    }

    void e1000_driver_t::receive_packets(){
        uint32_t rflags = spin_lock(&this->receive_lock);

        uint32_t idx = rx_next;

        void* buffer = nullptr; // use this to store the buffer.
        size_t buffer_len = 0; 

        while (receive_ring[idx].status & E1000_RX_STS_DD) {
            // This descriptor has been filled
            
            bool eop = receive_ring[idx].status & E1000_RX_STS_EOP;
            uint16_t len = receive_ring[idx].length;
            void* data = (void*)physical_to_virtual(receive_ring[idx].buffer_address);
            
            // Handle multiple-descriptor packets
            if (buffer == nullptr){ // This is the first descriptor of the packet
                buffer = malloc(len); // use your kernel's heap allocator
                buffer_len = len;
                memcpy(buffer, data, len);
            }else{
                // Its the next part of the packet, add it to the packet
                void* new_buffer = malloc(buffer_len + len); // allocate a bigger buffer
                memcpy(new_buffer, buffer, buffer_len); // copy the previous data
                free(buffer); // free the old buffer
        
                // copy the new data
                memcpy((void*)((uint64_t)new_buffer + buffer_len), data, len);
                
                // Set the new buffer into the variables
                buffer_len += len;
                buffer = new_buffer;
            }
        
            // Set status to 0 (To give ownership back to the controller)
            receive_ring[idx].status = 0; 

            idx = (idx + 1) % NUM_OF_RX_DESCRIPTORS;

            if (eop) {
                // This is the last descriptor of the packet
                // Forward the packet to your network stack

                nic_common::nic_handle_packet(this->id, buffer, buffer_len);

                buffer = nullptr;
                buffer_len = 0;
            }
        }

        // Give the controller more free descriptors by updating RDT
        uint32_t tail = (idx == 0) ? NUM_OF_RX_DESCRIPTORS - 1 : idx - 1;
        this->_write_register(E1000_REG_RDT, tail);

        rx_next = idx;

        spin_unlock(&this->receive_lock, rflags);
    }

    void e1000_driver_t::handle_interrupt(){
        uint32_t cause = this->_read_register(E1000_REG_ICR); // Cleared uppon read

        if (cause & E1000_BIT_IM_RXT) { // Packets received
            this->receive_packets();  // Call the function responsible for receiving
                                      // packets and sending them to the network stack
        }

        if (cause & E1000_BIT_IM_LSC){ // link status change
            // Read the status register and check the LU bit to get the link status
            if (this->_read_register(E1000_REG_STATUS) & E1000_BIT_STATUS_LU) {
                //kprintf("Link change detected: Link up!\n");
                nic_common::refresh_nic(true, this->id);
            } else {
                //kprintf("Link change detected: Link down!\n");
                nic_common::refresh_nic(false, this->id);
            }
        }
    }
}


