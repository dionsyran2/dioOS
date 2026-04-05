#include <filesystem/fat32/fat32.h>
#include <filesystem/fat32/fat32_helpers.h>
#include <memory.h>
#include <memory/heap.h>
#include <cstr.h>
#include <math.h>
#include <kstdio.h>
#include <kerrno.h>

/* IMPORTANT NOTE: */
/* The fs_identifier for FAT32 is a pointer to the driver class */
/*
The file_identifier contains the pointer to a fat32_vnode_info_t struct.
*/

int fat32_vnode_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;
    fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
    if (file_info == nullptr) return -EFAULT;

    uint32_t cluster = file_info->start_cluster;

    int ret = fs->read_contents(cluster, offset, length, buffer);
    uint64_t rem = this_node->size - offset;
    return min(ret, rem);
}

int fat32_vnode_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;
    fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
    if (file_info == nullptr) return -EFAULT;
    
    uint32_t cluster = file_info->start_cluster;


    int ret = fs->write_contents(cluster, offset, length, buffer);
    
    // Update the entry's size
    uint64_t new_size = offset + ret;
    this_node->size = new_size;

    fat_directory_entry_t fat_entry;
    bool r = fs->load_directory_entry(file_info->parent_start_cluster, file_info->sfn_name, &fat_entry);

    if (r){
        // fat_entry.last_modification_date
        // fat_entry.last_modification_time
    
        fat_entry.file_size = new_size;
    }
    
    return new_size;
}

int fat32_vnode_read_dir(vnode_t** out, vnode_t* this_node);

int fat32_vnode_cleanup(vnode_t* this_node){
    // Cleanup
    if (this_node->file_identifier){
        fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
        delete file_info;
    }

    return 0;
}

int fat32_vnode_mkdir(char* name, vnode_t* this_node){
    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;
    fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;  
    if (file_info == nullptr) return -EFAULT;

    uint32_t cluster = file_info->start_cluster;

    fs->create_dir(cluster, name);
    return 0;
}

int fat32_vnode_creat(char* name, vnode_t* this_node){
    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;
    fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
    if (file_info == nullptr) return -EFAULT;

    uint32_t cluster = file_info->start_cluster;
    
    fs->create_file(cluster, name);
    return 0;
}

int fat32_vnode_unlink(vnode_t* this_node){
    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;
    fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
    if (file_info == nullptr) return -EFAULT;

    uint32_t cluster = file_info->start_cluster;

    // --- SAFETY CHECK FOR DIRECTORIES ---
    if (this_node->type == VDIR) {
        if (!fs->is_dir_empty(cluster)) {
            return -ENOTEMPTY;
        }
    }

    // TO-DO: Free LFNS (so we don't waste space)
    fs->free_cluster_chain(cluster);

    fat_directory_entry_t entry;
    
    if (fs->load_directory_entry(file_info->parent_start_cluster, file_info->sfn_name, &entry)) {
        entry.name[0] = 0xE5; // Magic Deleted Marker
        entry.first_cluster_high = 0;
        
        fs->update_directory_entry(file_info->parent_start_cluster, file_info->sfn_name, &entry);
    }

    // Clear any allocated memory
    fat32_vnode_cleanup(this_node);

    // Set the file identifier to null to avoid use after free
    this_node->file_identifier = 0;
    
    return 0;
}

int fat32_vnode_find_file(const char *filename, vnode_t** out, vnode_t* this_node);

vnode_ops_t fat32_file_operations = {
    .read = fat32_vnode_read,
    .write = fat32_vnode_write,
    .unlink = fat32_vnode_unlink,
    .cleanup = fat32_vnode_cleanup,
};

vnode_ops_t fat32_dir_operations = {
    .read_dir = fat32_vnode_read_dir,
    .find_file = fat32_vnode_find_file,
    .mkdir = fat32_vnode_mkdir,
    .creat = fat32_vnode_creat,
    .rmdir = fat32_vnode_unlink,
    .cleanup = fat32_vnode_cleanup,
};

