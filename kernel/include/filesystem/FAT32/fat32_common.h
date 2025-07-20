#pragma once
#include <stdint.h>
#include <stddef.h>

struct FAT_BPB{
    uint8_t     jmp_instruction[3];
    char        oem_identifier[8];
    uint16_t    bytes_per_sector;
    uint8_t     sectors_per_cluster;
    uint16_t    num_of_reserved_sectors;
    uint8_t     num_of_fats; // usually 2
    uint16_t    num_of_root_directory_entries;
    uint16_t    total_sectors;
    uint8_t     media_descriptor_type;
    uint16_t    num_of_sectors_per_fat; // FAT12 - FAT16 only!
    uint16_t    num_of_sectors_per_track;
    uint16_t    num_of_heads_or_sides_on_media;
    uint32_t    num_of_hidden_sectors;
    uint32_t    large_sector_count; // used if  'total_sectors' is zero!
    // The EBR is after this
}__attribute__((packed));


struct FAT32_EBR{
    uint32_t sectors_per_fat; // The size of the fat in sectors
    uint16_t flags;
    uint16_t fat_version_number; // The high byte is the major version and the low byte is the minor version. FAT drivers should respect this field. 
    uint32_t root_dir_cluster;
    uint16_t fsinfo_sector_number;
    uint16_t backup_boot_structure_sector;
    uint8_t  rsvd[12];
    uint8_t  drive_number;
    uint8_t  winnt_flags;
    uint8_t  signature; // Either 0x28 or 0x29
    uint8_t  volume_id[4];
    char     volume_label[11];
    char     identifier_string[8]; // System identifier string. Always "FAT32   ". The spec says never to trust the contents of this string for any use. 
    char     boot_code[420];
    uint16_t bootable_partition_signature; //0xAA55
}__attribute__((packed));

struct FAT32_FSINFO{
    uint32_t    lead_signature; // 0x41615252
    uint8_t     rsvd[480];
    uint32_t    another_signature; // 0x61417272
    uint32_t    last_known_free_cluster;
    uint32_t    start_looking_for_available_clusters;
    uint8_t     rsvd1[12];
    uint32_t    tail_signature; // 0xAA550000
}__attribute__((packed));

enum FAT_ATTRIBUTES{
    FAT_READ_ONLY = 0x01,
    FAT_HIDDEN = 0x02,
    FAT_SYSTEM = 0x04,
    FAT_VOLUME_ID = 0x08,
    FAT_DIRECTORY = 0x10,
    FAT_ARCHIVE = 0x20,
    FAT_LFN = 0x0F,
};

struct FAT_DIR{ // standard 8.3 entry
    char name[11];
    uint8_t attributes;
    uint8_t winnt_rsvd; // Has information about the capitalization
    uint8_t creation_time_in_hundredths; // Creation time in hundredths of a second
    uint16_t creation_time; // The time that the file was created. Multiply Seconds by 2.
    uint16_t creation_date;
    uint16_t last_accessed_date;
    uint16_t first_cluster_high; // The high 16 bits of this entry's first cluster number.
    uint16_t last_modification_time;
    uint16_t last_modification_date;
    uint16_t first_cluster_low;
    uint32_t file_size_in_bytes;
}__attribute__((packed));


struct FAT32_LFN{ // Long file name entry
    uint8_t order; // The order of this entry in the sequence of long file name entries.
    wchar_t first_5_chars[5];
    uint8_t attributes;
    uint8_t long_entry_type;
    uint8_t short_filename_checksum;
    wchar_t middle_6_chars[6];
    uint16_t zero;
    wchar_t last_2_chars[2];
}__attribute__((packed));