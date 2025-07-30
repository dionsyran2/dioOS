/* /dev/null */
#include <devices/null.h>
#include <VFS/vfs.h>
#include <kerrno.h>

int null_write(const char* txt, size_t length, vnode* this_node){
    return length;
}

void* null_read(size_t* cnt, vnode* this_node){
    *cnt = 0;
    return nullptr;
}


int initialize_null_dev(){
    vnode_t* dev = vfs::resolve_path("/dev");
    if (dev == nullptr) dev = vfs::mount_node("dev", VDIR, vfs::get_root_node());
    if (vfs::resolve_path("/dev/null")) return -EEXIST;

    vnode_t* null = vfs::mount_node("null", VCHR, dev);
    null->ops.read = null_read;
    null->ops.write = null_write;
    null->is_tty = false;
    null->data_read = true;
    null->data_write = true;
    return 0;
}