int fat32_vnode_find_file(const char *filename, vnode_t **out, vnode_t *this_node){
    if (this_node->type != VDIR) return 0;

    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;

    uint32_t cluster = 0;
    if (this_node->file_identifier == 0){
        cluster = fs->get_root_dir_cluster();
    } else {
        // Read the entry and get its first cluster
        fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
        
        cluster = file_info->start_cluster;
    }


    uint64_t buffer_size = fs->get_total_size(cluster);

    fat_directory_entry_t* entry_table = (fat_directory_entry_t*)malloc(buffer_size);
    if (!entry_table) return 0;

    if (!fs->read_contents(cluster, 0, buffer_size, entry_table)){
        free(entry_table);
        return 0;
    }

    bool lfn_found = false;
    char lfn_buffer[512];
    char sfn_buffer[13];
    uint8_t expected_checksum = 0;

    for (fat_directory_entry_t* entry = entry_table; entry->name[0] != 0; ) {        
        if ((unsigned char)entry->name[0] == 0xE5) {
            lfn_found = false;
            entry++;
            continue;
        }

        if (entry->attributes == FAT_LFN) {
            memset(lfn_buffer, 0, sizeof(lfn_buffer));
            entry = fat32_helper_parse_lfn(entry, lfn_buffer, &expected_checksum);
            lfn_found = true;
            continue; 
        }

        if (lfn_found) {
            uint8_t actual_checksum = lfn_checksum((uint8_t*)entry->name);
            if (actual_checksum != expected_checksum) {
                lfn_found = false; 
            }
        }
        
        char* entry_name = lfn_found ? lfn_buffer : sfn_buffer;
        if (!lfn_found) fat32_parse_sfn(entry, sfn_buffer); 

        if (strcmp(filename, entry_name)) {
            lfn_found = false;
            entry++;
            continue;
        }

        vnode_t* node = new vnode_t(entry->attributes & FAT_DIR ? VDIR : VREG);
        strcpy(node->name, entry_name);
        node->parent = this_node;

        node->permissions = 0777; // FAT32 has no permission support, let alone unix permissions.
        node->io_block_size = this_node->io_block_size;
        
        node->fs_identifier = (uint64_t)fs;

        fat32_vnode_info_t* file_info = new fat32_vnode_info_t;
        file_info->parent_start_cluster = cluster;
        file_info->start_cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
        memcpy(file_info->sfn_name, entry->name, 11);

        node->file_identifier = (uint64_t)file_info;
        
        node->size = entry->file_size;
        
        // Add time and date stuff

        // Copy the operation table
        if (node->type == VDIR){
            memcpy(&node->file_operations, &fat32_dir_operations, sizeof(node->file_operations));
        } else {
            memcpy(&node->file_operations, &fat32_file_operations, sizeof(node->file_operations));
        }

        *out = node;
        break;
    }

    free(entry_table);

    return *out ? 0 : -ENOENT;
}

int fat32_vnode_read_dir(vnode_t** out, vnode_t* this_node){
    if (this_node->type != VDIR) return 0;

    filesystems::fat32_t* fs = (filesystems::fat32_t*)this_node->fs_identifier;

    uint32_t cluster = 0;
    if (this_node->file_identifier == 0){
        cluster = fs->get_root_dir_cluster();
    } else {
        // Read the entry and get its first cluster
        fat32_vnode_info_t* file_info = (fat32_vnode_info_t*)this_node->file_identifier;
        
        cluster = file_info->start_cluster;
    }


    uint64_t buffer_size = fs->get_total_size(cluster);

    fat_directory_entry_t* entry_table = (fat_directory_entry_t*)malloc(buffer_size);
    if (!entry_table) return 0;

    if (!fs->read_contents(cluster, 0, buffer_size, entry_table)){
        free(entry_table);
        return 0;
    }

    bool lfn_found = false;
    char lfn_buffer[512];
    char sfn_buffer[13];
    uint8_t expected_checksum = 0;

    vnode_t* node_chain = nullptr;
    vnode_t* last_node = nullptr;
    int node_count = 0;

    for (fat_directory_entry_t* entry = entry_table; entry->name[0] != 0; ) {        
        if ((unsigned char)entry->name[0] == 0xE5) {
            lfn_found = false;
            entry++;
            continue;
        }

        if (entry->attributes == FAT_LFN) {
            memset(lfn_buffer, 0, sizeof(lfn_buffer));
            entry = fat32_helper_parse_lfn(entry, lfn_buffer, &expected_checksum);
            lfn_found = true;
            continue; 
        }

        if (lfn_found) {
            uint8_t actual_checksum = lfn_checksum((uint8_t*)entry->name);
            if (actual_checksum != expected_checksum) {
                lfn_found = false; 
            }
        }
        
        char* entry_name = lfn_found ? lfn_buffer : sfn_buffer;
        if (!lfn_found) fat32_parse_sfn(entry, sfn_buffer); 

        vnode_t* node = new vnode_t(entry->attributes & FAT_DIR ? VDIR : VREG);
        strcpy(node->name, entry_name);
        node->parent = this_node;

        node->permissions = 0777; // FAT32 has no permission support, let alone unix permissions.
        node->io_block_size = this_node->io_block_size;
        
        node->fs_identifier = (uint64_t)fs;

        fat32_vnode_info_t* file_info = new fat32_vnode_info_t;
        file_info->parent_start_cluster = cluster;
        file_info->start_cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
        memcpy(file_info->sfn_name, entry->name, 11);

        node->file_identifier = (uint64_t)file_info;
        
        node->size = entry->file_size;
        
        // Add time and date stuff

        // Copy the operation table
        if (node->type == VDIR){
            memcpy(&node->file_operations, &fat32_dir_operations, sizeof(node->file_operations));
        } else {
            memcpy(&node->file_operations, &fat32_file_operations, sizeof(node->file_operations));
        }

        // Add to the list

        if (node_chain == nullptr){
            node_chain = node;
            last_node = node;
        } else {
            last_node->next = node;
            last_node = node;
        }
        node_count++;


        lfn_found = false;
        entry++;
    }

    free(entry_table);

    *out = node_chain;
    return node_count;
}



