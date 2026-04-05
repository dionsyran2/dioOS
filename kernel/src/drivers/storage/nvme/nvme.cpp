#include <drivers/storage/nvme/nvme.h>
#include <drivers/storage/nvme/nvme_commands.h>
#include <drivers/timers/common.h>
#include <partitions/partitions.h>
#include <kerrno.h>
#include <kstdio.h>
#include <cstr.h>
#include <math.h>
#include <CONFIG.h>

int nvme_driver_count = 0; // Used for node creation (nvme0, nvme1, ...)

bool __nvme_driver_t_supports_device(pci::pci_device_header* device){
    if (device->class_code == 1 && device->subclass == 8) return true;
    return false;
}

base_driver_t* __create_nvme_driver_t_instance(pci::pci_device_header* header){
    return (base_driver_t*)(new drivers::nvme_driver_t(header));
}

DEFINE_DRIVER(nvme_driver_class) = {
    .name = "NVME driver",
    .supports_device = __nvme_driver_t_supports_device,
    .create_instance = __create_nvme_driver_t_instance
};

namespace drivers{

    uint32_t nvme_driver_t::_read_register(uint32_t register_offset){
        volatile uint32_t* reg = (volatile uint32_t*)(this->base_address + register_offset);
        return *reg;
    }

    uint64_t nvme_driver_t::_read_registerq(uint32_t register_offset){
        volatile uint64_t* reg = (volatile uint64_t*)(this->base_address + register_offset);
        return *reg;
    }

    void nvme_driver_t::_write_register(uint32_t register_offset, uint32_t value){
        volatile uint32_t* reg = (volatile uint32_t*)(this->base_address + register_offset);
        *reg = value;
    }

    void nvme_driver_t::_write_registerq(uint32_t register_offset, uint64_t value){
        volatile uint64_t* reg = (volatile uint64_t*)(this->base_address + register_offset);
        *reg = value;
    }

    void nvme_driver_t::_log_caps(){
        #ifdef NVME_LOGGING
        kprintf("---------- NVME Capabilities ----------\n");
        kprintf("Max queue entries: %d\n", caps.bits.max_queue_entries_supported);
        kprintf("Contiguous queues required: %d\n", caps.bits.contiguous_queues_required);
        kprintf("Arbitration mechanism supported: %d\n", caps.bits.arbitration_mechanism_supported);
        kprintf("Timeout: %d ms\n", caps.bits.timeout * 500);
        kprintf("Doorbell stride: %d\n", caps.bits.doorbell_stride);
        kprintf("NVM subsystem reset supported: %d\n", caps.bits.nvm_subsystem_reset_supported);
        kprintf("Command sets supported: %s %s\n",
            caps.bits.command_sets_supported & NVME_CAPS_SET_ADMIN ? "Admin" : "",
            caps.bits.command_sets_supported & NVME_CAPS_SET_NVM   ? "NVM" : ""
        );
        kprintf("Boot partition support: %d\n", caps.bits.boot_partition_support);
        kprintf("Controller power scope: %d\n", caps.bits.controller_power_scope);
        kprintf("Memory page size min: %d\n", this->min_page_size);
        kprintf("Memory page size max: %d\n", this->max_page_size);
        kprintf("Persistant memory region supported: %d\n", caps.bits.persistant_memory_region_supported);
        kprintf("Controller memory buffer supported: %d\n", caps.bits.controller_memory_buffer_supported);
        kprintf("NVM subsystem shutdown supported: %d\n", caps.bits.nvm_subsystem_shutdown_supported);
        kprintf("Controller ready with media support: %d\n\n", caps.bits.controller_ready_with_media_support);
        #endif
    }

    void nvme_driver_t::_log_controller_status(){
        #ifdef NVME_LOGGING
        uint32_t csts = _read_register(NVME_CSTS_REG);

        kprintf("---------- NVME Status ----------\n");
        kprintf("Ready: %d\n", csts & NVME_CSTS_RDY ? 1 : 0);
        kprintf("Fatal: %d\n", csts & NVME_CSTS_CFS ? 1 : 0);
        kprintf("Shutdown status: %d\n", (csts &  NVME_CSTS_SHST) >> 2);
        kprintf("NVM subsystem reset: %d\n", csts & NVME_CSTS_NSSRO ? 1 : 0);
        kprintf("Processing Paused: %d\n\n", csts & NVME_CSTS_PP ? 1 : 0);
        #endif
    }

