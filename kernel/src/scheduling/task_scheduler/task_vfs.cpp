#include <scheduling/task_scheduler/task_scheduler.h>
#include <filesystem/vfs/vfs.h>
#include <cstr.h>

// Read /proc/x/stat
int read_task_stat(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    if (offset != 0) return 0;

    task_t *task = (task_t*)this_node->fs_identifier;

    char state;
    switch (task->task_state){
        case PAUSED:
            state = 'I';
            break;
        case RUNNING:
            state = 'R';
            break;
        case BLOCKED:
            state = 'S';
            break;
        case ZOMBIE:
            state = 'Z';
            break;
        case STOPPED:
            state = 'T';
            break;
        default:
            break;
    }

    /* https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html */
    int l = stringf((char*)buffer, length, "%d (%s) %c %d %d %d 0 0 %d 0 0 0 0 %d %d %d %d %d %d %d %d %d %d %d", 
        task->pid, task->executable_name, state, task->ppid, task->pgid, task->sid, 0 /* Process Flags */,
        task->cpu_time, 0 /* Time in kernel mode */, 0 /* cstime? */, 0 /* priority */, 0 /* nice */,
        0 /* num_threads */, 0, task->start_time, task->vm_tracker.total_marked_memory, task->vm_tracker.total_marked_memory / 0x1000,
        UINT64_MAX);

    return l;
}


int task_vfs_unlink(vnode_t *this_node){
    delete this_node;

    return 0;
}

void _add_dynamic_task_virtual_files(task_t *task){
    vnode_t *stat = new vnode_t(VREG);
    strcpy(stat->name, "stat");

    stat->file_operations.unlink = task_vfs_unlink;
    stat->file_operations.read = read_task_stat;
    stat->fs_identifier = (uint64_t)task;

    vfs::add_node(task->proc_vfs_dir, stat);
}



/* /proc/stat (CPU Information) */
#include <local.h>
#include <drivers/timers/common.h>
#include <paging/PageFrameAllocator.h>

int _read_proc_stat(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    // Well, i shouldn't be writing to the buffer directly but it currently works...
    if (offset != 0) return 0;

    int64_t rem_length = length;
    uint64_t off = 0;

    uint64_t total_usr = 0, total_sys = 0, total_idle = 0;
    for (cpu_local_data *d = bsp_local; d != nullptr; d = d->next) {
        total_usr += d->time_in_userspace;
        total_sys += d->time_in_kernel;
        total_idle += d->idle_time;
    }

    // 2. Aggregate line (MUST start with 'cpu ' and no index)
    off += stringf((char *)buffer + off, length - off, 
                   "cpu  %lu 0 %lu %lu 0 0 0 0 0 0\n", 
                   total_usr, total_sys, total_idle);

    for (cpu_local_data *data = bsp_local; data != nullptr; data = data->next){
        if (rem_length <= 0) break;

        int r = stringf((char*)buffer + off, rem_length, "cpu%d %d %d %d %d 0 0 0 0 0 0\n",
                        data->cpu_id, data->time_in_userspace, 0 /* NICE */,
                        data->time_in_kernel /* System */, data->idle_time);

        off += r;
        rem_length -= r;
    }

    if (rem_length <= 0) return off;

    int r = stringf((char*)buffer + off, rem_length, "btime %d", boot_time);

    off += r;
    rem_length -= r;

    return off;
}


int _read_proc_meminfo(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    if (offset != 0) return 0;

    int r = stringf((char*)buffer, length, "MemTotal:\t%d kB\nMemFree:\t%d kB\nMemAvailable:\t%d kB",
                    GlobalAllocator.total_memory / 1024, GlobalAllocator.free_memory / 1024, GlobalAllocator.free_memory / 1024);
    
    return r;
}

void _init_cpu_proc_fs(){
    vnode_t *proc = vfs::resolve_path("/proc");
    if (!proc) return;

    vnode_t *stat = new vnode_t(VREG);
    strcpy(stat->name, "stat");

    stat->file_operations.unlink = task_vfs_unlink;
    stat->file_operations.read = _read_proc_stat;

    vfs::add_node(proc, stat);

    vnode_t *meminfo = new vnode_t(VREG);
    strcpy(meminfo->name, "meminfo");

    meminfo->file_operations.unlink = task_vfs_unlink;
    meminfo->file_operations.read = _read_proc_meminfo;

    vfs::add_node(proc, meminfo);
}