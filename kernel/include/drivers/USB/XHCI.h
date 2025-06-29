#pragma once
#include "../../memory.h"
#include <stdint.h>
#include "../../pci.h"
#include "../../scheduling/apic/apic.h"
#include "../../BasicRenderer.h"
#include "../../paging/PageTableManager.h"
#include "../../memory/heap.h"
#include "../../paging/PageFrameAllocator.h"
#include "../../cstr.h"
#include "../../acpi.h"
#include "../../scheduling/apic/apic.h"
#include "xhci_mem.h"
#include "xhci_regs.h"
#include "xhci_rings.h"
#include "xhci_ext_cap.h"

namespace PCI{
    struct PCIHeader0;
}

namespace drivers{
    class xhci_driver : public PCI_BASE_DRIVER{
        public:
        xhci_driver(PCI::PCIDeviceHeader* device);
        ~xhci_driver();

        bool init_device();
        bool start_device();
        bool shutdown_device();
        void handle_int();


        private:
            uintptr_t m_xhc_base;

            volatile xhci_capability_registers* m_cap_regs;
            volatile xhci_operational_registers* m_op_regs;
            volatile xhci_runtime_registers* m_runtime_regs;

            // Linked list of extended capabilities
            xhci_extended_capability* m_extended_capabilities_head;

            // CAPLENGTH
            uint8_t m_capability_regs_length;
            
            // HCSPARAMS1
            uint8_t m_max_device_slots;
            uint8_t m_max_interrupters;
            uint8_t m_max_ports;

            // HCSPARAMS2
            uint8_t m_isochronous_scheduling_threshold;
            uint8_t m_erst_max;
            uint8_t m_max_scratchpad_buffers;

            // hccparams1
            bool m_64bit_addressing_capability;
            bool m_bandwidth_negotiation_capability;
            bool m_64byte_context_size;
            bool m_port_power_control;
            bool m_port_indicators;
            bool m_light_reset_capability;
            uint32_t m_extended_capabilities_offset;

            uint64_t* m_dcbaa;

            xhci_command_ring* m_command_ring;
            xhci_event_ring* m_event_ring;

            // Doorbell register
            xhci_doorbell_manager* m_doorbell_manager;

            kstl::Vector<xhci_command_completion_trb_t*> m_command_completion_events;

            // Flag indicating we have a command completion event
            volatile uint8_t m_command_irq_completed = 0;

            // USB 3.x specific ports (0-based)
            kstl::Vector<uint8_t> m_usb3_ports;

    private:
        void _process_events();
        void _parse_capability_registers();
        void _parse_extended_capabilities();

        void _log_capability_registers();
        void _log_operational_registers();
        void _log_usbsts();

        bool _is_usb3_port(uint8_t port_num);
        
        bool _reset_host_controller();

        void _configure_operational_registers();
        void _setup_dcbaa();

        void _configure_runtime_registers();
        void _acknowledge_irq(uint8_t interrupter);

        bool _start_host_controller();
        void _setup_interrupts();

        bool _has_int();

        xhci_command_completion_trb_t* _send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms = 200);
    };
}
