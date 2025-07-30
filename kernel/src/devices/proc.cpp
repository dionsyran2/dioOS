// Welcome to the worst procfs implementation you have ever seen :)
#include <devices/proc.h>
#include <VFS/vfs.h>
#include <memory.h>
#include <memory/heap.h>
#include <paging/PageFrameAllocator.h>

void* meminfo_read(size_t* cnt, vnode_t* this_node){
    char* tmp = (char*)GlobalAllocator.RequestPage();
    memset(tmp, 0, 0x1000);

    strcat(tmp, "MemTotal: ");
    strcat(tmp, (char*)toString(GlobalAllocator.GetMemSize() / 1024));
    strcat(tmp, " kB\n");

    strcat(tmp, "MemFree: ");
    strcat(tmp, (char*)toString((GlobalAllocator.GetFreeRAM() + heapFree) / 1024));
    strcat(tmp, " kB\n");

    strcat(tmp, "MemAvailable: ");
    strcat(tmp, (char*)toString((GlobalAllocator.GetFreeRAM() + heapFree) / 1024));
    strcat(tmp, " kB\n");

    size_t sz = strlen(tmp);
    char* ret = new char[sz + 1];
    strcpy(ret, tmp);

    this_node->size = sz;

    GlobalAllocator.FreePage(tmp);
    return ret;
}

bool init_meminfo(){
    vnode_t* proc = vfs::resolve_path("/proc");
    if (proc == nullptr) return false;

    vnode_t* meminfo = vfs::mount_node("meminfo", VREG, proc);
    meminfo->flags = VFS_NO_CACHE | VFS_RO;
    meminfo->ops.read = meminfo_read;
}

bool init_proc_fs(){
    vnode_t* proc = vfs::mount_node("proc", VDIR, vfs::get_root_node());
    if (!init_meminfo()) return false;
}