    bool nvme_driver_t::init_device(){
        uint64_t cmd = this->device_header->Header.command;
        cmd |= PCI_CMD_BUS_MASTER | PCI_CMD_MEMORY; // Enable bus mastering & memory space
        cmd &= ~PCI_CMD_INTERRUPT_DISABLE; // Enable Interrupts
        this->device_header->Header.command = cmd;

        this->base_address = pci::get_device_bar(this->pci_device_hdr, 0);

        caps.raw = _read_registerq(NVME_CAP_REG); // Read the capabilities
        this->min_page_size = 1 << (12 + caps.bits.memory_page_size_min);
        this->max_page_size = 1 << (12 + caps.bits.memory_page_size_max);

        _log_caps();

        // Disable the controller
        _disable_controller();

        // Create the admin queues
        // (They are used to create I/O queues and namespace identification)
        _create_admin_queues();

        // Fetch the current count and increase it.
        this->nvme_driver_id = __atomic_fetch_add(&nvme_driver_count, 1, __ATOMIC_SEQ_CST);

        // Create a node to represent this device
        // Currently no I/O operations are supported on this device directly
        char pathname[128];

        stringf(pathname, sizeof(pathname), "/dev/nvme%d", this->nvme_driver_id);
        this->vnode = vfs::create_path(pathname, VBLK);
        memset(&this->vnode->file_operations, 0, sizeof(this->vnode->file_operations));

        this->vnode->close();        

        kprintf("\e[0;32m[NVMe]\e[0m Driver initialized successfully!\n");
        return true;
    }

    bool nvme_driver_t::start_device(){
        _enable_controller();
        _log_controller_status();

        _identify_controller();

        _create_io_queues();

        _initialize_namespaces();
        return true;
    }

    bool nvme_driver_t::shutdown_device(){
        _disable_controller();
        return true;
    }

    bool nvme_driver_t::_disable_controller(){
        uint32_t csts = _read_register(NVME_CSTS_REG);
        uint32_t cc = _read_register(NVME_CC_REG);

        // Clear the EN bit
        cc &= ~NVME_CC_EN;

        // Write back the value
        _write_register(NVME_CC_REG, cc);

        uint64_t timeout = caps.bits.timeout * 500;
        
        // Wait until the RDY bit is 0
        while (csts & NVME_CSTS_RDY){
            Sleep(10);
            timeout -= 10;

            csts = _read_register(NVME_CSTS_REG);

            if (timeout == 0) break;
        }

        csts = _read_register(NVME_CSTS_REG);
        if (csts & NVME_CSTS_RDY) return false;

        return true;
    }

    bool nvme_driver_t::_enable_controller(){
        uint32_t csts = _read_register(NVME_CSTS_REG);
        uint32_t cc = _read_register(NVME_CC_REG);

        // Set the EN bit
        cc |= NVME_CC_EN;

        // Write back the value
        _write_register(NVME_CC_REG, cc);

        uint64_t timeout = caps.bits.timeout * 500;
        
        // Wait until the RDY bit is set
        while ((csts & NVME_CSTS_RDY) == 0){
            Sleep(10);
            timeout -= 10;

            csts = _read_register(NVME_CSTS_REG);

            if (timeout == 0) break;
        }

        csts = _read_register(NVME_CSTS_REG);
        if ((csts & NVME_CSTS_RDY) == 0) return false;

        return true;
    }

