#include <drivers/storage/ahci/ahci.h>
#include <kstdio.h>
#include <kerrno.h>
#include <math.h>
#include <cpu.h>


#define MAX_PRDT_ENTRY_BUFFER_SIZE  (4 * 1024 * 1024)
#define MAX_SECTORS_PER_OP          UINT16_MAX

bool ahci_port_t::read(uint64_t start_sector, uint64_t sector_count, void* buffer){
    // If the sector count is too big, split it into multiple operations
    while (sector_count > MAX_SECTORS_PER_OP){
        if (!read(start_sector, MAX_SECTORS_PER_OP, buffer)) {
            return false;
        }
        sector_count -= MAX_SECTORS_PER_OP;
        start_sector += MAX_SECTORS_PER_OP;
        buffer = (void*)((uint64_t)buffer + (MAX_SECTORS_PER_OP * 512));
    }

    // Allocate a free command slot
    uint32_t slot = allocate_free_slot();
    // Clear the interrupts
    clear_interrupt(slot);

    volatile ahci_command_header_t* header = &this->command_list[slot];
    volatile ahci_command_table_t* table = &this->command_table_list[slot];

    memset((void*)table, 0, sizeof(ahci_command_table_t));

    memset((void*)&table->prdt_entries, 0, sizeof(table->prdt_entries));


    uint64_t boffset = 0;
    uint64_t buffer_physical = virtual_to_physical((uint64_t)buffer);
    uint64_t total_buffer_size = sector_count * 512;
    uint64_t prdt_entries_needed = DIV_ROUND_UP(total_buffer_size, MAX_PRDT_ENTRY_BUFFER_SIZE);
    
    uint64_t prdt_count = 0;
    for (uint64_t p = 0; p < prdt_entries_needed; p++){
        uint64_t physical = buffer_physical + boffset;
        uint64_t bsize = min(total_buffer_size - boffset, MAX_PRDT_ENTRY_BUFFER_SIZE);
        boffset += bsize;

        table->prdt_entries[p].data_base_address_low = physical & 0xFFFFFFFF;
        table->prdt_entries[p].data_base_address_high = physical >> 32;
        table->prdt_entries[p].byte_count = bsize - 1;
        table->prdt_entries[p].interrupt_on_completion = (p == (prdt_entries_needed - 1)) ? 1 : 0;
        prdt_count++;
    }

    header->prdtl = prdt_count;
    header->command_fis_length = sizeof(ahci_fis_reg_h2d) / 4;
    header->write = 0;
    header->atapi = 0;
    header->prefetchable = 0;
    
    ahci_fis_reg_h2d* fis = (ahci_fis_reg_h2d*)&table->command_fis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // This is a command
    fis->command = ATA_CMD_READ_DMA_EX;

    fis->lba0 = (uint8_t)start_sector;
	fis->lba1 = (uint8_t)(start_sector >> 8);
	fis->lba2 = (uint8_t)(start_sector >> 16);
    fis->lba3 = (uint8_t)(start_sector >> 24);
	fis->lba4 = (uint8_t)(start_sector >> 32);
	fis->lba5 = (uint8_t)(start_sector >> 40);

    fis->device = 1 << 6;	// LBA mode

    fis->count_low = sector_count & 0xFF;
	fis->count_high = (sector_count >> 8) & 0xFF;

    // Enable Interrupts
    bool intr = are_interrupts_enabled();
    asm ("sti");

    // Wait for the port to become available
    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ))) __asm__ ("hlt");


    // Issue the command
    issue_command(slot);

    // Wait for completion
    while (1)
	{
		if (get_issue_status(slot) == false)
			break;
        

		if (port->interrupt_status & HBA_PxIS_TFES) {
            uint32_t tfd = port->task_file_data;
            uint8_t status = tfd & 0xFF;
            uint8_t error  = (tfd >> 8) & 0xFF;

            kprintf("[AHCI FAIL] TFD: 0x%x | STS: %x | ERR: %x\n", tfd, status, error);
            
            if (error & 4) kprintf("  -> ABRT (Command Aborted) - Invalid Command or Device mismatch\n");
            if (error & 16) kprintf("  -> IDNF (ID Not Found) - Invalid LBA address\n");
            if (error & 64) kprintf("  -> UNC (Uncorrectable Data) - Bad Sector\n");

            free_slot(slot);
            if (!intr) asm ("cli");
            return false;
        }
        asm ("hlt");
	}

    if (!intr) asm ("cli");
    free_slot(slot);
    return true;
}