namespace filesystems {
    bool is_fat32(vnode_t* blk){
        fat32_extended_boot_record_t* ebr = new fat32_extended_boot_record_t;
        int ret = blk->read(FAT32_EBR_OFFSET, sizeof(fat32_extended_boot_record_t), ebr); // Load the EBR

        if (ret <= 0) return false; // I/O Error

        // Check the identifier
        bool is_fat32 = memcmp(ebr->identifier, FAT32_IDENTIFIER, sizeof(ebr->identifier)) == 0;

        delete ebr;
        return is_fat32;
    }

    fat32_t::fat32_t(vnode_t* blk){
        memset(this, 0, sizeof(fat32_t));

        this->blk = blk;

        // Load the FAT32 Structures
        _load_bpb();
        _load_ebr();
        _load_fsinfo();
        _load_fat();
    }

    fat32_t::~fat32_t(){
        // sync

        // free
        free(this->fat_table);
    }

    bool fat32_t::_load_bpb(){
        return blk->read(0, sizeof(fat_bpb_t), &this->bios_parameter_block) == sizeof(fat_bpb_t);
    }
    bool fat32_t::_load_ebr(){
        return blk->read(FAT32_EBR_OFFSET, sizeof(fat32_extended_boot_record_t), &this->extended_boot_record) == sizeof(fat32_extended_boot_record_t);
    }

    bool fat32_t::_save_bpb(){
        return blk->write(0, sizeof(fat_bpb_t), &this->bios_parameter_block) == sizeof(fat_bpb_t);
    }
    bool fat32_t::_save_ebr(){
        return blk->write(FAT32_EBR_OFFSET, sizeof(fat32_extended_boot_record_t), &this->extended_boot_record) == sizeof(fat32_extended_boot_record_t);
    }

    bool fat32_t::_load_fsinfo(){
        uint64_t fsinfo_offset = this->extended_boot_record.fs_info_sector * this->bios_parameter_block.bytes_per_sector;

        return blk->read(fsinfo_offset, sizeof(fat32_fsinfo), &this->fsinfo) == sizeof(fat32_fsinfo);
    }

    bool fat32_t::_save_fsinfo(){
        uint64_t fsinfo_offset = this->extended_boot_record.fs_info_sector * this->bios_parameter_block.bytes_per_sector;

        return blk->write(fsinfo_offset, sizeof(fat32_fsinfo), &this->fsinfo) == sizeof(fat32_fsinfo);
    }

    bool fat32_t::_load_fat(){
        uint64_t fat_size = this->extended_boot_record.sectors_per_fat * this->bios_parameter_block.bytes_per_sector; // Get the size in bytes
        this->fat_table = (uint32_t*)malloc(fat_size);

        // The FAT is located after the reserved sectors so:
        uint64_t fat_offset = bios_parameter_block.reserved_sectors * this->bios_parameter_block.bytes_per_sector;

        // Read the FAT
        return blk->read(fat_offset, fat_size, this->fat_table) == fat_size;
    }

    bool fat32_t::_save_fat(){
        if (!this->fat_table) return false;

        uint64_t fat_size = this->extended_boot_record.sectors_per_fat * this->bios_parameter_block.bytes_per_sector; // Get the size in bytes

        // The FAT is located after the reserved sectors so:
        uint64_t fat_offset = bios_parameter_block.reserved_sectors * this->bios_parameter_block.bytes_per_sector;

        // Read the FAT
        return blk->write(fat_offset, fat_size, this->fat_table);
    }

    // @brief Fetch the value 
    uint32_t fat32_t::_read_fat_entry(uint32_t cluster){
        // Get the value
        uint32_t table_value = this->fat_table[cluster];

        // Mask the high 4 bits
        return table_value & 0x0FFFFFFF;
    }

