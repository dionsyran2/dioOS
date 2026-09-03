#include <drivers/partitioning/common.h>
#include <memory.h>
#include <limine.h>
#include <memory/heap.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <kerrno.h>
#include <cstr.h>
#include <kstdio.h>
#include <drivers/filesystems/common.h>

extern volatile struct limine_kernel_file_request kernel_file_request;

vnode_t *create_node_for_partition(partition_t* pent, char *name, int partition_id, bool p_prefix);


namespace gpt{
    bool is_gpt(vnode_t* blk, uint32_t block_size){
        // If its a gpt partitioned disk, the first 8 bytes of lba 1 will be "EFI PART"

        // Allocate the struct
        partition_table_header_t buffer;

        // Read LBA 1
        int ret = blk->read(&buffer, sizeof(partition_table_header_t), 1 * block_size);

        // If the read operation failed, return
        if (ret <= 0) return false;

        // If it does not match, its not a gpt partition
        if (memcmp(GPT_SIGNATURE, buffer.signature, sizeof(buffer.signature))) return false;

        // Otherwise it is!
        
        return true;
    }

    void initialize_partitions(vnode_t* blk, char *name, uint32_t block_size, bool p_prefix){
        // Allocate the struct
        partition_table_header_t* header = new partition_table_header_t;

        // Read LBA 1

        int ret = blk->read(header, sizeof(partition_table_header_t), 1 * block_size);
        bool is_gpt = memcmp(GPT_SIGNATURE, header->signature, sizeof(header->signature)) == 0;
        // If the read operation failed or the layout is not gpt, return
        if (ret <= 0 || !is_gpt) {
            delete header;
            return;
        }

        // Check if its the boot drive
        bool is_boot = memcmp(header->disk_guid, &kernel_file_request.response->kernel_file->gpt_disk_uuid, 16) == 0;

        // Define a couple of values
        uint64_t partition_array_lba = header->partition_entry_table_lba ? header->partition_entry_table_lba : 2; // Fallback to 2 if its 0 (For some reason)
        uint64_t partition_entry_size = header->partition_entry_size;
        uint64_t partition_count = header->partition_count;
        uint64_t total_array_size = partition_count * partition_entry_size;

        // Allocate the buffer to store the array
        void* array = malloc(total_array_size);

        // Perform the read
        ret = blk->read(array, total_array_size, partition_array_lba * block_size);

        // If the I/O operation failed, return
        if (ret <= 0){
            delete header;
            free(array);
            return;
        }

        uint8_t unused_type[16] = { 0 };

        int partition = 1;

        bool mounted_root = false;

        for (uint64_t offset = 0; offset < total_array_size; offset += partition_entry_size){
            partition_entry_t* entry = (partition_entry_t*)((uint64_t)array + offset);
            if (memcmp(unused_type, entry->partition_type_guid, 16) == 0) continue; // Unused entry

            // Check if its the esp
            bool is_esp = is_boot && (memcmp(entry->partition_guid, &kernel_file_request.response->kernel_file->gpt_part_uuid, 16) == 0);

            partition_t* pent = new partition_t;
            pent->starting_lba = entry->starting_lba;
            pent->ending_lba = entry->ending_lba;
            pent->lba_size = block_size;
            
            blk->open();
            pent->node = blk;

            vnode_t* node = create_node_for_partition(pent, name, partition, p_prefix);
            partition++;

            if (!node) continue;

            if (is_esp && is_boot){
                filesystem_t *fs = find_filesystem(node);

            } else if (is_boot && !mounted_root){
                filesystem_t *fs = find_filesystem(node);

                dentry_t *f = fs->mount();
                vfs::mkdir("/ext2", 0777);

                if (f) {
                    dentry_t *m = vfs::resolve_path_dentry("/ext2");
                    if (m){
                        vfs::mount(m, f, node);
                        m->unref();
                    }
                    f->unref();
                }
            }
            node->close();
        }

        delete header;
        free(array);
    }
} // namespace gpt

