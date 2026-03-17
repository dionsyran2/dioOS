#include <syscalls/syscalls.h>

#define ARCH_SET_FS     0x1002
#define ARCH_GET_FS     0x1003
#define ARCH_SET_GS     0x1001
#define ARCH_GET_GS     0x1004

long sys_arch_prctl(int op, unsigned long addr){
    unsigned long *write = (unsigned long *)addr;
    task_t* self = task_scheduler::get_current_task();
    
    switch (op){
        case ARCH_SET_FS:{
            self->fs_pointer = addr;
            write_msr(IA32_FS_BASE, addr);
            break;
        }
        case ARCH_GET_FS:{
            uint64_t src = read_msr(IA32_FS_BASE);
            self->write_memory(write, &src, sizeof(uint64_t));
            break;
        }
        default:{
            return -EINVAL;
        }
    }
    return 0;
}

REGISTER_SYSCALL(SYS_arch_prctl, sys_arch_prctl);