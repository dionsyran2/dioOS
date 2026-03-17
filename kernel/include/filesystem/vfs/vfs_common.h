#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>

#define FILE_PERMISSIONS_X 1
#define FILE_PERMISSIONS_W 2
#define FILE_PERMISSIONS_R 4

struct vnode_t;


enum vnode_type_t{
    VDIR,
    VREG,
    VCHR,
    VLNK,
    VPIPE,
    VSOC,
    VBLK
};

struct vnode_ops_t{
    // @brief Returns the path as a string (Heap Allocated)
    char* (*read_link)(vnode_t* this_node);

    // @brief Creates a link
    int (*create_link)(char* name, char* target, vnode_t* this_node);

    // @brief Reads length bytes from offset offset
    int (*read)(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node);

    // @brief Writes length bytes from offset offset
    int (*write)(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node);

    // @brief Truncates the file to a specific length (Does not expand)
    int (*truncate)(uint64_t length, vnode_t* this_node);

    // @warning For directories only
    // @brief Puts a linked list in ret and returns the amount of entries.
    int (*read_dir)(vnode_t** ret, vnode_t* this_node);

    // @brief Creates a directory
    int (*mkdir)(char* name, vnode_t* this_node);

    // @brief Creates a file
    int (*creat)(char* name, vnode_t* this_node);

    // @brief Unlinks (Or deletes if links are not supported) a file
    int (*unlink)(vnode_t* this_node);
    int (*rmdir)(vnode_t* this_node);

    int (*open)(vnode_t* this_node);
    int (*close)(vnode_t* this_node);

    int (*ioctl)(int op, char* argp, vnode_t* this_node);

    // --- Called right before the vnode is deleted (cleanup memory) --- //
    int (*cleanup)(vnode_t* this_node);

    void (*save_metadata)(vnode_t* this_node);

    // --- Filesystem Specific (ROOT NODE Only) --- //
    // @brief Prepare the filesystem before it gets mounted (In case things need to get cached etc.)
    void (*mount)(vnode_t* this_node);
    // @brief Prepare the filesystem to get unmounted (Free and write caches, etc.)
    void (*unmount)(vnode_t* this_node);
    // @brief Sync changes to the disk
    void (*sync)(vnode_t* this_node);
};

struct vnode_t{
    char name[128];

    vnode_type_t type;
    uint32_t ref_count;

    // Do not mess up the child chain :)
    spinlock_t child_list_lock;

    // Waiting
    bool ready_to_receive_data;
    bool data_ready_to_read;
    
    // @brief Do not clear it when ref_count reaches 0
    bool virtual_file;
    
    // --- Permissions --- //
    uint16_t permissions; // e.g., 0755 (rwxr-xr-x)
    uint32_t uid;         // Owner User ID
    uint32_t gid;         // Owner Group ID

    // --- Dates --- //
    uint64_t last_accessed;
    uint64_t last_modified;
    uint64_t creation_time;

    // --- Size --- //
    uint64_t size; // Size on the root node of a filesystem is its usage stats.
    uint64_t partition_total_size; // Used for root nodes, to note the total size of the partition
    
    uint64_t io_block_size;

    // --- Type Specific --- //
    bool fs_root_node;
    bool is_mounted;
    vnode_t* mount_point;

    bool is_mount_point;
    vnode_t* mounted_on;

    bool directory_cached;
    bool should_cache_directory;

    // --- Locks --- //
    int lock_owner;
    int read_lock;
    int write_lock;

    // --- Driver Specific Data --- //
    vnode_ops_t file_operations;

    int inode;
    uint64_t fs_identifier; // An identifier (could be anything)
    uint64_t file_identifier; // An identifier for the file

    // --- List --- //
    vnode_t* children; /* If its a directory */
    vnode_t* next;
    vnode_t* parent;
    vnode_t* real_node; // If this is a copy, it points to the real node. Otherwise 0

    // --- Methods --- //
    // @brief Constructor
    vnode_t();
    vnode_t(vnode_type_t type);

    // @brief Deconstructor
    ~vnode_t();

    // @brief Reads the path
    char* read_link();
    
    // @brief Reads length bytes from offset offset
    int read(uint64_t offset, uint64_t length, void* buffer);

    // @brief Writes length bytes from offset offset
    int write(uint64_t offset, uint64_t length, const void* buffer);

    // @brief Writes length bytes from offset offset
    int truncate(uint64_t size);

    // @brief It will check the cache (children list) and if its not found it will call operations.find_file()
    int find_file(const char* filename, vnode_t** ret);

    // @warning For directories only
    // @brief Puts a linked list in ret and returns the amount of entries.
    int read_dir(vnode_t** ret);

    // @brief Create a directory
    // @warning For directories only
    int mkdir(const char* name);

    // @brief Create a file
    // @warning For directories only
    int creat(const char* name);

    // @brief Saves metadata
    void save_metadata();

    // @brief Removes a link or Deletes the file depending on the filesystem
    int unlink();

    int rmdir();

    int open();
    int close();

    int ioctl(int op, char* argp);
};