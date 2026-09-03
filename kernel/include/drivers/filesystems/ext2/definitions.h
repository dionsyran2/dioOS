#pragma once

#define EXT2_SUPERBLOCK_OFFSET  1024
#define EXT2_SUPERBLOCK_SIZE    1024
#define EXT2_ROOT_INODE         2



#define EXT2_SIGNATURE           0xEF53


// Features
#define EXT2_REQ_FEATURE_DIR_TYPE   0x0002
// Inode type_permissions Field


// Permissions are typical unix permissions (12 bits)