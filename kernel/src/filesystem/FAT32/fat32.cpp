#include <filesystem/FAT32/fat32.h>
#include <paging/PageFrameAllocator.h>
#include <memory.h>
#include <BasicRenderer.h>
#include <drivers/rtc/rtc.h>
#include <scheduling/apic/apic.h>

namespace filesystem{

    bool is_fat32(vblk_t* blk, uint64_t start_block){
        // load the BPB and the EBR
        FAT_BPB* bpb = (FAT_BPB*)GlobalAllocator.RequestPage();
        FAT32_EBR* ebr = (FAT32_EBR*)((uint64_t)bpb + sizeof(FAT_BPB));
        memset(bpb, 0, 0x1000);

        uint32_t blocks_per_page = 0x1000 / blk->block_size;
        blk->blk_ops.read(start_block, blocks_per_page, bpb, blk);


        /* FAT32 can be distinguished from FAT12/16 by the number of entries in the root directory  */
        /* (word at offset 17 in the BPB) and/or the size of one FAT (word at offset 22) which are  */
        /* always zero on FAT32 and always non-zero on FAT12/16.                                    */
        /* https://forum.osdev.org/viewtopic.php?t=26373                                            */


        bool ret = true;

        if (bpb->num_of_root_directory_entries || bpb->num_of_sectors_per_fat) ret = false;

        if (!(ebr->signature == 0x28 || ebr->signature == 0x29)) ret = false;

        if (strncmp(ebr->identifier_string, "FAT32", 5)) ret = false; // I know the spec says not to trust it... but are there actually any tools that dont set this correctly?

        GlobalAllocator.FreePage(bpb);
        return ret;
    }

    void* fat32_load(size_t* cnt, vnode_t* this_node){
        /*fat32* fs = (fat32*)this_node->fs_sec_data;
        FAT_DIR* dir = (FAT_DIR*)this_node->fs_data;

        return fs->_load_dir(dir, cnt);*/
    }

    int fat32_write(const char* txt, size_t length, vnode_t* this_node){
        /*fat32* fs = (fat32*)this_node->fs_sec_data;
        FAT_DIR* dir = (FAT_DIR*)this_node->fs_data;
        return fs->write_data(dir, (FAT_DIR*)this_node->parent->fs_data, (char*)txt, length);*/
    }

    int fat32_create_dir(const char* fn, bool directory, vnode_t* this_node){
        /*fat32* fs = (fat32*)this_node->fs_sec_data;
        FAT_DIR* dir = (FAT_DIR*)this_node->fs_data;
        FAT_DIR* dirEntry = fs->_make_dir(dir, directory, (char*)fn);
        if (dirEntry != nullptr){
            vnode_t* node = fs->_create_vnode_from_fat_dir(dirEntry, (char*)fn);
            this_node->children[this_node->num_of_children] = node;
            this_node->num_of_children++;
        }
        return dirEntry != nullptr ? 0 : -1;*/
    }

    fat32::fat32(vblk_t* blk, uint64_t start_block){
        this->disk = blk;
        this->start_block = start_block;
        _init();
    }

    uint32_t fat32::calculate_block(uint32_t cluster){
        uint32_t first_data_sector = bpb->num_of_reserved_sectors + (bpb->num_of_fats * ebr->sectors_per_fat);
        uint32_t sector = first_data_sector + ((cluster - 2) * bpb->sectors_per_cluster);
        sector += start_block;
        return sector;
    }

    uint32_t fat32::get_next_cluster(uint32_t cluster){
        //remember to ignore the high 4 bits.
        uint32_t offset = cluster * 4;
        uint32_t table_value = *(uint32_t*)&file_allocation_table[offset];
        table_value &= 0x0FFFFFFF;
                
        if (table_value == 0x00000000) {
            //Cluster Free
        } else if (table_value >= 0x0FFFFFF8 && table_value <= 0x0FFFFFFF) {
            return 0; //End of chain
        } else if (table_value == 0x0FFFFFF7) {
            return 0; //Bad Cluster
        } else if (table_value >= 0x0FFFFFF0 && table_value <= 0x0FFFFFF6) {
            return 0; //Reserved Cluster
        } else {
            return table_value; //Next Cluster
        }
        return 0;
    }

    void fat32::_mark_cluster(uint32_t cluster, uint32_t data){ // either the next cluster or 0x0FFFFFFF for end of chain
        uint32_t offset = cluster * 4;
        *(uint32_t*)&file_allocation_table[offset] = (data & 0x0FFFFFFF);
    }