    // @brief Write a value to the FAT table (And save it to the disk)
    bool fat32_t::_write_fat_entry(uint32_t cluster, uint32_t value){
        // Get the entry
        uint32_t* entry = &this->fat_table[cluster];

        // Track usage
        // If it was free and now its not, mark the change
        if (FAT32_IS_CLUSTER_FREE(*entry) && !FAT32_IS_CLUSTER_FREE(value)){
            this->root_node->size += this->bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;

        } else if (!FAT32_IS_CLUSTER_FREE(*entry) && FAT32_IS_CLUSTER_FREE(value)){
            /* If it wasn't free and now it is, mark the change */
            this->root_node->size -= this->bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;
        }

        // Mask and write the value
        *entry = value & 0x0FFFFFFF;

        return _save_fat();
    }

    void fat32_t::free_cluster_chain(uint32_t first_cluster){
        uint32_t cluster = first_cluster;

        while(1){
            uint32_t val = _read_fat_entry(cluster);
            bool terminate = false;

            if (FAT32_IS_CLUSTER_FREE(val) || FAT32_IS_END_OF_CHAIN(val)) terminate = true;

            _write_fat_entry(cluster, 0); // Mark as free

            if (terminate) break;

            // Follow the chain
            cluster = val;
        }
    }


    uint32_t fat32_t::_get_chained_clusters_count(uint32_t cluster){
        uint32_t current_cluster = cluster;
        uint32_t chain_length = 1;

        while (true){
            uint32_t entry = _read_fat_entry(current_cluster);

            if (FAT32_IS_END_OF_CHAIN(entry) || FAT32_IS_CLUSTER_FREE(entry)) break;

            current_cluster = entry;
            chain_length++;
        }

        return chain_length;
    }

    uint64_t fat32_t::get_total_size(uint32_t start_cluster){
        return _get_chained_clusters_count(start_cluster) * this->bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;
    }

    uint32_t fat32_t::get_root_dir_cluster(){
        return this->extended_boot_record.root_dir_cluster;
    }

    uint64_t fat32_t::get_cluster_offset(uint32_t cluster) {
        if (cluster < 2) return 0; // Handle Root Dir (if Cluster 0) logic separately if needed
        
        uint64_t reserved_bytes = (uint64_t)bios_parameter_block.reserved_sectors * this->bios_parameter_block.bytes_per_sector;
        uint64_t fat_size_bytes = (uint64_t)extended_boot_record.sectors_per_fat * this->bios_parameter_block.bytes_per_sector;
        uint64_t data_start     = reserved_bytes + (bios_parameter_block.fat_count * fat_size_bytes);
        uint64_t cluster_size   = bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;

        return data_start + ((uint64_t)(cluster - 2) * cluster_size);
    }

    uint32_t fat32_t::_find_free_cluster(){
        uint32_t cluster = 0;
        uint32_t total_clusters = this->bios_parameter_block.total_sectors - this->bios_parameter_block.reserved_sectors;
        total_clusters /= this->bios_parameter_block.sectors_per_cluster;

        if (last_free_cluster == 0) last_free_cluster = 2;

        for (uint32_t clust = last_free_cluster; clust < total_clusters; clust++){
            uint32_t res = _read_fat_entry(clust);
            if (FAT32_IS_CLUSTER_FREE(res)){
                last_free_cluster = clust;
                cluster = clust;
                break;
            }
        }

        // If no free cluster was found and we started at an offset, rescan the whole table
        if (cluster == 0 && last_free_cluster > 2){
            last_free_cluster = 2;
            cluster = _find_free_cluster();
        }

        return cluster;
    }

    bool fat32_t::_zero_cluster(uint32_t cluster) {
        uint32_t cluster_size = this->bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;
        uint8_t* zero_buf = (uint8_t*)malloc(cluster_size);
        if (!zero_buf) return false;
        memset(zero_buf, 0, cluster_size);

        uint64_t offset_start = 0; 
        
        uint64_t addr = get_cluster_offset(cluster);
        blk->write(addr, cluster_size, zero_buf);

        free(zero_buf);
        return true;
    }

    bool fat32_t::is_dir_empty(uint32_t cluster) {
        uint64_t buffer_size = this->get_total_size(cluster);
        fat_directory_entry_t* entry_table = (fat_directory_entry_t*)malloc(buffer_size);
        if (!entry_table) return false; // Fail safe

        if (!read_contents(cluster, 0, buffer_size, entry_table)){
            free(entry_table);
            return false;
        }

        bool empty = true;

        for (fat_directory_entry_t* entry = entry_table; entry->name[0] != 0; entry++) {
            // Skip Deleted Entries
            if ((uint8_t)entry->name[0] == 0xE5) continue;
            
            // Skip Long File Name entries
            if (entry->attributes == FAT_LFN) continue;

            // Skip "." and ".."
            if (memcmp(entry->name, ".          ", 11) == 0) continue;
            if (memcmp(entry->name, "..         ", 11) == 0) continue;

            // If we are here, we found a valid file/folder!
            empty = false;
            break;
        }

        free(entry_table);
        return empty;
    }

