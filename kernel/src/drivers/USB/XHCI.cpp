#include <drivers/USB/XHCI.h>
#include <drivers/USB/xhci_common.h>
#include "interrupts/interrupts.h"

void XHCI_INT_HANDLER(drivers::xhci_driver* driver);

namespace drivers{

    xhci_driver::xhci_driver(PCI::PCIDeviceHeader* device) : PCI_BASE_DRIVER(device){
        this->type = PCI_DRIVER_XHCI;
    }

    xhci_driver::~xhci_driver(){

    }

    bool xhci_driver::init_device(){
        PCI::PCIHeader0* hdr0 = (PCI::PCIHeader0*)device_header; 
        m_xhc_base = 0;

        m_xhc_base = (hdr0->BAR0 & 0xFFFFFFF0);
        if ((hdr0->BAR1) != 0){ // 64 bit bar address (fix qemu shenanigans)
            m_xhc_base = m_xhc_base;
            m_xhc_base |= ((uint64_t)hdr0->BAR1) << 32;
        }

        m_xhc_base = xhci_map_mmio(m_xhc_base, 0x4000);

        kprintf("m_xhc_base virtual     : 0x%llx\n", m_xhc_base);
        kprintf("m_xhc_base physical     : 0x%llx\n", xhci_get_physical_addr((void*)m_xhc_base));

        _parse_capability_registers();
        _log_capability_registers();

        _parse_extended_capabilities();

        if (!_reset_host_controller()){
            return false;
        }
        
        _configure_operational_registers();
        _log_operational_registers();

        _configure_runtime_registers();

        _setup_interrupts();

        return true;
    }

    bool xhci_driver::start_device(){
        _log_usbsts();
        bool status = _start_host_controller();

        xhci_trb_t trb;
        memset(&trb, 0, sizeof(trb));
        trb.trb_type = XHCI_TRB_TYPE_ENABLE_SLOT_CMD;

        Sleep(1000);

        for (int i = 0; i < m_max_ports; i++){
            xhci_command_completion_trb_t* completion_trb = _send_command_trb(&trb);
            kprintf("Port %d is USB%d\n", i, _is_usb3_port(i) ? 3 : 2);
            if (completion_trb){
                //kprintf("Completion TRB, completion code: 0x%x, slot_id: %d\n", completion_trb->completion_code, completion_trb->slot_id);
            }
        }


        _log_usbsts();

        return status;
    }

    bool xhci_driver::shutdown_device(){
        
    }


    void xhci_driver::_parse_capability_registers() {
        m_cap_regs = reinterpret_cast<volatile xhci_capability_registers*>(m_xhc_base);

        m_capability_regs_length = m_cap_regs->caplength;

        m_max_device_slots = XHCI_MAX_DEVICE_SLOTS(m_cap_regs);
        m_max_interrupters = XHCI_MAX_INTERRUPTERS(m_cap_regs);
        m_max_ports = XHCI_MAX_PORTS(m_cap_regs);

        m_isochronous_scheduling_threshold = XHCI_IST(m_cap_regs);
        m_erst_max = XHCI_ERST_MAX(m_cap_regs);
        m_max_scratchpad_buffers = XHCI_MAX_SCRATCHPAD_BUFFERS(m_cap_regs);

        m_64bit_addressing_capability = XHCI_AC64(m_cap_regs);
        m_bandwidth_negotiation_capability = XHCI_BNC(m_cap_regs);
        m_64byte_context_size = XHCI_CSZ(m_cap_regs);
        m_port_power_control = XHCI_PPC(m_cap_regs);
        m_port_indicators = XHCI_PIND(m_cap_regs);
        m_light_reset_capability = XHCI_LHRC(m_cap_regs);
        m_extended_capabilities_offset = XHCI_XECP(m_cap_regs) * sizeof(uint32_t);

        // Update the base pointer to operational register set
        m_op_regs = reinterpret_cast<volatile xhci_operational_registers*>(m_xhc_base + m_capability_regs_length);


        // Update the base pointer to the runtime register set
        m_runtime_regs = reinterpret_cast<volatile xhci_runtime_registers*>(m_xhc_base + m_cap_regs->rtsoff);

        // Construct a manager class instance for the doorbell register array
        m_doorbell_manager = new xhci_doorbell_manager(m_xhc_base + m_cap_regs->dboff);
    }

