#pragma once
#include <pci.h>
#include <memory.h>
#include <memory/heap.h>
#include <paging/PageFrameAllocator.h>
#include <drivers/storage/nvme/nvme_regs.h>
#include <filesystem/vfs/vfs.h>
#include <drivers/drivers.h>

namespace drivers{
    class nvme_namespace_t;

    class nvme_driver_t : public base_driver_t {
        public:
        nvme_driver_t(pci::pci_device_header* hdr);
        ~nvme_driver_t();

        bool init_device();
        bool start_device();
        bool shutdown_device();

        int _identify_namespace(int identifier, void* buffer);
        int _send_io_command(nvme_submission_entry_t* entry);

        
        private:
        uint32_t _read_register(uint32_t register_offset);
        uint64_t _read_registerq(uint32_t register_offset);
        void _write_register(uint32_t register_offset, uint32_t value);
        void _write_registerq(uint32_t register_offset, uint64_t value);

        void _log_caps();
        void _log_controller_status();

        bool _disable_controller();
        bool _enable_controller();

        void _create_admin_queues();
        bool _create_io_queues();
        
        int _send_admin_command(nvme_submission_entry_t* entry);

        void _identify_controller();
        void _initialize_namespaces();

        public:
        char model_name[41];
        char serial_number[25];

        uint32_t max_data_transfer_size; // MDTS
        
        vnode_t* vnode;

        uint64_t min_page_size;
        uint64_t max_page_size;

        private:
        pci::PCIHeader0* device_header;
        uint64_t base_address;

        int nvme_driver_id;

        nvme_cap_t caps;

        nvme_queue_set admin_queue;
        nvme_queue_set io_queue; // We will use a single I/O queue for simplicity

        nvme_namespace_t* namespace_list;
        nvme_namespace_t* last_namespace_entry; // For simplicity
    };

    class nvme_namespace_t{
        public:
        nvme_namespace_t(int identifier, nvme_driver_t* driver);
        ~nvme_namespace_t();

        bool create_prps(nvme_submission_entry_t* cmd, void* buffer, size_t size);
        void free_prps(nvme_submission_entry_t* cmd, uint64_t size);

        bool read(uint64_t lba_start, uint32_t lba_count, void* buffer);
        bool write(uint64_t lba_start, uint32_t lba_count, const void* buffer);
        
        public:
        nvme_driver_t* driver;
        nvme_namespace_t* next;

        uint16_t lba_size;
        uint64_t amount_of_lbas; // In LBAs 

        uint64_t min_page_size;
        uint64_t max_page_size;
        uint32_t max_data_transfer_size;

        vnode_t* vnode;
        private:
        int identifier;
    };
}