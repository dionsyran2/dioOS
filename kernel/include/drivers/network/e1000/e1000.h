#pragma once
#include <stdint.h>
#include <stddef.h>
#include <drivers/drivers.h>

#include <scheduling/spinlock/spinlock.h>
#include <drivers/network/e1000/e1000_structs.h>


namespace drivers{
    class e1000_driver_t : public base_driver_t{
        public:
        e1000_driver_t(pci::pci_device_header* hdr);
        ~e1000_driver_t();

        bool init_device();
        bool start_device();
        bool shutdown_device();

        size_t send(void* data, size_t length);
        void handle_interrupt();

        private:
        void _write_register(uint16_t reg, uint32_t value);
        uint32_t _read_register(uint16_t reg);
        uint16_t _eeprom_read(uint8_t address);

        void _reset_nic();
        void _setup_transmit_ring();
        void _setup_receive_ring();
        void _enable_interrupts();

        void _write_data(void* data, uint32_t size, bool EOP);
        void receive_packets();
        
        
        public:
        

        private:
        uint64_t id = 0;
        
        uint64_t base_address;
        uint8_t mac[6];
        uint8_t rx_next = 0;

        transmit_descriptor_t *transmit_ring;
        receive_descriptor_t *receive_ring;

        spinlock_t transmit_lock;
        spinlock_t receive_lock;
    };
}