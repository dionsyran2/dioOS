#pragma once
#include <vfs/vfs.h>

enum procfs_node_type {
    PROCFS_ROOT,       // The /proc folder itself
    PROCFS_MEMINFO,    // /proc/meminfo
    PROCFS_PID_DIR,    // /proc/[pid]
    PROCFS_PID_STAT,   // /proc/[pid]/stat
    PROCFS_PID_CMDLINE,// /proc/[pid]/cmdline
    PROCFS_FD_DIR,     // /proc/[pid]/fd
    PROCFS_FD_LINK     // /proc/[pid]/fd/[num]
};

namespace procfs {
    dentry_t *get_root();
}