    bool nvme_driver_t::_create_io_queues(){
        nvme_queue_set* io_q = &this->io_queue;

        memset(io_q, 0, sizeof(nvme_queue_set));

        // Allocate the queues
        io_q->submission_queue = (nvme_submission_entry_t*)GlobalAllocator.RequestPage();
        io_q->completion_queue = (nvme_completion_entry_t*)GlobalAllocator.RequestPage();

        // Get Physical Addresses
        uint64_t sq_phys = virtual_to_physical((uint64_t)io_q->submission_queue);
        uint64_t cq_phys = virtual_to_physical((uint64_t)io_q->completion_queue);

        // Clear memory
        memset(io_q->submission_queue, 0, PAGE_SIZE);
        memset(io_q->completion_queue, 0, PAGE_SIZE);

        // Set Queue Depth (e.g., 256 entries)
        io_q->submission_queue_size = PAGE_SIZE / sizeof(nvme_submission_entry_t); 
        io_q->completion_queue_size = PAGE_SIZE / sizeof(nvme_completion_entry_t);
        
        // Setup internal tracking
        io_q->submission_queue_tail = 0;
        io_q->completion_queue_head = 0;
        io_q->completion_queue_phase = 1;
        io_q->queue_id = 1; // ID 0 is Admin, so we use 1

        io_q->command_id = 0;
        // Send Admin Command: Create Completion Queue (CQ)
        nvme_submission_entry_t create_cq_cmd;
        memset(&create_cq_cmd, 0, sizeof(create_cq_cmd));
        
        create_cq_cmd.command = NVME_ADMIN_CMD_CREATE_CQ;
        create_cq_cmd.data_pointers[0] = cq_phys;
        create_cq_cmd.cmd_specific[0] = (io_q->completion_queue_size - 1) << 16 | (1); // Size | Queue ID
        create_cq_cmd.cmd_specific[1] = 3;
        
        // Send to ADMIN queue
        if (_send_admin_command(&create_cq_cmd) != 0) {
            kprintf("NVMe: Failed to create I/O CQ\n");
            return false;
        }

        // Send Admin Command: Create Submission Queue (SQ)
        nvme_submission_entry_t create_sq_cmd;
        memset(&create_sq_cmd, 0, sizeof(create_sq_cmd));
        
        create_sq_cmd.command = NVME_ADMIN_CMD_CREATE_SQ;
        create_sq_cmd.data_pointers[0] = sq_phys;
        create_sq_cmd.cmd_specific[0] = (io_q->submission_queue_size - 1) << 16 | (1); // Size | Queue ID
        create_sq_cmd.cmd_specific[1] = (1 << 16) | 1;
        
        if (_send_admin_command(&create_sq_cmd) != 0) {
            kprintf("NVMe: Failed to create I/O SQ\n");
            return false;
        }
        return true;
    }

    void nvme_driver_t::_create_admin_queues(){
        nvme_aqa_t queue_attributes;
        queue_attributes.raw = 0;

        // Create the submission queue
        this->admin_queue.submission_queue = (nvme_submission_entry_t*)GlobalAllocator.RequestPage();
        uint64_t submission_queue_physical = virtual_to_physical((uint64_t)this->admin_queue.submission_queue);

        memset(this->admin_queue.submission_queue, 0, PAGE_SIZE);

        // Set the size of the submission queue (in entries, 0 based)
        this->admin_queue.submission_queue_size = (PAGE_SIZE / sizeof(nvme_submission_entry_t));
        queue_attributes.bits.asq_depth = admin_queue.submission_queue_size - 1;

        // Create the completion queue
        this->admin_queue.completion_queue = (nvme_completion_entry_t*)GlobalAllocator.RequestPage();
        uint64_t completion_queue_physical = virtual_to_physical((uint64_t)this->admin_queue.completion_queue);

        memset(this->admin_queue.completion_queue, 0, PAGE_SIZE);

        // Set the size of the completion queue (in entries, 0 based)
        this->admin_queue.completion_queue_size = (PAGE_SIZE / sizeof(nvme_completion_entry_t));
        queue_attributes.bits.acq_depth = this->admin_queue.completion_queue_size - 1;


        // Write the registers
        _write_register(NVME_AQA_REG, queue_attributes.raw);
        _write_registerq(NVME_ASQ_REG, submission_queue_physical);
        _write_registerq(NVME_ACQ_REG, completion_queue_physical);

        // Configure the length of the entries, and the command set
        uint32_t cc = NVME_CC_COMMAND_SET_NVM;
        cc |= NVME_CC_IO_SQES(log2(sizeof(nvme_submission_entry_t)));
        cc |= NVME_CC_IO_CQES(log2(sizeof(nvme_completion_entry_t)));

        _write_register(NVME_CC_REG, cc);

        // Update internal variables
        this->admin_queue.submission_queue_tail = 0;
        this->admin_queue.completion_queue_head = 0;
        this->admin_queue.completion_queue_phase = 1;
    }

