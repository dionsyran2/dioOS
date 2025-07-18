#ifndef XHCI_RINGS_H
#define XHCI_RINGS_H

#include "xhci_mem.h"
#include "xhci_trb.h"
#include "xhci_regs.h"
#include <vector.h>

class xhci_command_ring {
public:
    xhci_command_ring(size_t max_trbs);

    inline xhci_trb_t* get_virtual_base() const { return m_trbs; }
    inline uintptr_t get_physical_base() const { return m_physical_base; }
    inline uint8_t  get_cycle_bit() const { return m_rcs_bit; }

    void enqueue(xhci_trb_t* trb);

private:
    size_t              m_max_trb_count;     // Number of valid TRBs in the ring including the LINK_TRB
    size_t              m_enqueue_ptr;       // Index in the ring where to enqueue next TRB
    xhci_trb_t*         m_trbs;              // Base address of the ring buffer
    uintptr_t           m_physical_base;     // Physical base of the ring
    uint8_t             m_rcs_bit;           // Ring cycle state
};

/*
// xHci Spec Section 6.5 Event Ring Segment Table Figure 6-40: Event Ring Segment Table Entry

Note: The Ring Segment Size may be set to any value from 16 to 4096, however
software shall allocate a buffer for the Event Ring Segment that rounds up its
size to the nearest 64B boundary to allow full cache-line accesses.
*/
struct xhci_erst_entry {
    uint64_t ring_segment_base_address; // Base address of the Event Ring segment
    uint32_t ring_segment_size;         // Size of the Event Ring segment (only low 16 bits are used)
    uint32_t rsvd;
} __attribute__((packed));

class xhci_event_ring {
public:
    xhci_event_ring(size_t max_trbs, volatile xhci_interrupter_registers* interrupter);

    inline xhci_trb_t* get_virtual_base() const { return m_trbs; }
    inline uintptr_t get_physical_base() const { return m_physical_base; }
    inline uint8_t  get_cycle_bit() const { return m_rcs_bit; }

    bool has_unprocessed_events();
    void dequeue_events(kstl::Vector<xhci_trb_t*>& trbs);

    void flush_unprocessed_events();

private:
    volatile xhci_interrupter_registers* m_interrupter_regs;

    size_t             m_segment_trb_count;     // Max TRBs allowed on the segment

    xhci_trb_t*        m_trbs;                  // Primary segment ring base
    uintptr_t          m_physical_base;

    xhci_erst_entry*   m_segment_table;         // Event ring segment table base

    uint64_t           m_dequeue_ptr;           // Event ring dequeue pointer
    uint8_t            m_rcs_bit;               // Ring cycle state

private:
    void _update_erdp();
    xhci_trb_t* _dequeue_trb();
} __attribute__((packed));

#endif // XHCI_RINGS_H