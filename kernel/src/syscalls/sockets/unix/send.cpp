#include <syscalls/syscalls.h>
#include <memory.h>
#include <memory/heap.h>
#include <syscalls/files/files.h>
#include <syscalls/sockets.h>
#include <helpers/sockets.h>
#include <helpers/pipes.h>

int64_t unix_socket_t::send(const void* buf, size_t size, uint64_t flags){
    task_t *self = task_scheduler::get_current_task();

    if (state == CONNECTED){
        void* buffer = malloc(4096);
        if (!buffer) return -ENOMEM;

        size_t total_written = 0;
        size_t remaining = size;
        const char* user_ptr = (const char*)buf;

        while (remaining > 0) {
            size_t chunk_size = (remaining > 4096) ? 4096 : remaining;

            if (!self->read_memory((void*)(user_ptr + total_written), buffer, chunk_size)) {
                free(buffer);
                return -EFAULT;
            }

            int64_t written = outbound->write(buffer, chunk_size);

            if (written < 0) {
                if (total_written == 0) {
                    free(buffer);
                    return (int)written;
                }
                break; 
            }

            total_written += written;

            if ((size_t)written < chunk_size) break;
            
            remaining -= written;
        }

        free(buffer);

        return total_written;
    } else {
        return -ENOTCONN;
    }
}

