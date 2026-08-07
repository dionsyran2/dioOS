#pragma once

#include <vfs/vfs.h>


struct ustar_header_t {
    char name[100];     // Filename or relative path
    char mode[8];       // File permissions (ASCII octal)
    char uid[8];        // Owner User ID (ASCII octal)
    char gid[8];        // Owner Group ID (ASCII octal)
    char size[12];      // File size in bytes (ASCII octal)
    char mtime[12];     // Last modification time (ASCII octal)
    char chksum[8];     // Checksum for header
    char typeflag;      // Entry type ('0'/'\0'=File, '5'=Dir, '2'=Symlink)
    char linkname[100]; // Target path for links
    char magic[6];      // "ustar\0" or "ustar "
    char version[2];    // "00"
    char uname[32];     // Owner Username
    char gname[32];     // Owner Groupname
    char devmajor[8];   // Device major number
    char devminor[8];   // Device minor number
    char prefix[155];   // Path prefix (for paths > 100 chars)
    char pad[12];       // Padding to reach 512 bytes
} __attribute__((packed));

namespace tarfs{
    void init_tarfs(void* file, size_t file_size);
}