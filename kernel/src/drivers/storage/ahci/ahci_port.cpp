#include <drivers/storage/ahci/ahci.h>


void ahci_port_t::start(){
    // Wait until CR (bit15) is cleared
	while (this->port->command & HBA_PxCMD_CR);

	// Set FRE (bit4) and ST (bit0)
	this->port->command |= HBA_PxCMD_FRE;
    this->port->command |= HBA_PxCMD_ST;
}


void ahci_port_t::stop(){
    // Clear ST (bit0)
	this->port->command &= ~HBA_PxCMD_ST;

	// Clear FRE (bit4)
	this->port->command &= ~HBA_PxCMD_FRE;

	// Wait until FR (bit14), CR (bit15) are cleared
	while(1)
	{
		if (this->port->command & HBA_PxCMD_FR)
			continue;
		if (this->port->command & HBA_PxCMD_CR)
			continue;
		break;
	}
}

uint8_t ahci_port_t::allocate_free_slot(){
    while (1){
        uint64_t rflags = spin_lock(&this->command_slot_lock);
        for (int i = 0; i < 32; i++){
            if ((this->command_slot_state >> i) & 1) continue;
            
            this->command_slot_state |= (1 << i);
            spin_unlock(&this->command_slot_lock, rflags);
            return i;
        }
        spin_unlock(&this->command_slot_lock, rflags);

        __asm__ ("hlt");
    }
}

void ahci_port_t::free_slot(uint8_t slot){
    uint64_t rflags = spin_lock(&this->command_slot_lock);

    this->command_slot_state &= ~(1 << slot);

    spin_unlock(&this->command_slot_lock, rflags);
}

bool ahci_port_t::is_interrupt_set(uint8_t slot){
    return this->port->interrupt_status & (1 << slot) != 0;
}

void ahci_port_t::clear_interrupt(uint8_t slot){
    this->port->interrupt_status |= 1 << slot;
    this->port->sata_error = 0xFFFFFFFF;
}

void ahci_port_t::enable_interrupts(){
    this->port->interrupt_enable = -1;
}

void ahci_port_t::issue_command(uint8_t slot){
    this->port->command_issue |= (1 << slot);
}


bool ahci_port_t::get_issue_status(uint8_t slot){
    return this->port->command_issue & (1 << slot);
}