    int nvme_driver_t::_send_admin_command(nvme_submission_entry_t* entry){
        entry->command &= 0xFF;
        entry->command |= this->admin_queue.command_id << 16;

        memcpy(&admin_queue.submission_queue[admin_queue.submission_queue_tail], entry, sizeof(nvme_submission_entry_t));
        this->admin_queue.submission_queue_tail++;
        asm volatile("clflush (%0)" :: "r"(&admin_queue.submission_queue[admin_queue.submission_queue_tail])); // Flush the cache

        int cmd_id = this->admin_queue.command_id;
        this->admin_queue.command_id++;
        if (this->admin_queue.submission_queue_tail >= this->admin_queue.submission_queue_size) admin_queue.submission_queue_tail = 0;

        _write_register(NVME_SQTDBL(0, caps.bits.doorbell_stride), admin_queue.submission_queue_tail);
        bool intr = are_interrupts_enabled();
        asm ("sti");

        while (true) {
            volatile nvme_completion_entry_t* cqe = &this->admin_queue.completion_queue[this->admin_queue.completion_queue_head];
            
            uint8_t p = cqe->phase_status & 1; // Phase is at bit 1 of this field

            if (p == this->admin_queue.completion_queue_phase && cqe->command_identifier == cmd_id){
                this->admin_queue.completion_queue_head++;
                if (this->admin_queue.completion_queue_head >= this->admin_queue.completion_queue_size){
                    this->admin_queue.completion_queue_head = 0;
                    this->admin_queue.completion_queue_phase = !this->admin_queue.completion_queue_phase;
                }

                // Ring Completion Doorbell (Tell HW we processed this slot)
                _write_register(NVME_CQHDBL(0, caps.bits.doorbell_stride), this->admin_queue.completion_queue_head);
                
                if (!intr) asm ("cli");
                return (cqe->phase_status >> 1);
            }
            
            asm volatile("hlt"); // Don't use full power :). I/O is slow
        }
        if (!intr) asm ("cli");
    }

    int nvme_driver_t::_send_io_command(nvme_submission_entry_t* entry){
        // Acquire the lock so we dont mess up the queue
        uint64_t rflags = spin_lock(&io_queue.submission_lock);

        entry->command &= 0xFF;
        entry->command |= this->io_queue.command_id << 16;

        memcpy(&io_queue.submission_queue[io_queue.submission_queue_tail], entry, sizeof(nvme_submission_entry_t));
        this->io_queue.submission_queue_tail++;
        asm volatile("clflush (%0)" :: "r"(&io_queue.submission_queue[io_queue.submission_queue_tail])); // Flush the cache

        int cmd_id = this->io_queue.command_id;
        this->io_queue.command_id++;
        if (this->io_queue.submission_queue_tail >= this->io_queue.submission_queue_size) io_queue.submission_queue_tail = 0;

        _write_register(NVME_SQTDBL(this->io_queue.queue_id, caps.bits.doorbell_stride), io_queue.submission_queue_tail);

        // Release after we hit the doorbell and wait for a response
        spin_unlock(&io_queue.submission_lock, rflags);
        bool intr = are_interrupts_enabled();
        asm ("sti");

        while (true) {
            volatile nvme_completion_entry_t* cqe = &this->io_queue.completion_queue[this->io_queue.completion_queue_head];
            
            uint8_t p = cqe->phase_status & 1; // Phase is at bit 1 of this field

            if (p == this->io_queue.completion_queue_phase && cqe->command_identifier == cmd_id){
                // Acquire the lock so we dont mess up the queue head
                uint64_t flags = spin_lock(&io_queue.completion_lock);

                this->io_queue.completion_queue_head++;
                if (this->io_queue.completion_queue_head >= this->io_queue.completion_queue_size){
                    this->io_queue.completion_queue_head = 0;
                    this->io_queue.completion_queue_phase = !this->io_queue.completion_queue_phase;
                }

                // Ring Completion Doorbell (Tell HW we processed this slot)
                _write_register(NVME_CQHDBL(this->io_queue.queue_id, caps.bits.doorbell_stride), this->io_queue.completion_queue_head);
                
                spin_unlock(&io_queue.completion_lock, flags);
                if (!intr) asm ("cli");
                return ((cqe->phase_status >> 1) & 0xFF);
            }

            asm volatile("hlt"); // Don't use full power :). I/O is slow
        }
        if (!intr) asm ("cli");
    }