    bool fat32::_is_cluster_free(uint32_t cluster){
        uint32_t offset = cluster * 4;
        uint32_t table_value = *(uint32_t*)&file_allocation_table[offset];
        table_value &= 0x0FFFFFFF;
                
        if (table_value == 0x00000000) return true;
        return false;
    }

    uint32_t fat32::get_amount_of_chained_cluster(uint32_t start_cluster){
        uint32_t cluster = start_cluster;
        uint32_t count = 0;

        while (cluster != 0){
            cluster = get_next_cluster(cluster);
            count++;
        }

        return count;
    }
    
    void fat32::_parse_data(){
        bpb = (FAT_BPB*)GlobalAllocator.RequestPage();
        ebr = (FAT32_EBR*)((uint64_t)bpb + sizeof(FAT_BPB));
        memset(bpb, 0, 0x1000);

        blocks_per_page = 0x1000 / disk->block_size;
        disk->blk_ops.read(start_block, blocks_per_page, bpb, disk);

        fat_start_block = start_block + bpb->num_of_reserved_sectors;
        fat_size_in_blocks = ebr->sectors_per_fat;
        fat_size_in_bytes = fat_size_in_blocks * disk->block_size;  
    }

    void fat32::_load_fat(){
        file_allocation_table = (uint8_t*)GlobalAllocator.RequestPages((fat_size_in_bytes / 0x1000) + 1);
        memset(file_allocation_table, 0, (fat_size_in_bytes / 0x1000) + 1);
        for (int i = 0; i < fat_size_in_blocks; i++){
            disk->blk_ops.read(fat_start_block + i, 1, (file_allocation_table + (disk->block_size * i)), disk);
        }
    }

    void fat32::_save_fat(){
        for (int i = 0; i < fat_size_in_blocks; i++){
            disk->blk_ops.write(fat_start_block + i, 1, (file_allocation_table + (disk->block_size * i)), disk);
        }
    }

    void fat32::_init(){
        /*_parse_data();
        _load_fat();
        
        vnode_t* fs = vfs::get_root_node();
        if (true){ // check if this should be mounted as the root partition, if not change the fs
            vnode_t* mnt = vfs::resolve_path("/mnt");
            if (mnt == nullptr){
                mnt = vfs::mount_node("mnt", VDIR, nullptr);
            }
            
            char* name = new char[strlen(disk->node.name) + 2];
            strcpy(name, disk->node.name);
            strcat(name, toString((uint8_t)(disk->num_of_partitions + 1)));
            disk->num_of_partitions++;
            fs = vfs::mount_node(name, VDIR, mnt);
        }

        fs->fs_sec_data = (void*)this;
        fs->fs_data = nullptr;
        //fs->ops.read_dir = fat32_list_dir;
        fs->ops.load = fat32_load;
        fs->ops.write = fat32_write;
        fs->ops.create_subdirectory = fat32_create_dir;
        _mount_dir(nullptr, fs);*/
    }

    uint8_t* fat32::_load_dir(FAT_DIR* dir, size_t* cnt){
        uint32_t cluster = 0;
        if (dir == nullptr){
            cluster = ebr->root_dir_cluster;
        }else{
            cluster = dir->first_cluster_high;
            cluster <<= 16;
            cluster |= dir->first_cluster_low;
        }

        uint32_t chained_clusters = get_amount_of_chained_cluster(cluster);
        
        uint8_t* data = (uint8_t*)GlobalAllocator.RequestPages((chained_clusters * bpb->sectors_per_cluster * disk->block_size) / 0x1000);
        uint64_t offset = 0;
        while (cluster != 0){
            disk->blk_ops.read(calculate_block(cluster), bpb->sectors_per_cluster, data + offset, disk);
            offset += bpb->sectors_per_cluster * disk->block_size;

            cluster = get_next_cluster(cluster);
        }

        uint8_t* heap_memory = new uint8_t[offset + 0x1000];
        heap_memory += (0x1000 - ((uint64_t)heap_memory & 0xFFF)); // I have issues with musl so lets align it :)
        memcpy(heap_memory, data, offset);

        GlobalAllocator.FreePages(data, (chained_clusters * bpb->sectors_per_cluster * disk->block_size) / 0x1000);
        *cnt = offset;

        return heap_memory;
    }