bool ahci_port_t::write(uint64_t start_sector, uint64_t sector_count, const void* buffer){
    __asm__ volatile ("mfence" ::: "memory");
    // If the sector count is too big, split it into multiple operations
    while (sector_count > MAX_SECTORS_PER_OP){
        if (!write(start_sector, MAX_SECTORS_PER_OP, buffer)) {
            return false; // Stop immediately if a chunk fails
        }
        sector_count -= MAX_SECTORS_PER_OP;
        start_sector += MAX_SECTORS_PER_OP;
        buffer = (void*)((uint64_t)buffer + (MAX_SECTORS_PER_OP * 512));
    }

    // Allocate a free command slot
    uint32_t slot = allocate_free_slot();

    // Clear the interrupts
    clear_interrupt(slot);

    volatile ahci_command_header_t* header = &this->command_list[slot];
    volatile ahci_command_table_t* table = &this->command_table_list[slot];

    memset((void*)table, 0, sizeof(ahci_command_table_t));

    memset((void*)&table->prdt_entries, 0, sizeof(table->prdt_entries));


    uint64_t boffset = 0;
    uint64_t buffer_physical = virtual_to_physical((uint64_t)buffer);
    uint64_t total_buffer_size = sector_count * 512;
    uint64_t prdt_entries_needed = DIV_ROUND_UP(total_buffer_size, MAX_PRDT_ENTRY_BUFFER_SIZE);;
    
    uint64_t prdt_count = 0;
    for (uint64_t p = 0; p < prdt_entries_needed; p++){
        uint64_t physical = buffer_physical + boffset;
        uint64_t bsize = min(total_buffer_size - boffset, MAX_PRDT_ENTRY_BUFFER_SIZE);
        boffset += bsize;

        table->prdt_entries[p].data_base_address_low = physical & 0xFFFFFFFF;
        table->prdt_entries[p].data_base_address_high = physical >> 32;
        table->prdt_entries[p].byte_count = bsize - 1;
        table->prdt_entries[p].interrupt_on_completion = (p == (prdt_entries_needed - 1)) ? 1 : 0;
        prdt_count++;
    }

    header->prdtl = prdt_count;
    header->command_fis_length = sizeof(ahci_fis_reg_h2d) / 4;
    header->write = 1; // We are writing to the device
    header->atapi = 0;
    header->prefetchable = 0;

    ahci_fis_reg_h2d* fis = (ahci_fis_reg_h2d*)&table->command_fis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; // This is a command
    fis->command = ATA_CMD_WRITE_DMA_EX;

    fis->lba0 = (uint8_t)start_sector;
	fis->lba1 = (uint8_t)(start_sector >> 8);
	fis->lba2 = (uint8_t)(start_sector >> 16);
    fis->lba3 = (uint8_t)(start_sector >> 24);
	fis->lba4 = (uint8_t)(start_sector >> 32);
	fis->lba5 = (uint8_t)(start_sector >> 40);

    fis->device = 1 << 6;	// LBA mode

    fis->count_low = sector_count & 0xFF;
	fis->count_high = (sector_count >> 8) & 0xFF;

    // Enable interrupts
    bool intr = are_interrupts_enabled();
    asm ("sti");
    
    // Wait for the port to become available
    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ))) __asm__ ("hlt");
    
    // Issue the command
    issue_command(slot);

    // Wait for completion
    while (1)
	{
		if (get_issue_status(slot) == false)
			break;
        

		if (port->interrupt_status & HBA_PxIS_TFES) {
            uint32_t tfd = port->task_file_data;
            uint8_t status = tfd & 0xFF;
            uint8_t error  = (tfd >> 8) & 0xFF;

            kprintf("[AHCI FAIL] TFD: 0x%x | STS: %x | ERR: %x\n", tfd, status, error);
            
            if (error & 4) kprintf("  -> ABRT (Command Aborted) - Invalid Command or Device mismatch\n");
            if (error & 16) kprintf("  -> IDNF (ID Not Found) - Invalid LBA address\n");
            if (error & 64) kprintf("  -> UNC (Uncorrectable Data) - Bad Sector\n");

            free_slot(slot);
            if (!intr) asm ("cli");
            return false;
        }
        asm ("hlt");
	}

    if (!intr) asm ("cli");
    free_slot(slot);
    return true;
}