namespace mbr{
    bool is_mbr(vnode_t* blk, uint32_t block_size) {
        kprintf("Checking mbr\n");
        // Allocate buffer
        mbr_sector_t* mbr = new mbr_sector_t;
        
        // Read LBA 0
        uint64_t bs = block_size ? block_size : 512;
        int ret = blk->read(mbr, sizeof(mbr_sector_t), 0);
        
        if (ret <= 0) {
            delete mbr;
            return false;
        }

        // Check Magic Signature (0xAA55)
        if (mbr->signature != 0xAA55) {
            delete mbr;
            return false;
        }

        // Check for GPT Protective MBR
        // If type is 0xEE, this is actually a GPT disk. 
        // Return false so the GPT parser handles it instead.
        if (mbr->entries[0].type == 0xEE) {
            delete mbr;
            return false; 
        }

        // It is a valid Legacy MBR disk!
        delete mbr;
        return true;
    }

    void initialize_partitions(vnode_t* blk, char *name, uint32_t block_size, bool p_prefix) {
        mbr_sector_t* mbr = new mbr_sector_t;
        uint64_t bs = block_size ? block_size : 512;
        
        blk->read(mbr, sizeof(mbr_sector_t), 0);

        int partition = 1;

        // Iterate over the 4 primary partitions
        for (int i = 0; i < 4; i++) {
            partition_entry_t* entry = &mbr->entries[i];

            // Type 0 means unused/empty
            if (entry->type == 0) continue;

            // Create the partition struct
            partition_t* pent = new partition_t;
            pent->starting_lba = entry->lba_start;
            pent->ending_lba = entry->lba_start + entry->sector_count - 1; // Inclusive end
            pent->lba_size = block_size;

            blk->open();
            pent->node = blk;

            vnode_t *node = create_node_for_partition(pent, name, partition, p_prefix);
            node->close();
            partition++;
        }

        delete mbr;
    }
} // namespace mbr

// @brief Reads length bytes from offset offset
int partition_read(void *context, void *buffer, size_t length, size_t offset){
    partition_t* pent = (partition_t*)context;
    
    // Calculate the Partition Size (in bytes)
    uint64_t partition_size = (pent->ending_lba - pent->starting_lba + 1) * pent->lba_size;

    // Bounds Check (Relative)
    if (offset >= partition_size) return 0;

    // Truncate Length
    if ((offset + length) > partition_size){
        length = partition_size - offset;
    }

    uint64_t abs_start_byte = pent->starting_lba * pent->lba_size;
    uint64_t final_phys_offset = abs_start_byte + offset;

    // Read from Parent Node (Raw Disk)
    return pent->node->read(buffer, length, final_phys_offset);
}

// @brief Writes length bytes from offset offset
int partition_write(void *context, const void *buffer, size_t length, size_t offset){
    partition_t* pent = (partition_t*)context;

    uint64_t partition_size = (pent->ending_lba - pent->starting_lba + 1) * pent->lba_size;

    // Bounds Check
    if (offset >= partition_size) return -EIO; // Cannot write past end of device

    // Truncate Length
    if ((offset + length) > partition_size){
        length = partition_size - offset;
    }

    // Calculate Absolute Offset
    uint64_t abs_start_byte = pent->starting_lba * pent->lba_size;
    uint64_t final_phys_offset = abs_start_byte + offset;

    return pent->node->write(buffer, length, final_phys_offset);
}

devfs_ops_t partition_operations = {
    .read = partition_read,
    .write = partition_write
};

vnode_t *create_node_for_partition(partition_t* pent, char *name, int partition_id, bool p_prefix){
    char pathname[128];
    stringf(pathname, sizeof(pathname), "/%s%s%d", name, p_prefix ? "p" : "", partition_id);

    int r = devfs::mknod(pathname, S_IFBLK | 0766, &partition_operations, pent);
    return devfs::resolve_path(pathname);
}


// @brief It will read the block device, detect partitioning layout (MBR/GPT)
// and create vfs entries for any partitions detected. If its the boot drive
// The main ext2 partition will be mounted at root (/) and the first fat32 (BOOT PART) as (/boot)
void intialize_disk_partitions(vnode_t* blk, uint32_t block_size, char *name, bool p_prefix){
    if (gpt::is_gpt(blk, block_size)){
        gpt::initialize_partitions(blk, name, block_size, p_prefix);
    } else if (mbr::is_mbr(blk, block_size)){
        mbr::initialize_partitions(blk, name, block_size, p_prefix);
    } else {
        //create_root_node_fs(blk); // It could still contain a filesystem
    }
}