    int fat32_t::read_contents(uint64_t start_cluster, uint64_t file_offset, uint64_t byte_length, void* buffer) {
        if (start_cluster < 2 && start_cluster != 0) return 0; // Invalid cluster
        if (byte_length == 0) return 0;

        uint8_t* out_buf = (uint8_t*)buffer;
        uint32_t current_cluster = (uint32_t)start_cluster;
        
        // Calculate Geometry Constants
        uint32_t bytes_per_cluster = this->bios_parameter_block.sectors_per_cluster * bios_parameter_block.bytes_per_sector;
        
        // Calculate the Byte Offset where the Data Region starts (Partition Start + Reserved + FATs)
        uint64_t fat_size_bytes = (uint64_t)this->extended_boot_record.sectors_per_fat * this->bios_parameter_block.bytes_per_sector;
        uint64_t reserved_bytes = (uint64_t)this->bios_parameter_block.reserved_sectors * this->bios_parameter_block.bytes_per_sector;
        uint64_t data_start_offset = reserved_bytes + (this->bios_parameter_block.fat_count * fat_size_bytes);

        // SEEK: Skip clusters to reach the file_offset
        uint32_t clusters_to_skip = file_offset / bytes_per_cluster;
        uint32_t cluster_offset   = file_offset % bytes_per_cluster; // Offset inside the specific cluster

        for (uint32_t i = 0; i < clusters_to_skip; i++) {
            // If we hit the end of the chain while seeking, the offset is out of bounds
            if (FAT32_IS_END_OF_CHAIN(current_cluster) || FAT32_IS_CLUSTER_FREE(current_cluster)) {
                return 0; 
            }
            current_cluster = _read_fat_entry(current_cluster);
        }

        // READ: Loop through clusters until we read all bytes
        uint64_t total_read = 0;
        uint64_t bytes_left = byte_length;

        while (bytes_left > 0) {
            // If chain ends prematurely, stop reading
            if (FAT32_IS_END_OF_CHAIN(current_cluster) || FAT32_IS_CLUSTER_FREE(current_cluster)) {
                break; 
            }

            // Calculate physical address of this cluster
            // Formula: DataStart + ((Cluster - 2) * BytesPerCluster)
            uint64_t cluster_base_addr = data_start_offset + ((uint64_t)(current_cluster - 2) * bytes_per_cluster);
            
            // Determine where to read from (Base + Offset inside cluster)
            uint64_t read_addr = cluster_base_addr + cluster_offset;

            // Determine how much to read from this cluster
            // It's either the rest of the file request OR the rest of the cluster, whichever is smaller
            uint64_t available_in_cluster = bytes_per_cluster - cluster_offset;
            uint64_t amount_to_read = min(bytes_left, available_in_cluster);

            // Perform the read
            blk->read(read_addr, amount_to_read, out_buf);

            // Advance counters
            out_buf += amount_to_read;
            bytes_left -= amount_to_read;
            total_read += amount_to_read;

            // Reset cluster_offset for subsequent clusters
            cluster_offset = 0;

            // Move to the next cluster in the chain
            if (bytes_left > 0) {
                current_cluster = _read_fat_entry(current_cluster);
            }
        }

        return total_read;
    }

