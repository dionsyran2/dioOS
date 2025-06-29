#include <drivers/USB/xhci_rings.h>
#include <BasicRenderer.h>

xhci_command_ring::xhci_command_ring(size_t max_trbs){
    m_max_trb_count = max_trbs;
    m_rcs_bit = 1;
    m_enqueue_ptr = 0;

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);
    
    
    // Create the command ring memory block
    m_trbs = (xhci_trb_t*)alloc_xhci_memory(ring_size, XHCI_COMMAND_RING_SEGMENTS_ALIGNMENT, XHCI_COMMAND_RING_SEGMENTS_BOUNDARY);

    m_physical_base = xhci_get_physical_addr(m_trbs);


    // Set the last TRB to be a link trb
    m_trbs[m_max_trb_count - 1].parameter = m_physical_base;
    m_trbs[m_max_trb_count - 1].control = 
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;

}


void xhci_command_ring::enqueue(xhci_trb_t* trb){
    // Adjust the TRB's cycle bit

    trb->cycle_bit = m_rcs_bit;

    // Insert the TRB into the ring;
    m_trbs[m_enqueue_ptr] = *trb;

    // Advance or wrap the pointer
    if (++m_enqueue_ptr == m_max_trb_count - 1){
        m_trbs[m_max_trb_count - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;

        m_enqueue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }
}

xhci_event_ring::xhci_event_ring(size_t max_trbs, volatile xhci_interrupter_registers* interrupter){
    m_interrupter_regs = interrupter;
    m_segment_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_dequeue_ptr = 0;

    // Event ring will only use one segment
    const uint64_t segment_count = 1;

    const uint64_t segment_size = max_trbs * sizeof(xhci_trb_t);
    const uint64_t segment_table_size = segment_count * sizeof(xhci_erst_entry);
    
    // Create the event ring segment memory block
    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        segment_size,
        XHCI_EVENT_RING_SEGMENTS_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENTS_BOUNDARY
    );

    // Store the physical DMA address
    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Create the segment table
    
    m_segment_table = (xhci_erst_entry*)alloc_xhci_memory(
        segment_table_size,
        XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY
    );

    // Construct the segment table entry
    xhci_erst_entry entry;
    entry.ring_segment_base_address = m_physical_base;
    entry.ring_segment_size = m_segment_trb_count;
    entry.rsvd = 0;


    // Insert the constructed segment into the table
    m_segment_table[0] = entry;

    // Configure the Event Ring Segment Table Size (ERSTSZ)
    m_interrupter_regs->erstsz = 1;

    // Initialize and set the ERDP
    _update_erdp();

    // Write the ERSTBA register
    m_interrupter_regs->erstba = xhci_get_physical_addr(m_segment_table);

}


void xhci_event_ring::_update_erdp(){
    uint64_t dequeue_address = m_physical_base + (m_dequeue_ptr * sizeof(xhci_trb_t));
    m_interrupter_regs->erdp = dequeue_address;
}

xhci_trb_t* xhci_event_ring::_dequeue_trb(){
    if (m_trbs[m_dequeue_ptr].cycle_bit != m_rcs_bit){
        kprintf("Event Ring attempted to dequeue an invalid TRB, returning nullptr\n");
        return nullptr;
    }

    // Get the resulting TRB
    xhci_trb_t* ret = &m_trbs[m_dequeue_ptr];


    // Advance and possibly wrap the dequeue pointer
    if (++m_dequeue_ptr == m_segment_trb_count){
        m_dequeue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }

    return ret;
}

bool xhci_event_ring::has_unprocessed_events(){
    return (m_trbs[m_dequeue_ptr].cycle_bit == m_rcs_bit);
}

void xhci_event_ring::dequeue_events(kstl::Vector<xhci_trb_t*>& trbs){
    // Process each event TRB
    while(has_unprocessed_events()){
        xhci_trb_t* trb = _dequeue_trb();
        if (!trb) break;


        trbs.push_back(trb);
    }

    // Update the ERDP register
    _update_erdp();

    // Clear the EHB (Event Handler Busy) bit
    uint64_t erdp = m_interrupter_regs->erdp;
    erdp |= XHCI_ERDP_EHB;
    m_interrupter_regs->erdp = erdp;
}

void xhci_event_ring::flush_unprocessed_events(){
    kstl::Vector<xhci_trb_t*> events;
    dequeue_events(events);
    events.clear();
}