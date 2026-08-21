#include <nullfs.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <sys/poll.h>

int null_read(void *context, void *buffer, size_t size, size_t offset){
    return 0;
}

int null_write(void *context, const void *buffer, size_t size, size_t offset){
    return size;
}

int null_poll(void *context, int events, poll_table_t *pt){
    return POLLIN | POLLOUT; 
}

devfs_ops_t nullops = {
    .read = null_read,
    .write = null_write,
    .poll = null_poll
};

void init_nullfs(){
    devfs::mknod("/null", S_IFCHR | 0666, &nullops, nullptr);    
}