    int fat32::_list_dir(FAT_DIR* dir, vnode_t** nodes, size_t* out_count){
        size_t count = 0;
        FAT_DIR* data = (FAT_DIR*)_load_dir(dir, &count);
        size_t int_cnt = 0;

        for (int i = 0; i < (count / sizeof(FAT_DIR)); i++){
            FAT_DIR* directory = &data[i];
            
            if (directory == NULL) continue;
            if (directory->name[0] == 0x00) break;
            if ((uint8_t)directory->name[0] == 0xE5) continue;
            if (directory->attributes == FAT_LFN) continue;
            if (directory->name[0] == '.'){
                if(directory->name[1] == '.' || directory->name[1] == ' '){
                    continue;
                }
            }

            vnode_t* node = _create_vnode_from_fat_dir(directory, _parse_name(data, i));
            nodes[int_cnt] = node;
            int_cnt++;
        }
        *out_count = int_cnt;

        return 0;
    }

    int fat32::_mount_dir(FAT_DIR* dir, vnode_t* parent){
        size_t count = 0;
        FAT_DIR* data = (FAT_DIR*)_load_dir(dir, &count);
        size_t int_cnt = 0;

        for (int i = 0; i < (count / sizeof(FAT_DIR)); i++){
            FAT_DIR* directory = &data[i];
            
            if (directory == NULL) continue;
            if (directory->name[0] == 0x00) break;
            if ((uint8_t)directory->name[0] == 0xE5) continue;
            if (directory->attributes == FAT_LFN) continue;
            if (directory->name[0] == '.'){
                if(directory->name[1] == '.' || directory->name[1] == ' '){
                    continue;
                }
            }

            vnode_t* node = _create_vnode_from_fat_dir(directory, _parse_name(data, i));
            node->parent = parent;
            //parent->children[parent->num_of_children] = node;
            //parent->num_of_children++;

            if (directory->attributes == FAT_DIRECTORY) _mount_dir(directory, node);
        }

        return 0;
    }

    uint32_t fat32::_find_empty_cluster(){
        uint32_t total_clusters = bpb->total_sectors;
        if (total_clusters == 0) total_clusters = bpb->large_sector_count;

        total_clusters /= bpb->sectors_per_cluster;

        for (int cluster = 2; cluster < total_clusters; cluster++){
            if (_is_cluster_free(cluster)) return cluster;
        }

        return 0;
    }

    int fat32::write_data(FAT_DIR* dir, FAT_DIR* parent, char* data, size_t cnt){
        uint32_t cluster = 0;
        uint32_t prev_cluster = 0;
        
        if (dir == nullptr){
            cluster = ebr->root_dir_cluster;
        }else{
            cluster = dir->first_cluster_high;
            cluster <<= 16;
            cluster |= dir->first_cluster_low;

            if (cluster == 0){
                // Its an empty file, lets allocate a cluster :)
                cluster = _find_empty_cluster();
                _mark_cluster(cluster, 0x0FFFFFFF);

                dir->first_cluster_high = cluster >> 16;
                dir->first_cluster_low = cluster & 0xFFFF;
            }
        }

        uint64_t offset = 0;
        size_t pages = ((bpb->sectors_per_cluster * disk->block_size) / 0x1000) + 1;
        void* tmp = GlobalAllocator.RequestPages(pages);



        while (cluster != 0 && offset < cnt){
            size_t amount_to_copy =  bpb->sectors_per_cluster * disk->block_size;
            if ((offset + amount_to_copy) > cnt) amount_to_copy = cnt - offset;
            memset(tmp, 0, pages * 0x1000);
            memcpy(tmp, data + offset, amount_to_copy);


            size_t block = calculate_block(cluster);
            disk->blk_ops.write(block, bpb->sectors_per_cluster, tmp, disk);

            offset += amount_to_copy;

            prev_cluster = cluster;
            cluster = get_next_cluster(cluster);
            if (cluster == 0 && offset < cnt){ // allocate space
                cluster = _find_empty_cluster();
                if (cluster != 0){
                    _mark_cluster(prev_cluster, cluster); // Set the previous to point to the next one
                    _mark_cluster(cluster, 0x0FFFFFFF); // Mark the next one as end of chain
                }
            }
        }

        while (cluster != 0){
            uint32_t nclust = get_next_cluster(cluster);
            _mark_cluster(cluster, 0); // free remaining clusters
            cluster = nclust;
        }

        uint32_t pclust = 0;
        if (parent == nullptr){
            pclust = ebr->root_dir_cluster;
        }else{
            pclust = parent->first_cluster_high;
            pclust <<= 16;
            pclust |= parent->first_cluster_low;
        }

        if (pclust != 0){
            size_t count = 0;
            FAT_DIR* data = (FAT_DIR*)_load_dir(parent, &count);

            for (int i = 0; i < (count / sizeof(FAT_DIR)); i++){
                FAT_DIR* directory = &data[i];
                
                if (directory == NULL) continue;
                if (directory->name[0] == 0x00) break;
                if (memcmp(dir->name, directory->name, 11) == 0){
                    directory->file_size_in_bytes = cnt;

                    RTC::rtc_time_t time = RTC::read_rtc_time();
                    directory->last_modification_time = _convert_time(&time);
                    directory->last_modification_date = _convert_date(&time);
                    directory->first_cluster_high = dir->first_cluster_high;
                    directory->first_cluster_low = dir->first_cluster_low;

                    break;
                }
            }

            uint32_t write_cluster = pclust;
            uint32_t remaining_bytes = count;
            size_t dirent_bytes_per_cluster = bpb->sectors_per_cluster * disk->block_size;

            uint8_t* ptr = (uint8_t*)data;
            while (write_cluster < 0x0FFFFFF8 && remaining_bytes > 0) {
                size_t to_write = (remaining_bytes > dirent_bytes_per_cluster) ? dirent_bytes_per_cluster : remaining_bytes;
                memset(tmp, 0, pages * 0x1000);
                memcpy(tmp, ptr, to_write);
                disk->blk_ops.write(calculate_block(write_cluster), bpb->sectors_per_cluster, tmp, disk);
                
                ptr += to_write;
                remaining_bytes -= to_write;
                write_cluster = get_next_cluster(write_cluster);
            }
        }

        _save_fat();
        return 0;
    }