    void xhci_driver::_parse_extended_capabilities(){
        volatile uint32_t* head_cap_ptr = (volatile uint32_t*)(m_xhc_base + m_extended_capabilities_offset);

        m_extended_capabilities_head = new xhci_extended_capability(head_cap_ptr);

        xhci_extended_capability* node = m_extended_capabilities_head;
        while(node != nullptr){
            if (node->id() == xhci_extended_capability_code::supported_protocol){
                xhci_usb_supported_protocol_capability cap(node->base());

                // Make the ports zero-based
                uint8_t first_port = cap.compatible_port_offset - 1;
                uint8_t last_port = first_port + cap.compatible_port_count - 1;

                if (cap.major_revision_version == 3) {
                    for (uint8_t port = first_port; port <= last_port; port++){
                        // Keep track of USB3 ports
                        m_usb3_ports.push_back(port);
                    }
                }
            }

            // Advance to the next node
            node = node->next();
        }
    }

    void xhci_driver::_log_capability_registers(){
        kprintf("===== Xhci Capability Registers (0x%llx) =====\n", (uint64_t)m_cap_regs);
        kprintf("    Length                : %d\n", m_capability_regs_length);
        kprintf("    Max Device Slots      : %d\n", m_max_device_slots);
        kprintf("    Max Interrupters      : %d\n", m_max_interrupters);
        kprintf("    Max Ports             : %d\n", m_max_ports);
        kprintf("    IST                   : %d\n", m_isochronous_scheduling_threshold);
        kprintf("    ERST Max Size         : %d\n", m_erst_max);
        kprintf("    Scratchpad Buffers    : %d\n", m_max_scratchpad_buffers);
        kprintf("    64-bit Addressing     : %s\n", m_64bit_addressing_capability ? "yes" : "no");
        kprintf("    Bandwidth Negotiation : %d\n", m_bandwidth_negotiation_capability);
        kprintf("    64-byte Context Size  : %s\n", m_64byte_context_size ? "yes" : "no");
        kprintf("    Port Power Control    : %d\n", m_port_power_control);
        kprintf("    Port Indicators       : %d\n", m_port_indicators);
        kprintf("    Light Reset Available : %d\n", m_light_reset_capability);
        kprintf("\n");
    }

    void xhci_driver::_log_operational_registers(){
        kprintf("===== Xhci Operational Registers (0x%llx) =====\n", (uint64_t)m_op_regs);
        kprintf("    usbcmd     : 0x%x\n", m_op_regs->usbcmd);
        kprintf("    usbsts     : 0x%x\n", m_op_regs->usbsts);
        kprintf("    pagesize   : 0x%x\n", m_op_regs->pagesize);
        kprintf("    dnctrl     : 0x%x\n", m_op_regs->dnctrl);
        kprintf("    crcr       : 0x%llx\n", m_op_regs->crcr);
        kprintf("    dcbaap     : 0x%llx\n", m_op_regs->dcbaap);
        kprintf("    config     : 0x%x\n", m_op_regs->config);
        kprintf("\n");
    }

    
    bool xhci_driver::_is_usb3_port(uint8_t port_num){
        for (size_t i = 0; i < m_usb3_ports.size(); i++){
            if (m_usb3_ports[i] == port_num) {
                return true;
            }
        }

        return false;
    }

