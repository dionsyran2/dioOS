#include "XHCI.h"
#include "xhci_common.h"

namespace drivers{

    xhci_driver::xhci_driver(PCI::PCIDeviceHeader* device) : PCI_BASE_DRIVER(device){
        
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

        m_xhc_base = xhci_map_mmio(m_xhc_base, 0x2000);

        kprintf("m_xhc_base virtual     : 0x%llx\n", m_xhc_base);
        kprintf("m_xhc_base physical     : 0x%llx\n", xhci_get_physical_addr((void*)m_xhc_base));

        _parse_capability_registers();

        _log_capability_registers();

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

    void xhci_driver::_setup_interrupts(){
        PCI::MSIX_Capability* msix = (PCI::MSIX_Capability*)PCI::findCap(device_header, PCI_CAP_MSIX_ID);

        kprintf("CAPID: %h\n", msix->CapID);
    }
}