    vnode_t* fat32::_create_vnode_from_fat_dir(FAT_DIR* dir, char* name){
        vnode_t* node = new vnode_t;
        memset(node, 0, sizeof(vnode_t));

        strcpy(node->name, name);
        //node->fs_data = (void*)dir;
        //node->fs_sec_data = (void*)this;
        node->type = dir->attributes == FAT_DIRECTORY ? VDIR : VREG;
        node->size = dir->file_size_in_bytes;
        node->ops.read_dir = vfs::def_read_dir;
        //node->ops.load = fat32_load;
        node->ops.write = fat32_write;
        //node->ops.create_subdirectory = fat32_create_dir;
        return node;
    }

    char* fat32::_parse_name(FAT_DIR* data, int i){
        if (data[i - 1].attributes == FAT_LFN){
            char* name = new char[64];
            FAT32_LFN *lne = (FAT32_LFN *)&data[i - 1];
            uint8_t chk = lne->short_filename_checksum;

            int lnOff = 1;
            int off = 0;
            while (true)
            {
                int x = 0;
                FAT32_LFN *ln = (FAT32_LFN *)&data[i - lnOff];
                if (ln->short_filename_checksum != chk)
                    break;

                while (x < 5)
                {
                    name[off] = (char)ln->first_5_chars[x];
                    x += 1;
                    off++;
                }
                x = 0;
                while (x < 6)
                {
                    name[off] = (uint8_t)ln->middle_6_chars[x];
                    x += 1;
                    off++;
                }
                x = 0;
                while (x < 2)
                {
                    name[off] = (uint8_t)ln->last_2_chars[x];
                    x += 1;
                    off++;
                }
                lnOff++;
            }
            name[off] = '\0';
            return name;
        }else{
            char* filename = (char*)malloc(13);
            int pos = 0;
            char* sfn = data[i].name;
            uint8_t ntresv = data[i].winnt_rsvd;
            // Base name: 8 chars max
            for (int i = 0; i < 8 && sfn[i] != ' '; ++i) {
                char c = sfn[i];
                if (ntresv & (1 << 3)) c = to_lower(c);
                filename[pos++] = c;
            }

            // Extension: only add if not all spaces
            bool has_ext = false;
            for (int i = 0; i < 3; ++i) {
                if (sfn[8 + i] != ' ') {
                    has_ext = true;
                    break;
                }
            }

            if (has_ext) {
                filename[pos++] = '.';
                for (int i = 0; i < 3 && sfn[8 + i] != ' '; ++i) {
                    char c = sfn[8 + i];
                    if (ntresv & (1 << 4)) c = to_lower(c);
                    filename[pos++] = c;
                }
            }

            filename[pos] = '\0';
            return filename;
        }
    }