    int fat32_t::write_contents(uint64_t start_cluster, uint64_t file_offset, uint64_t byte_length, const void* buffer) {
        if (start_cluster < 2 && start_cluster != 0) return 0; // Invalid cluster
        if (byte_length == 0) return 0;

        const uint8_t* in_buf = (const uint8_t*)buffer;
        uint32_t current_cluster = (uint32_t)start_cluster;

        // Calculate Geometry Constants
        uint32_t bytes_per_cluster = this->bios_parameter_block.sectors_per_cluster * bios_parameter_block.bytes_per_sector;

        // Calculate the Byte Offset where the Data Region starts
        uint64_t fat_size_bytes = (uint64_t)this->extended_boot_record.sectors_per_fat * this->bios_parameter_block.bytes_per_sector;
        uint64_t reserved_bytes = (uint64_t)this->bios_parameter_block.reserved_sectors * this->bios_parameter_block.bytes_per_sector;
        uint64_t data_start_offset = reserved_bytes + (this->bios_parameter_block.fat_count * fat_size_bytes);

        // SEEK: Skip clusters to reach the file_offset
        uint32_t clusters_to_skip = file_offset / bytes_per_cluster;
        uint32_t cluster_offset   = file_offset % bytes_per_cluster; 

        for (uint32_t i = 0; i < clusters_to_skip; i++) {
            uint32_t next_cluster = _read_fat_entry(current_cluster);

            // If we hit the end of the chain while seeking, we must extend the file
            // to fill the "gap" (Sparse files are not supported in standard FAT32)
            if (FAT32_IS_END_OF_CHAIN(next_cluster) || FAT32_IS_CLUSTER_FREE(next_cluster)) {
                uint32_t new_cluster = _find_free_cluster(); 
                if (new_cluster == 0) return 0; // Disk full

                _write_fat_entry(current_cluster, new_cluster); // Link current to new
                _write_fat_entry(new_cluster, 0x0FFFFFFF);      // Mark new as EOC
                
                current_cluster = new_cluster;
            } else {
                current_cluster = next_cluster;
            }
        }

        // WRITE: Loop through clusters writing bytes
        uint64_t total_written = 0;
        uint64_t bytes_left = byte_length;

        while (bytes_left > 0) {
            // Calculate physical address of this cluster
            uint64_t cluster_base_addr = data_start_offset + ((uint64_t)(current_cluster - 2) * bytes_per_cluster);
            
            // Determine where to write (Base + Offset inside cluster)
            uint64_t write_addr = cluster_base_addr + cluster_offset;

            // Determine how much to write to this cluster
            uint64_t available_in_cluster = bytes_per_cluster - cluster_offset;
            uint64_t amount_to_write = min(bytes_left, available_in_cluster);

            // Perform the write
            blk->write(write_addr, amount_to_write, in_buf);

            // Advance counters
            in_buf += amount_to_write;
            bytes_left -= amount_to_write;
            total_written += amount_to_write;

            // Reset cluster_offset for subsequent clusters
            cluster_offset = 0;

            // Move to the next cluster if we still have data to write
            if (bytes_left > 0) {
                uint32_t next_cluster = _read_fat_entry(current_cluster);

                // If we are at the end of the chain but still have data, allocate a new cluster
                if (FAT32_IS_END_OF_CHAIN(next_cluster) || FAT32_IS_CLUSTER_FREE(next_cluster)) {
                    uint32_t new_cluster = _find_free_cluster();
                    if (new_cluster == 0) break; // Disk full, stop writing

                    _write_fat_entry(current_cluster, new_cluster);
                    _write_fat_entry(new_cluster, 0x0FFFFFFF); 
                    
                    current_cluster = new_cluster;
                } else {
                    current_cluster = next_cluster;
                }
            }
        }

        return total_written;
    }

    bool fat32_t::load_directory_entry(uint32_t parent_cluster, char* name, fat_directory_entry_t* out){
        uint64_t buffer_size = this->get_total_size(parent_cluster);

        fat_directory_entry_t* entry_table = (fat_directory_entry_t*)malloc(buffer_size);
        if (!entry_table) return false;

        if (!read_contents(parent_cluster, 0, buffer_size, entry_table)){
            free(entry_table);
            return false;
        }

        bool found = false;

        // Loop through all entries in the directory
        for (fat_directory_entry_t* entry = entry_table; entry->name[0] != 0; entry++) {
            
            // Skip Deleted Entries
            if ((uint8_t)entry->name[0] == 0xE5) continue;

            // Skip Long File Name (LFN) Entries
            if (entry->attributes == FAT_LFN) continue;

            // Compare the 11-byte SFN directly
            if (memcmp(entry->name, name, 11) == 0) {
                memcpy(out, entry, sizeof(fat_directory_entry_t));
                found = true;
                break;
            }
        }

        free(entry_table);
        return found;
    }

    bool fat32_t::update_directory_entry(uint32_t parent_cluster, char* name, fat_directory_entry_t* in){
        uint64_t buffer_size = this->get_total_size(parent_cluster);

        fat_directory_entry_t* entry_table = (fat_directory_entry_t*)malloc(buffer_size);
        if (!entry_table) return false;

        if (!read_contents(parent_cluster, 0, buffer_size, entry_table)){
            free(entry_table);
            return false;
        }

        bool found = false;

        for (fat_directory_entry_t* entry = entry_table; entry->name[0] != 0; entry++) {
            
            // Skip Deleted Entries
            if ((uint8_t)entry->name[0] == 0xE5) continue;

            // Skip Long File Name (LFN) Entries
            if (entry->attributes == FAT_LFN) continue;

            // Compare the 11-byte SFN directly
            if (memcmp(entry->name, name, 11) == 0) {
                // Calculate the offset of this entry relative to the start of the directory file
                uint64_t entry_offset = (uint64_t)entry - (uint64_t)entry_table;
                
                // Write the updated entry back to disk
                write_contents(parent_cluster, entry_offset, sizeof(fat_directory_entry_t), in);
                found = true;
                break;
            }
        }

        free(entry_table);
        return found;
    }