    // @brief It will fetch data like the model and serial number
    void nvme_driver_t::_identify_controller(){
        nvme_identify_controller_t* response = (nvme_identify_controller_t*)GlobalAllocator.RequestPage();

        nvme_submission_entry_t entry;
        // Clear the entry
        memset(&entry, 0, sizeof(entry));

        // Set the command
        entry.command = NVME_ADMIN_CMD_IDENTIFY;

        // The buffer to place the response
        entry.data_pointers[0] = virtual_to_physical((uint64_t)response);

        // cmd_specific[0] for identify (The low byte) is the type (what to identify)
        // 0 - a namespace, 1 - the controller, 2 - the namespace list.
        entry.cmd_specific[0] = NVME_ADMIN_IDENTIFY_PARAM_CONTROLLER;

        int ret = _send_admin_command(&entry);
        
        if (ret != 0){
            kprintf("\e[0;31m[NVMe]\e[0m Identify(Controller) command failed with status: %d\n", ret);
            goto cleanup;
        }

        
        memcpy(this->serial_number, response->serial_number, 24);
        this->serial_number[24] = '\0';
        
        memcpy(this->model_name, response->model_number, 40);
        this->model_name[40] = '\0';

        this->max_data_transfer_size = (1U << (12 + response->mdts));

        kprintf("[NVMe] Serial Number: %s\n", this->serial_number);
        kprintf("[NVMe] Model Number: %s\n", this->model_name);
        kprintf("[NVMe] Max data transfer size: %d\n", this->max_data_transfer_size);
    cleanup:
        GlobalAllocator.FreePage(response);
        return;
    }

    int nvme_driver_t::_identify_namespace(int identifier, void* buffer){
        nvme_submission_entry_t entry;
        // Clear the entry
        memset(&entry, 0, sizeof(entry));

        // Set the command
        entry.command = NVME_ADMIN_CMD_IDENTIFY;

        // The buffer to place the response
        entry.data_pointers[0] = virtual_to_physical((uint64_t)buffer);

        // cmd_specific[0] for identify (The low byte) is the type (what to identify)
        // 0 - a namespace, 1 - the controller, 2 - the namespace list.
        entry.cmd_specific[0] = NVME_ADMIN_IDENTIFY_PARAM_NS;

        entry.namespace_identifier = identifier;

        return _send_admin_command(&entry);
    }

