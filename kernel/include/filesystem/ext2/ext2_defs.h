#pragma once

#define EXT2_SUPERBLOCK_OFFSET  1024
#define EXT2_SUPERBLOCK_SIZE    1024
#define EXT2_ROOT_INODE         2



#define EXT2_SIGNATURE           0xEF53


// Features
#define EXT2_REQ_FEATURE_DIR_TYPE   0x0002
// Inode type_permissions Field

// Types
#define EXT2_INODE_FIFO         0x1000  // FIFO / Named Pipe
#define EXT2_INODE_CHR          0x2000  // Character Device
#define EXT2_INODE_DIR          0x4000  // Directory
#define EXT2_INODE_BLK          0x6000  // Block Device
#define EXT2_INODE_REG          0x8000  // Regular File
#define EXT2_INODE_SYM          0xA000  // Symbolic Link
#define EXT2_INODE_SOC          0xC000  // Socket

// Permissions are typical unix permissions (12 bits)