    // @returns The offset where the entry was written, or -1 on failure
    int64_t fat32_t::_push_dir_entry(uint32_t parent_cluster, fat_directory_entry_t* entry) {
        fat_directory_entry_t temp;
        uint32_t offset = 0;
        bool found_spot = false;

        // Scan for a free slot (0x00 or 0xE5)
        while (true) {
            int read_res = read_contents(parent_cluster, offset, sizeof(fat_directory_entry_t), &temp);
            
            // If we read 0 bytes, we hit the end of the chain. 
            // We can append here.
            if (read_res == 0) {
                found_spot = true;
                break; 
            }

            // Check for End of Directory marker
            if (temp.name[0] == 0x00) {
                found_spot = true;
                break;
            }

            // Check for Deleted Entry (Reuse it!)
            if ((uint8_t)temp.name[0] == 0xE5) {
                found_spot = true;
                break;
            }

            offset += sizeof(fat_directory_entry_t);
        }

        if (found_spot) {
            write_contents(parent_cluster, offset, sizeof(fat_directory_entry_t), entry);
            return offset;
        }

        return -1;
    }

    int fat32_t::_link_entry(uint32_t parent_cluster, const char* name, uint32_t target_cluster, uint8_t attributes) {
    
        bool need_lfn = !_is_valid_8_3(name);
        
        char sfn[11];
        uint8_t nt_resv = 0;
        uint8_t checksum = 0;
        int lfn_entries_needed = 0;

        if (need_lfn) {
            int name_len = strlen(name);
            lfn_entries_needed = (name_len + 12) / 13;
            
            _generate_sfn(name, sfn); 
            checksum = lfn_checksum((uint8_t*)sfn);
        } else {
            // Standard 8.3 (possibly with NT capitalization)
            nt_resv = _format_8_3(name, sfn);
        }

        int total_slots = lfn_entries_needed + 1; // +1 for the SFN entry

        fat_directory_entry_t temp;
        uint32_t offset = 0;
        uint32_t hole_start_offset = 0;
        int consecutive_free = 0;
        bool found_spot = false;

        while (true) {
            int res = read_contents(parent_cluster, offset, sizeof(fat_directory_entry_t), &temp);
            
            if (res == 0 || temp.name[0] == 0x00) {
                if (consecutive_free < total_slots) {
                    hole_start_offset = offset - (consecutive_free * sizeof(fat_directory_entry_t));
                }
                found_spot = true;
                break;
            }

            if ((uint8_t)temp.name[0] == 0xE5) {
                if (consecutive_free == 0) hole_start_offset = offset;
                consecutive_free++;

                if (consecutive_free == total_slots) {
                    found_spot = true;
                    break;
                }
            } else {
                // Occupied, reset counter
                consecutive_free = 0;
            }

            offset += sizeof(fat_directory_entry_t);
        }

        if (!found_spot) return -ENOSPC;

        // Write the lfns (if needed)
        uint32_t write_offset = hole_start_offset;

        if (need_lfn) {
            for (int i = lfn_entries_needed; i > 0; i--) {
                fat_long_file_name lfn;
                memset(&lfn, 0, sizeof(lfn));

                lfn.sequence = i;
                if (i == lfn_entries_needed) lfn.sequence |= 0x40; // Last LFN marker
                lfn.attributes = 0x0F; 
                lfn.checksum = checksum;

                const char* name_ptr = name + ((i - 1) * 13);
                int chars_processed = 0;

                // Fill LFN Name (Simplified ASCII -> UTF16)
                for (int j = 0; j < 13; j++) {
                    uint16_t char_val = 0xFFFF; // Padding
                    if (chars_processed < 13 && name_ptr[j] != 0) {
                        char_val = (uint8_t)name_ptr[j];
                    } else if (chars_processed < 13 && name_ptr[j] == 0) {
                        char_val = 0x0000;
                        chars_processed = 999;
                    }

                    if (j < 5) lfn.name1[j] = char_val;
                    else if (j < 11) lfn.name2[j - 5] = char_val;
                    else lfn.name3[j - 11] = char_val;
                }

                write_contents(parent_cluster, write_offset, sizeof(lfn), &lfn);
                write_offset += sizeof(lfn);
            }
        }

        // Write the sfn entry
        fat_directory_entry_t sfn_entry;
        memset(&sfn_entry, 0, sizeof(sfn_entry));
        memcpy(sfn_entry.name, sfn, 11);
        sfn_entry.attributes = attributes;
        sfn_entry.winnt_flags = nt_resv; // Apply the capitalization flags
        sfn_entry.first_cluster_high = target_cluster >> 16;
        sfn_entry.first_cluster_low = target_cluster & 0xFFFF;
        
        write_contents(parent_cluster, write_offset, sizeof(sfn_entry), &sfn_entry);

        return 0;
    }