    void nvme_driver_t::_initialize_namespaces(){
        volatile uint32_t* response = (volatile uint32_t*)GlobalAllocator.RequestPage();
        
        nvme_submission_entry_t entry;
        // Clear the entry
        memset(&entry, 0, sizeof(entry));

        // Set the command
        entry.command = NVME_ADMIN_CMD_IDENTIFY;

        // The buffer to place the response
        entry.data_pointers[0] = virtual_to_physical((uint64_t)response);

        // cmd_specific[0] for identify (The low byte) is the type (what to identify)
        entry.cmd_specific[0] = NVME_ADMIN_IDENTIFY_PARAM_NSLIST;

        // The controller will fill the buffer as a uint32_t array, with all of the valid namespace ids
        // The last entry is 0, to signify the end
        int ret = _send_admin_command(&entry);
        if (ret != 0){
            kprintf("\e[0;31m[NVMe]\e[0m Identify(Namespace List) command failed with status: %d\n", ret);
            goto cleanup;
        }

        for (volatile uint32_t* ns = response; *ns != 0; ns++){
            // Create a new nvme namespace
            nvme_namespace_t* nvme_namespace = new nvme_namespace_t(*ns, this);
            nvme_namespace->next = nullptr;


            // Add it to our list
            if (this->namespace_list == nullptr){
                this->namespace_list = nvme_namespace;
            } else {
                this->last_namespace_entry->next = nvme_namespace;
            }

            this->last_namespace_entry = nvme_namespace;
        }

        cleanup:
        GlobalAllocator.FreePage((void*)response);
        return;
    }
    
    

    nvme_driver_t::nvme_driver_t(pci::pci_device_header* hdr) : base_driver_t(hdr) {
        this->device_header = (pci::PCIHeader0*)hdr;
        this->namespace_list = nullptr;
    }

    nvme_driver_t::~nvme_driver_t(){

    }

    // -------- nvme_namespace_t --------
    int nvme_ns_vnode_read(uint64_t offset, uint64_t length, void* out, vnode_t* this_node){
        // Calculate the lbas
        uint64_t byte_offset = offset % this_node->io_block_size;
        uint64_t start_lba = offset / this_node->io_block_size;
        uint64_t total_mem = ROUND_UP(length + byte_offset, this_node->io_block_size);
        uint64_t lba_count = total_mem / this_node->io_block_size;

        // Get the namespace
        nvme_namespace_t* ns = (nvme_namespace_t*)this_node->fs_identifier;
        if (ns == nullptr) return -EFAULT;

        // Check if its out of bounds
        if (start_lba > ns->amount_of_lbas || (start_lba + lba_count) > ns->amount_of_lbas) return EOF;

        // Allocate the memory
        uint64_t pages = DIV_ROUND_UP(ROUND_UP(total_mem, ns->min_page_size), PAGE_SIZE);
        void* buffer = GlobalAllocator.RequestPages(pages);
       
        // Perform the read
        bool ret = ns->read(start_lba, lba_count, buffer);
        if (!ret) goto cleanup;

        // Read was successful, copy the data
        memcpy(out, (uint8_t*)buffer + byte_offset, length);

    cleanup:
        GlobalAllocator.FreePages(buffer, pages);
        return ret ? length : -EIO;
    }


    int nvme_ns_vnode_write(uint64_t offset, uint64_t length, const void* input, vnode_t* this_node){
        // Calculate the lbas
        uint64_t byte_offset = offset % this_node->io_block_size;
        uint64_t start_lba = offset / this_node->io_block_size;
        uint64_t total_mem = ROUND_UP(length + byte_offset, this_node->io_block_size);
        uint64_t lba_count = total_mem / this_node->io_block_size;

        // Get the namespace
        nvme_namespace_t* ns = (nvme_namespace_t*)this_node->fs_identifier;

        if (ns == nullptr) return -EFAULT;

        // Check if its out of bounds
        if (start_lba > ns->amount_of_lbas || (start_lba + lba_count) > ns->amount_of_lbas) return EOF;

        // Allocate the memory
        uint64_t pages = DIV_ROUND_UP(ROUND_UP(total_mem, ns->min_page_size), PAGE_SIZE);
        void* buffer = GlobalAllocator.RequestPages(pages);

        bool ret = false;

        // If the write is at an offset or does not span the entire lba, fill the gaps
        bool exact_overwrite = (offset % this_node->io_block_size == 0) && (length == total_mem);

        if (!exact_overwrite){
            ret = ns->read(start_lba, lba_count, buffer);
            if (!ret) goto cleanup; // I/O Failed
        }

        // Copy the data to the buffer
        memcpy((uint8_t*)buffer + byte_offset, input, length);
        ret = ns->write(start_lba, lba_count, buffer);

    cleanup:
        GlobalAllocator.FreePages(buffer, pages);
        return ret ? length : -EIO;
    }

