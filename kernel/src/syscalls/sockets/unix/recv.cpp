#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int64_t unix_socket_t::recv(void* buf, size_t size, uint64_t flags){
    task_t *self = task_scheduler::get_current_task();
    if (state != CONNECTED) return -ENOTCONN;

    void* temp = malloc(0x1000);
    if (temp == nullptr) return -ENOMEM;

    bool non_blocking = (flags & 04000) || (flags & 0x40) || (type & 04000);
    size_t i = 0;

    while (i < size){
        if (non_blocking && this->pollout() == false) {
            if (i == 0) {
                free(temp);
                return -EWOULDBLOCK;
            }

            break;
        }

        size_t remaining_to_read = size - i;
        size_t chunk_size = (remaining_to_read > 0x1000) ? 0x1000 : remaining_to_read;

        int64_t c = this->inbound->read(temp, chunk_size); 
        
        if (c <= 0) {
            if (c < 0 && i == 0) { free(temp); return c; }
            break;
        }

        if (!self->write_memory((char*)buf + i, temp, c)) {
            free(temp);
            return -EFAULT;
        }
        
        i += c;

        if ((size_t)c < chunk_size) {
            break;
        }
    }
    
    free(temp);
    return i;
}