    uint32_t fat32_t::create_dir(uint32_t parent_cluster, char* name) {
        // Allocate a Cluster for the new directory
        uint32_t new_cluster = _find_free_cluster();
        if (new_cluster == 0) return -ENOSPC;

        // Mark chain end & Zero it out
        _write_fat_entry(new_cluster, 0x0FFFFFFF);
        _zero_cluster(new_cluster);

        // Create "." and ".." inside the NEW cluster
        fat_directory_entry_t dot;
        memset(&dot, 0, sizeof(dot));
        memcpy(dot.name, ".          ", 11);
        dot.attributes = FAT_DIR;
        dot.first_cluster_high = new_cluster >> 16;
        dot.first_cluster_low = new_cluster & 0xFFFF;

        fat_directory_entry_t dotdot;
        memset(&dotdot, 0, sizeof(dotdot));
        memcpy(dotdot.name, "..         ", 11);
        dotdot.attributes = FAT_DIR;

        uint32_t parent_ref = (parent_cluster == extended_boot_record.root_dir_cluster) ? 0 : parent_cluster;
        dotdot.first_cluster_high = parent_ref >> 16;
        dotdot.first_cluster_low = parent_ref & 0xFFFF;

        write_contents(new_cluster, 0, sizeof(dot), &dot);
        write_contents(new_cluster, 32, sizeof(dotdot), &dotdot);

        _link_entry(parent_cluster, name, new_cluster, FAT_DIR);

        return 0; 
    }

    int fat32_t::create_file(uint32_t parent_cluster, char* name, bool preallocate) {
        uint32_t new_cluster = 0;
        if (preallocate){
            // Allocate a Cluster for the new directory
            new_cluster = _find_free_cluster();
            if (new_cluster == 0) return -ENOSPC;

            // Mark chain end & Zero it out
            _write_fat_entry(new_cluster, 0x0FFFFFFF);
            _zero_cluster(new_cluster);
        }
        // Create the entry
        _link_entry(parent_cluster, name, new_cluster, 0);
        return 0; 
    }
    
    void fat32_t::_calculate_space(){
        uint32_t root_dir_sectors = ((this->bios_parameter_block.root_entry_count * 32)
        + (this->bios_parameter_block.bytes_per_sector - 1)) / this->bios_parameter_block.bytes_per_sector;
        
        uint32_t total_sectors = this->bios_parameter_block.total_sectors; // 16-bit
        if (total_sectors == 0) {
            total_sectors = this->bios_parameter_block.large_sector_count; // 32-bit
        }

        uint32_t data_sectors = total_sectors
        - (this->bios_parameter_block.reserved_sectors
        + (this->bios_parameter_block.fat_count * this->extended_boot_record.sectors_per_fat) 
        + root_dir_sectors);

        uint32_t total_clusters = data_sectors / this->bios_parameter_block.sectors_per_cluster;
        uint32_t cluster_size = this->bios_parameter_block.sectors_per_cluster * this->bios_parameter_block.bytes_per_sector;
        
        for (uint32_t i = 0; i < total_clusters; i++){
            uint32_t entry = this->fat_table[i] & 0x0FFFFFFF;

            if (entry){
                this->root_node->size += cluster_size;
            }
        }
    }

    // @brief Create a root node (Used to access the filesystem)
    vnode_t* fat32_t::create_root_node(){
        if (this->root_node) return this->root_node;
        
        vnode_t* node = new vnode_t(VDIR);
        strcpy(node->name, "FAT32_ROOT_NODE");

        node->permissions = 0777; // FAT32 has no permission support, let alone unix permissions.
        node->io_block_size = this->bios_parameter_block.bytes_per_sector;
        
        node->fs_identifier = (uint64_t)this;

        node->file_identifier = 0;
        node->partition_total_size = blk->size;
        node->size = 0;
        
        memcpy(&node->file_operations, &fat32_dir_operations, sizeof(node->file_operations));

        this->root_node = node;

        _calculate_space();
        
        return node;
    }
} // namespace filesystems