    vnode_ops_t nvme_namespace_operations = {
        .read = nvme_ns_vnode_read,
        .write = nvme_ns_vnode_write
    };

    nvme_namespace_t::nvme_namespace_t(int identifier, nvme_driver_t* driver){
        this->identifier = identifier;
        this->driver = driver;

        this->min_page_size = driver->min_page_size;
        this->max_page_size = driver->max_page_size;
        this->max_data_transfer_size = driver->max_data_transfer_size; 

        nvme_identify_ns_t* response = (nvme_identify_ns_t*)GlobalAllocator.RequestPage();
        
        int ret = driver->_identify_namespace(identifier, response);
        if (ret != 0){
            kprintf("\e[0;31m[NVMe]\e[0m Identify(Namespace %d) command failed with status: %d\n", identifier, ret);
            GlobalAllocator.FreePage(response);

            return;
        }

        // Find the current format (selected LBA size)
        uint8_t format_index = response->formatted_lba_size & 0x0F; // Lower 4 bits
        nvme_lba_format_t format = response->lba_formats[format_index];
        

        this->lba_size = 1ULL << format.lba_data_size;
        this->amount_of_lbas = response->ns_size;
        GlobalAllocator.FreePage(response);

        // Create a node to represent this device
        char pathname[128];
        stringf(pathname, sizeof(pathname), "/dev/%sn%d", this->driver->vnode->name, this->identifier); // Copy the name

        this->vnode = vfs::create_path(pathname, VBLK);
        this->vnode->fs_identifier = (uint64_t)this;
        this->vnode->io_block_size = this->lba_size;
        this->vnode->size = this->lba_size * this->amount_of_lbas;
        
        memcpy(&this->vnode->file_operations, &nvme_namespace_operations, sizeof(vnode_ops_t));


        intialize_disk_partitions(this->vnode);
        
        this->vnode->close();
        return;
    }

    nvme_namespace_t::~nvme_namespace_t(){

    }

    bool nvme_namespace_t::create_prps(nvme_submission_entry_t* cmd, void* buffer, size_t size){
        uint64_t physical = virtual_to_physical((uint64_t)buffer);
        uint64_t offset = 0;
        uint64_t page_size = this->max_page_size;

        // Set PRP1
        cmd->data_pointers[0] = physical;
        offset += page_size;

        if (size <= page_size) return true;

        // Setup PRP2 / List

        // Check if there is only 1 page remaining. If yes, set that to the prp2
        if (size - offset <= this->max_page_size){
            cmd->data_pointers[1] = physical + offset;
            return true;
        }

        uint64_t* last_link_ptr = &cmd->data_pointers[1]; 
        const uint64_t entries_per_page = page_size / sizeof(uint64_t); // 512
        const uint64_t pages_per_list = min(page_size / PAGE_SIZE, 1);

        while (offset < size) {
            // Allocate List Page (Virtual Address)
            uint64_t* page_list = (uint64_t*)GlobalAllocator.RequestPages(pages_per_list);
            if (!page_list) return false;
            memset(page_list, 0, page_size);

            // Link previous pointer to this new list (Physical Address)
            *last_link_ptr = virtual_to_physical((uint64_t)page_list);

            uint64_t bytes_remaining = size - offset;
            uint64_t pages_needed = DIV_ROUND_UP(bytes_remaining, page_size);
            uint64_t count = min(pages_needed, entries_per_page - 1);

            // Fill the list
            for (uint64_t i = 0; i < count; i++) {
                page_list[i] = physical + offset;
                offset += min(page_size, (size - offset));
            }

            // Final Check: Do we fit in the last slot, or do we need a new list?
            uint64_t rem = size - offset;
            
            // If 1 page (or less) remains, it fits in the very last slot (511).
            if (rem <= page_size) {
                if (rem > 0) {
                    page_list[entries_per_page - 1] = physical + offset;
                }
                return true;
            }

            // Otherwise, slot 511 becomes the link to the NEXT list.
            last_link_ptr = &page_list[entries_per_page - 1];
        }

        return true;
    }

