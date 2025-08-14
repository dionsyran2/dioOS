// Welcome to the worst procfs implementation you have ever seen :)
#include <devices/proc.h>
#include <VFS/vfs.h>
#include <memory.h>
#include <memory/heap.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/task/scheduler.h>
#include <kerrno.h>

const char* asound_version = "Advanced Linux Sound Architecture Driver Version k6.8.0-64-generic\n"; 

int64_t meminfo_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
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
    if (offset >= sz) return EOF;

    this_node->size = sz;
    size_t to_read = sz - offset;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, tmp + offset, to_read);
    GlobalAllocator.FreePage(tmp);
    
    return to_read;
}

int64_t stat_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    char* tmp = (char*)GlobalAllocator.RequestPage();
    strcat(tmp, "cpu  14024 2 29534 12233602 3370 0 4057 0 0 0\n");
    strcat(tmp, "cpu0 855 0 1819 764283 278 0 3235 0 0 0\n");
    strcat(tmp, "processes ");
    strcat(tmp, toString((uint64_t)task_scheduler::num_of_tasks));
    strcat(tmp, "\n");
    strcat(tmp, "btime 1753908371\n");



    size_t sz = strlen(tmp);
    if (offset >= sz) return EOF;
    size_t to_read = sz - offset;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, tmp + offset, to_read);
    GlobalAllocator.FreePage(tmp);
    
    return to_read;
}

int64_t uptime_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    char* tmp = (char*)GlobalAllocator.RequestPage();
    strcat(tmp, toString(uptime));
    strcat(tmp, " 0");



    size_t sz = strlen(tmp);
    if (offset >= sz) return EOF;
    size_t to_read = sz - offset;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, tmp + offset, to_read);
    GlobalAllocator.FreePage(tmp);
    
    return to_read;
}


bool init_meminfo(vnode_t* proc){

    vnode_t* meminfo = vfs::mount_node("meminfo", VREG, proc);
    meminfo->flags = VFS_NO_CACHE | VFS_RO;
    meminfo->ops.read = meminfo_read;
    return true;
}

bool init_stat(vnode_t* proc){
    vnode_t* stat = vfs::mount_node("stat", VREG, proc);
    stat->flags = VFS_NO_CACHE | VFS_RO;
    stat->ops.read = stat_read;
    return true;
}

bool init_uptime(vnode_t* proc){
    vnode_t* stat = vfs::mount_node("uptime", VREG, proc);
    stat->flags = VFS_NO_CACHE | VFS_RO;
    stat->ops.read = uptime_read;
    return true;
}

// asound

int64_t asound_version_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    size_t sz = strlen(asound_version);

    this_node->size = sz;
    size_t to_read = sz - offset;
    if (offset >= sz) return EOF;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, (char*)asound_version + offset, to_read);
    return to_read;
}

int64_t asound_pcm_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    char* t = "00-00: ES1371/1 : ES1371 DAC : playback 1";
    size_t sz = strlen(t);

    this_node->size = sz;
    size_t to_read = sz - offset;
    if (offset >= sz) return EOF;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, t + offset, to_read);
    return to_read;
}

int64_t asound_cards_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    char* t = "0 [AudioPCI]: ENS1371 - Ensoniq AudioPCI";
    size_t sz = strlen(t);
    this_node->size = sz;

    size_t to_read = sz - offset;
    if (offset >= sz) return EOF;
    if (to_read > cnt) to_read = cnt;

    memcpy(buffer, t + offset, to_read);
    return to_read;
}


int64_t asound_devices_read(void* buffer, size_t cnt, size_t offset, vnode_t* this_node){
    char* t = "1: [0- 0]: digital audio playback";
    size_t sz = strlen(t);
    
    this_node->size = sz;
    size_t to_read = sz - offset;
    if (offset >= sz) return EOF;
    if (to_read > cnt) to_read = cnt;

    memcpy(t + offset, buffer, to_read);
    return to_read;
}

bool init_asound_ver(vnode_t* asound){
    vnode_t* version = vfs::mount_node("version", VREG, asound);
    version->flags = VFS_NO_CACHE | VFS_RO;
    version->ops.read = asound_version_read;
    return true;
}


bool init_asound_pcm(vnode_t* asound){
    vnode_t* version = vfs::mount_node("pcm", VREG, asound);
    version->flags = VFS_NO_CACHE | VFS_RO;
    version->ops.read = asound_pcm_read;
    return true;
}

bool init_asound_cards(vnode_t* asound){
    vnode_t* version = vfs::mount_node("cards", VREG, asound);
    version->flags = VFS_NO_CACHE | VFS_RO;
    version->ops.read = asound_cards_read;
    return true;
}

bool init_asound(vnode_t* proc){
    vnode_t* asound = vfs::mount_node("asound", VDIR, proc);
    if (!init_asound_ver(asound)) return false;
    if (!init_asound_pcm(asound)) return false;
    if (!init_asound_cards(asound)) return false;

    return true;
}


bool init_proc_fs(){
    vnode_t* proc = vfs::mount_node("proc", VDIR, vfs::get_root_node());
    if (proc == nullptr) return false;
    if (!init_meminfo(proc)) return false;
    if (!init_stat(proc)) return false;
    if (!init_uptime(proc)) return false;
    if (!init_asound(proc)) return false;
}
