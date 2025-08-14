/* /dev/null */
#include <devices/null.h>
#include <VFS/vfs.h>
#include <kerrno.h>

int64_t null_write(const void* buffer, size_t cnt, size_t offset, vnode* this_node){
    return cnt;
}

int64_t null_read(void* buffer, size_t cnt, size_t offset, vnode* this_node){
    return 0;
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