    bool fat32::_has_dir(FAT_DIR* dir, char* name, bool sfn){
        size_t count = 0;
        FAT_DIR* data = (FAT_DIR*)_load_dir(dir, &count);
        size_t int_cnt = 0;

        for (int i = 0; i < (count / sizeof(FAT_DIR)); i++){
            FAT_DIR* directory = &data[i];
            
            if (directory == NULL) continue;
            if (directory->name[0] == 0x00) break;
            if ((uint8_t)directory->name[0] == 0xE5) continue;
            if (directory->attributes == FAT_LFN) continue;

            if (sfn){
                if (strncmp(directory->name, name, 11) == 0)
                    return true;
            }else{
                if (strcmp(_parse_name(data, i), name) == 0)
                    return true;
            }
        }

        return false;
    }

    char* fat32::_conv_name_to_sfn(char* name, uint8_t suffix, uint8_t* winnt) {
        if (suffix > 9) suffix = 9;

        char* out = new char[12]; // 11 chars + null
        out[11] = '\0';

        // Find dot position (extension separator)
        int dot = -1;
        int length = strlen(name);
        for (int i = 0; i < length; i++) {
            if (name[i] == '.') {
                dot = i;
                break;
            }
        }

        auto valid_char = [](char c) -> bool {
            static const char illegal[] = "\"+,;=[]|*?\\/: ";
            if (c >= 'a' && c <= 'z') return true;
            if (c >= 'A' && c <= 'Z') return true;
            if (c >= '0' && c <= '9') return true;
            for (char ic : illegal) {
                if (c == ic) return false;
            }
            return true;
        };

        auto get_case = [](char c) -> bool {
            if (c >= 'a' && c <= 'z') return 0;
            if (c >= 'A' && c <= 'Z') return 1;
            return -1;
        };

        int base_len = (dot == -1 ? length : dot);
        int max_base_len = (base_len > 8) ? 6 : base_len;

        uint8_t winnt_resv = 0;

        if (base_len == max_base_len){
            bool all_same = true;
            int last_case = -1;

            for (int i = 0; i < base_len; i++){
                char c = name[i];

                if (last_case == -1){
                    last_case = get_case(c);
                }else{
                    if (last_case != (get_case(c) != -1 ? get_case(c) : last_case)){
                        all_same = false;
                        break;
                    }
                }
            }
            if (last_case == 0 && all_same == true){
                winnt_resv |= 1 << 3;        
            }
        }
        

        int out_pos = 0;
        int src_pos = 0;
        while (src_pos < base_len && out_pos < max_base_len) {
            char c = name[src_pos++];
            if (c == ' ' || !valid_char(c)) continue;
            out[out_pos++] = to_upper(c);
        }

        if (base_len > 8) {
            // Pad to 6 chars if needed
            while (out_pos < 6) out[out_pos++] = ' ';
            out[out_pos++] = '~';
            out[out_pos++] = '1' + suffix - 1;
        } else {
            // Pad remainder of base name with spaces
            while (out_pos < 8) out[out_pos++] = ' ';
        }

        if (dot != -1) {
            int ext_pos = dot + 1;
            int ext_out = 8;
            int ext_count = -1;
            bool all_same = true;
            int last_case = -1;

            while (ext_pos < length && ext_count < 3) {
                char c = name[ext_pos++];
                if (c == ' ' || !valid_char(c)) continue;
                
                if (last_case == -1){
                    last_case = get_case(c);
                }else{
                    if (last_case != (get_case(c) != -1 ? get_case(c) : last_case)){
                        all_same = false;
                        break;
                    }
                }

                out[ext_out++] = to_upper(c);
                ext_count++;
            }
            while (ext_count < 3) {
                out[ext_out++] = ' ';
                ext_count++;
            }

            if (last_case == 0 && all_same == true){
                winnt_resv |= 1 << 4;
            }
        } else {
            out[8] = ' ';
            out[9] = ' ';
            out[10] = ' ';
        }

        *winnt = winnt_resv;
        return out;
    }