    void nvme_namespace_t::free_prps(nvme_submission_entry_t* cmd, uint64_t size){
        uint64_t page_size = this->max_page_size;
        
        // Check if lists were used
        if (size <= (page_size * 2)) return;

        uint64_t bytes_processed = page_size;
        uint64_t next_list_phys = cmd->data_pointers[1];
        
        const uint64_t entries_per_page = page_size / sizeof(uint64_t); // 512
        const uint64_t pages_per_list = min(page_size / PAGE_SIZE, 1);

        while (bytes_processed < size) {
            uint64_t* current_list_virt = (uint64_t*)physical_to_virtual(next_list_phys);
            
            // Calculate remaining data
            uint64_t bytes_remaining = size - bytes_processed;
            uint64_t pages_remaining = DIV_ROUND_UP(bytes_remaining, page_size);

            // Determine if this list links to another one
            bool is_chained = (pages_remaining > entries_per_page);
            
            uint64_t next_phys_temp = 0;
            if (is_chained) {
                next_phys_temp = current_list_virt[entries_per_page - 1];
                
                bytes_processed += (entries_per_page - 1) * page_size;
            } else {
                bytes_processed = size; // Force loop exit
            }

            GlobalAllocator.FreePages(current_list_virt, pages_per_list); 

            next_list_phys = next_phys_temp;
        }
    }

    bool nvme_namespace_t::read(uint64_t lba_start, uint32_t lba_count, void* buffer){
        uint8_t* ptr = (uint8_t*)buffer;
        
        uint32_t chunk_size_in_blocks = this->max_data_transfer_size / this->lba_size;

        while (lba_count > 0) {
            uint32_t chunk_blocks = min(chunk_size_in_blocks, lba_count);
            uint64_t chunk_bytes = chunk_blocks * this->lba_size;

            nvme_submission_entry_t entry;
            memset(&entry, 0, sizeof(nvme_submission_entry_t));

            entry.cmd_specific[0] = (uint32_t)(lba_start & 0xFFFFFFFF);
            entry.cmd_specific[1] = (uint32_t)((lba_start >> 32) & 0xFFFFFFFF);
            entry.cmd_specific[2] = chunk_blocks - 1;

            uint64_t buffer_size = chunk_bytes;
            this->create_prps(&entry, ptr, buffer_size);

            entry.namespace_identifier = this->identifier;
            entry.command = NVME_IO_CMD_READ;

            int ret = driver->_send_io_command(&entry);
            
            this->free_prps(&entry, buffer_size);

            if (ret != 0) return false;

            lba_count -= chunk_blocks;
            lba_start += chunk_blocks;
            ptr += chunk_bytes;
        }
        return true;
    }

    bool nvme_namespace_t::write(uint64_t lba_start, uint32_t lba_count, const void* buffer){
        uint8_t* ptr = (uint8_t*)buffer;

        uint32_t chunk_size_in_blocks = this->max_data_transfer_size / this->lba_size;

        while (lba_count > 0) {
            uint32_t chunk_blocks = min(chunk_size_in_blocks, lba_count);
            uint64_t chunk_bytes = chunk_blocks * this->lba_size;

            nvme_submission_entry_t entry;
            memset(&entry, 0, sizeof(nvme_submission_entry_t));

            entry.cmd_specific[0] = (uint32_t)(lba_start & 0xFFFFFFFF);
            entry.cmd_specific[1] = (uint32_t)((lba_start >> 32) & 0xFFFFFFFF);
            entry.cmd_specific[2] = chunk_blocks - 1;
            
            uint64_t buffer_size = chunk_bytes;
            this->create_prps(&entry, (void*)ptr, buffer_size);

            entry.namespace_identifier = this->identifier;
            entry.command = NVME_IO_CMD_WRITE;

            int ret = driver->_send_io_command(&entry);
            
            this->free_prps(&entry, buffer_size);

            if (ret != 0) return false;

            lba_count -= chunk_blocks;
            lba_start += chunk_blocks;
            ptr += chunk_bytes;
        }

        return true;
    }
} // namespace drivers

