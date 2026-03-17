#include <syscalls/syscalls.h>
#include <memory/heap.h>
#include <random.h>

long sys_getrandom(uint8_t* buf, size_t buflen, unsigned int flags){
    task_t* self = task_scheduler::get_current_task();

    if (buflen > (1 * 1024 * 1024)) return -ENOMEM;


    uint8_t* tmp = (uint8_t*)malloc(buflen);
    if (tmp == nullptr) return -ENOMEM;
    
    for (int i = 0; i < buflen; i++){
        tmp[i] = random() & 0xFF;
    }

    self->write_memory(buf, tmp, buflen);

    free(tmp);
    return buflen;
}

REGISTER_SYSCALL(SYS_getrandom, sys_getrandom);