    bool xhci_driver::_reset_host_controller(){
        // Clear R/S bit
        uint32_t usbcmd = m_op_regs->usbcmd;
        usbcmd &= ~XHCI_USBCMD_RUN_STOP;
        m_op_regs->usbcmd = usbcmd;
    
        // Wait for the HCH bit
        uint32_t timeout = 200;

        while (!(m_op_regs->usbsts & XHCI_USBSTS_HCH)){
            if (--timeout == 0){
                kprintf(0xFF0000, "[XHCI] ");
                kprintf("Host controller did not halt within 200ms\n");
                return false;
            }

            Sleep(1);
        }

        // Set the HC Reset bit
        usbcmd = m_op_regs->usbcmd;
        usbcmd |= XHCI_USBCMD_HCRESET;
        m_op_regs->usbcmd = usbcmd;


        // Wait for the controller to finish resetting
        timeout = 1000;

        while (
            m_op_regs->usbcmd & XHCI_USBCMD_HCRESET ||
            m_op_regs->usbsts & XHCI_USBSTS_CNR
        ){
            if (--timeout == 0){
                kprintf(0xFF0000, "[XHCI] ");
                kprintf("Host controller did not reset within 1000ms\n");
                return false;
            }

            Sleep(1);
        }

        Sleep(100);

        // Check the default of the operational registers
        if (m_op_regs->usbcmd != 0)
            return false;

        if (m_op_regs->dnctrl != 0)
            return false;

        if (m_op_regs->crcr != 0)
            return false;

        if (m_op_regs->dcbaap != 0)
            return false;

        if (m_op_regs->config != 0)
            return false;

        return true;
    }

    void xhci_driver::_configure_operational_registers(){
        // Enable notifications
        m_op_regs->dnctrl = 0xffff;


        // Configure the usbconfig field
        m_op_regs->config = static_cast<uint32_t>(m_max_device_slots);

        // Set up DCBAA with scratchpad buffer
        _setup_dcbaa();

        // Set up the command ring
        m_command_ring = new xhci_command_ring(XHCI_COMMAND_RING_TRB_COUNT);

        m_op_regs->crcr = m_command_ring->get_physical_base() | m_command_ring->get_cycle_bit();

    }

    void xhci_driver::_setup_dcbaa(){
        size_t dcbaa_size = sizeof(uintptr_t) * (m_max_device_slots + 1);


        m_dcbaa = reinterpret_cast<uint64_t*>(alloc_xhci_memory(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY));

        // Initialize scratchpad buffer array if needed
        if (m_max_scratchpad_buffers > 0){
            uint64_t* scratchpad_array = reinterpret_cast<uint64_t*>((
                m_max_scratchpad_buffers * sizeof(uint64_t),
                XHCI_DEVICE_CONTEXT_ALIGNMENT,
                XHCI_DEVICE_CONTEXT_BOUNDARY
            ));

            // Create scratchpad pages
            for (int i = 0; i < m_max_scratchpad_buffers; i++){
                void* scratchpad = alloc_xhci_memory(
                    PAGE_SIZE,
                    XHCI_SCRATCHPAD_BUFFER_ARRAY_ALIGNMENT,
                    XHCI_SCRATCHPAD_BUFFER_ARRAY_BOUNDARY
                );

                uint64_t scratchpad_paddr = xhci_get_physical_addr(scratchpad);
                scratchpad_array[i] = scratchpad_paddr;
            }

            uint64_t scratchpad_array_paddr = xhci_get_physical_addr(scratchpad_array);

            // Set the first entry in the DCBAA to point to the scratchpad array
            m_dcbaa[0] = scratchpad_array_paddr;
        }



        // Set DCBAA pointer in the operational registers
        m_op_regs->dcbaap = xhci_get_physical_addr(m_dcbaa);
    }


    void xhci_driver::_configure_runtime_registers(){
        // Get the primary interrupter registers
        volatile xhci_interrupter_registers* interrupter_regs = &m_runtime_regs->ir[0];

        // Enable the interrupts
        uint32_t iman = interrupter_regs->iman;
        iman |= XHCI_IMAN_INTERRUPT_ENABLE;
        interrupter_regs->iman = iman;

        // Setup the event ring and write to interrupter
        m_event_ring = new xhci_event_ring(XHCI_EVENT_RING_TRB_COUNT, interrupter_regs);

        // Clear any pending interrupts for the primary interrupter
        _acknowledge_irq(0);
    }