// @param out_size: Pointer to a uint64_t where we store the total sector count
bool ahci_port_t::identify(uint64_t* out_sector_count) {
    uint16_t* identity_buf = (uint16_t*)GlobalAllocator.RequestPage(); 
    memset(identity_buf, 0, 4096);

    uint32_t slot = allocate_free_slot();
    clear_interrupt(slot);

    volatile ahci_command_header_t* header = &this->command_list[slot];
    volatile ahci_command_table_t* table = &this->command_table_list[slot];

    header->command_fis_length = sizeof(ahci_fis_reg_h2d) / 4;
    header->write = 0; // Device to Host
    header->prdtl = 1;

    memset((void*)table, 0, sizeof(ahci_command_table_t));

    uint64_t buffer_physical = virtual_to_physical((uint64_t)identity_buf);

    memset((void*)&table->prdt_entries, 0, sizeof(table->prdt_entries));

    table->prdt_entries[0].data_base_address_low = buffer_physical & 0xFFFFFFFF;
    table->prdt_entries[0].data_base_address_high = buffer_physical >> 32;
    table->prdt_entries[0].byte_count = 512 - 1; // 512 bytes exactly
    table->prdt_entries[0].interrupt_on_completion = 1;

    ahci_fis_reg_h2d* fis = (ahci_fis_reg_h2d*)&table->command_fis;
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; 
    fis->command = ATA_CMD_IDENTIFY; // 0xEC
    // All LBA/Count fields must be 0 for IDENTIFY
    fis->device = 0; 

    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)));
    
    issue_command(slot);

    while (1) {
        if (get_issue_status(slot) == false) break;
        if (port->interrupt_status & HBA_PxIS_TFES) {
            kprintf("[AHCI] IDENTIFY failed. TFD: 0x%x\n", port->task_file_data);
            free_slot(slot);
            return false;
        }
    }
    
    uint64_t sectors = 0;

    if (identity_buf[83] & (1 << 10)) {
        uint64_t* lba48_sectors = (uint64_t*)&identity_buf[100];
        sectors = *lba48_sectors;
    } else {
        uint32_t* lba28_sectors = (uint32_t*)&identity_buf[60];
        sectors = *lba28_sectors;
    }

    if (out_sector_count) *out_sector_count = sectors;

    // Model name
    /*char model[41];
    for (int i = 0; i < 20; i++) {
        uint16_t w = identity_buf[27 + i];
        model[i * 2] = (char)(w >> 8);     // High byte first
        model[i * 2 + 1] = (char)(w & 0xFF); // Low byte second
    }
    model[40] = 0;
    kprintf("[AHCI] Drive Model: %s | Size: %d sectors\n", model, sectors);
    */

    free_slot(slot);
    GlobalAllocator.FreePage(identity_buf);
    return true;
}


int ata_vnode_read(uint64_t offset, uint64_t length, void* out, vnode_t* this_node){
    // Calculate the lbas
    uint64_t byte_offset = offset % this_node->io_block_size;
    uint64_t start_lba = offset / this_node->io_block_size;
    uint64_t total_mem = ROUND_UP(length + byte_offset, this_node->io_block_size);
    uint64_t lba_count = total_mem / this_node->io_block_size;
    

    ahci_port_t* port = (ahci_port_t*)this_node->fs_identifier;
    if (port == nullptr) return -EFAULT;

    // Check if its out of bounds
    if (start_lba > port->total_sectors || (start_lba + lba_count) > port->total_sectors) return EOF;

    // Allocate the memory
    uint64_t pages = DIV_ROUND_UP(total_mem, PAGE_SIZE);
    void* buffer = GlobalAllocator.RequestPages(pages);
    
    // Perform the read
    bool ret = port->read(start_lba, lba_count, buffer);
    if (!ret) goto cleanup;

    // Read was successful, copy the data
    memcpy(out, (uint8_t*)buffer + byte_offset, length);
cleanup:
    GlobalAllocator.FreePages(buffer, pages);
    return ret ? length : -EIO;
}


int ata_vnode_write(uint64_t offset, uint64_t length, const void* input, vnode_t* this_node){
    // Calculate the lbas
    uint64_t byte_offset = offset % this_node->io_block_size;
    uint64_t start_lba = offset / this_node->io_block_size;
    uint64_t total_mem = ROUND_UP(length + byte_offset, this_node->io_block_size);
    uint64_t lba_count = total_mem / this_node->io_block_size;
    

    // Get the namespace
    ahci_port_t* port = (ahci_port_t*)this_node->fs_identifier;
    if (port == nullptr) return -EFAULT;

    // Check if its out of bounds
    if (start_lba > port->total_sectors || (start_lba + lba_count) > port->total_sectors) return EOF;

    // Allocate the memory
    uint64_t pages = DIV_ROUND_UP(total_mem, PAGE_SIZE);
    void* buffer = GlobalAllocator.RequestPages(pages);

    bool ret = false;

    // If the write is at an offset or does not span the entire lba, fill the gaps
    bool exact_overwrite = (offset % this_node->io_block_size == 0) && (length == total_mem);
    if (!exact_overwrite){
        ret = port->read(start_lba, lba_count, buffer);
        if (!ret) goto cleanup; // I/O Failed
    }

    // Copy the data to the buffer
    memcpy((uint8_t*)buffer + byte_offset, input, length);
    ret = port->write(start_lba, lba_count, buffer);

cleanup:
    GlobalAllocator.FreePages(buffer, pages);
    return ret ? length : -EIO;
}
vnode_ops_t ata_disk_ops = {
    .read = ata_vnode_read,
    .write = ata_vnode_write
};