    uint16_t fat32::_convert_date(RTC::rtc_time_t* time){
        uint16_t out = 0;
        out |= time->day;
        out |= time->month << 5;
        out |= (time->year - 1980) << 9;
        return out;
    }

    uint16_t fat32::_convert_time(RTC::rtc_time_t* time){
        uint16_t out = 0;
        out |= ((time->second) != 0 ? (time->second - 1) / 2 : 0);
        out |= (time->minute) << 5;
        out |= (time->hour) << 11;
        return out;
    }

    bool fat32::_createlfns(FAT32_LFN** array, size_t* count, char* name){
        return true;
    }

    /* Just add the necessary entries at the end of the parent's entries, find a sector, create '.' and '..' entries with their respective sectors in the new dir */
    FAT_DIR* fat32::_make_dir(FAT_DIR* parent, bool type, char* name){
        
        uint32_t cluster = 0;
        if (parent == nullptr){
            cluster = ebr->root_dir_cluster;
        }else{
            cluster = parent->first_cluster_high;
            cluster <<= 16;
            cluster |= parent->first_cluster_low;
        }


        size_t count = 0;
        FAT_DIR* data = (FAT_DIR*)_load_dir(parent, &count);

        size_t cnt = 0;

        for (int i = 0; i < (count / sizeof(FAT_DIR)); i++){
            FAT_DIR* directory = &data[i];
            
            if (directory == NULL) continue;
            if (directory->name[0] == 0x00) break;
            cnt++;
        }


        if ((cnt + 1) * sizeof(FAT_DIR) >= count) {
            kprintf("We have to allocate a new cluster to add this entry!\n");
        }


        FAT_DIR* dir = &data[cnt];
        memset(dir, 0, sizeof(FAT_DIR));

        char* sfn;
        uint8_t winnt = 0;
        for (int i = 1; i < 10; i++){
            sfn = _conv_name_to_sfn(name, i, &winnt);
            if (!_has_dir(parent, sfn, true)) break;
        }

        memcpy(dir->name, sfn, 11);

        dir->attributes = type ? FAT_DIRECTORY : FAT_ARCHIVE;
        dir->winnt_rsvd = winnt;
        RTC::rtc_time_t time = RTC::read_rtc_time();
        dir->creation_date = _convert_date(&time);
        dir->last_accessed_date = _convert_date(&time);
        dir->last_modification_date = _convert_date(&time);
        dir->creation_time = _convert_time(&time);
        dir->last_modification_time = _convert_time(&time);


        FAT_DIR* end_dir = &data[cnt + 1]; // we overwrote the end directory so time to recreate it :)
        memset(end_dir, 0, sizeof(FAT_DIR));

        if (type){
            uint32_t free_cluster = _find_empty_cluster();
            _mark_cluster(free_cluster, 0x0FFFFFFF); // mark the cluster as 'end of chain'

            dir->first_cluster_high = free_cluster >> 16;
            dir->first_cluster_low = free_cluster & 0xFFFF;

            FAT_DIR* new_data = (FAT_DIR*)GlobalAllocator.RequestPage();
            memset(new_data, 0, 512); // clear the sector

            // "." entry
            memcpy((char*)new_data[0].name, ".          ", 11);
            new_data[0].attributes = FAT_DIRECTORY;
            new_data[0].first_cluster_high = free_cluster >> 16;
            new_data[0].first_cluster_low = free_cluster & 0xFFFF;

            // ".." entry
            memcpy((char*)new_data[1].name, "..         ", 11);
            new_data[1].attributes = FAT_DIRECTORY;
            new_data[1].first_cluster_high = parent->first_cluster_high;
            new_data[1].first_cluster_low = parent->first_cluster_low;

            write_data(dir, parent, (char*)new_data, 0x1000);
            //disk->blk_ops.write(calculate_block(free_cluster), blocks_per_page, new_data, disk); // PRAY
        } else {
            // File: leave cluster and size = 0, explicitly shown
            dir->first_cluster_high = 0;
            dir->first_cluster_low = 0;
            dir->file_size_in_bytes = 0;
        }

        void* mem = GlobalAllocator.RequestPages((count / 0x1000) + 1);
        memcpy(mem, data, count);

        write_data(parent, nullptr, (char*)mem, count);
        GlobalAllocator.FreePages(mem, (count / 0x1000) + 1);
        //disk->blk_ops.write(calculate_block(cluster), (count / 0x1000) * blocks_per_page, mem, disk); // PRAY

        return dir;
    }
}