    bool xhci_driver::_start_host_controller(){
        // Ensure USBCMD bit for RUN/STOP is properly set
        uint32_t usbcmd = m_op_regs->usbcmd;
        usbcmd |= XHCI_USBCMD_RUN_STOP;
        usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
        m_op_regs->usbcmd = usbcmd;


        int timeout = 1000;

        while(m_op_regs->usbsts & XHCI_USBSTS_HCH){
            if ((timeout--) <= 0){
                return false;
            }
            Sleep(1);
        }

        // Verify that the CNR bit is clear

        if (m_op_regs->usbsts & XHCI_USBSTS_CNR) 
            return false;

        return true;
    }

    
    void xhci_driver::_log_usbsts() {
        uint32_t status = m_op_regs->usbsts;
        kprintf("===== USBSTS =====\n");
        if (status & XHCI_USBSTS_HCH)  kprintf("    Host Controlled Halted\n");
        if (status & XHCI_USBSTS_HSE)  kprintf("    Host System Error\n");
        if (status & XHCI_USBSTS_EINT) kprintf("    Event Interrupt\n");
        if (status & XHCI_USBSTS_PCD)  kprintf("    Port Change Detect\n");
        if (status & XHCI_USBSTS_SSS)  kprintf("    Save State Status\n");
        if (status & XHCI_USBSTS_RSS)  kprintf("    Restore State Status\n");
        if (status & XHCI_USBSTS_SRE)  kprintf("    Save/Restore Error\n");
        if (status & XHCI_USBSTS_CNR)  kprintf("    Controller Not Ready\n");
        if (status & XHCI_USBSTS_HCE)  kprintf("    Host Controller Error\n");
        kprintf("\n");
    }


    void xhci_driver::_acknowledge_irq(uint8_t interrupter){
        // Clear the EINT bit in USBSTS by writing '1' to it
        m_op_regs->usbsts = XHCI_USBSTS_EINT;

        // Get the interrupter registers
        volatile xhci_interrupter_registers* interrupter_regs = &m_runtime_regs->ir[interrupter];

        // Read the current value of IMAN
        uint32_t iman = interrupter_regs->iman;

        // Set the IP bit to 1
        iman |= XHCI_IMAN_INTERRUPT_PENDING;

        // Write the new iman value
        interrupter_regs->iman = iman;
    }

    bool xhci_driver::_has_int(){
        return (m_op_regs->usbsts & XHCI_USBSTS_EINT) != 0;
    }

    void xhci_driver::_setup_interrupts(){
        addIsrWithContext(PCI_INT_HANDLER_VECTOR, (void*)XHCI_INT_HANDLER, (void*)this);
    }

    void xhci_driver::handle_int(){
        if (!_has_int()) return;
        //kprintf("[XHCI] Received Interrupt!\n");

        _process_events();

        _acknowledge_irq(0);
    }

    void xhci_driver::_process_events(){
        kstl::Vector<xhci_trb_t*> events;

        if (m_event_ring->has_unprocessed_events()){
            m_event_ring->dequeue_events(events);
        }

        uint8_t command_completion_status = 0;

        for (size_t i = 0; i < events.size(); i++){
            xhci_trb_t* event = events[i];
            switch(event->trb_type){
                case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: {
                    command_completion_status = 1;
                    m_command_completion_events.push_back((xhci_command_completion_trb_t*)event);
                    break;
                }

                default: {
                    break;
                }
            }
        }

        m_command_irq_completed = command_completion_status;
    }

    xhci_command_completion_trb_t* xhci_driver::_send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms){
        // Enqueue the trb
        m_command_ring->enqueue(cmd_trb);

        // Ring the command doorbell
        m_doorbell_manager->ring_command_doorbell();

        // Wait for the IRQ and let the host controller process the command
        uint64_t time_elapsed = 0;

        while(!m_command_irq_completed){
            Sleep(1);
            time_elapsed += 10;

            if (time_elapsed > timeout_ms)
                break;
        }
        
        xhci_command_completion_trb_t* completion_trb = 
            m_command_completion_events.size() > 0 ? m_command_completion_events[0] : nullptr;

        // Reset the irq flag and clear out the command completion event queue
        m_command_completion_events.clear();
        m_command_irq_completed = 0;

        if (!completion_trb){
            kprintf("Failed to find completion TRB for command %d\n", cmd_trb->trb_type);
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS){
            kprintf("Command TRB failed with error: %s\n", trb_completion_code_to_string(completion_trb->completion_code));
            return nullptr;
        }

        return completion_trb;
    }
    
}

void XHCI_INT_HANDLER(drivers::xhci_driver* driver){
    driver->handle_int();
}