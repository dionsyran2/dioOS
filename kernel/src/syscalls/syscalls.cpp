#include <syscalls/syscalls.h>
#include <cpu.h>
#include <BasicRenderer.h>
#include <kerrno.h>
#include <scheduling/task/scheduler.h>
uint64_t next_syscall_stack = 0;
uint64_t user_syscall_stack = 0;

long sys_arch_prctl(int op, unsigned long addr){
    unsigned long *write = (unsigned long *)addr; // in case its a read operation 
    switch (op){
        case ARCH_SET_FS:
            write_msr(IA32_FS_BASE, addr);
            break;
        case ARCH_GET_FS:
            *write = read_msr(IA32_FS_BASE);
            break;
        case ARCH_SET_GS:
            write_msr(IA32_GS_BASE, addr);
            break;
        case ARCH_GET_GS:
            *write = read_msr(IA32_GS_BASE);
            break;
        default:
            return EINVAL;
    }
    return 0;
}

long sys_set_tid_addr(unsigned long addr){
    taskScheduler::currentTask->tid_address = (unsigned long*)addr;
    return taskScheduler::currentTask->pid;
}

unsigned long sys_brk(unsigned long brk){
    taskScheduler::task_t* task = taskScheduler::currentTask;
    if (brk == 0) return task->brk_end;
    if (brk == task->brk_end) return task->brk_end;
    if (brk < task->brk_end) return task->brk_end; // maybe free the region instead of doing that?

    size_t size = brk - task->brk_end;
    size_t pages = size / 0x1000;
    if (size & 0xFFF) pages++;
    serialPrint(COM1, "BRK ");
    serialPrint(COM1, toString(pages));
    serialPrint(COM1, "\n\r");

    for (int i = 0; i < pages; i++){
        void* page = GlobalAllocator.RequestPage();
        if (page == nullptr) return 0; // run out of memory

        task->ptm->UnmapMemory(page);
        task->ptm->MapMemory((void*)task->brk_end, page);
        task->brk_end += 0x1000;
    }

    return brk;
}

unsigned long sys_mmap(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset){
    PageTableManager* ptm = taskScheduler::currentTask->ptm;
    bool mapped = ptm->isMapped(addr);
    
    if (mapped && (flags & MAP_FIXED) == 0) return (unsigned long)addr;
    
    length += (size_t)addr & 0xFFF;
    addr = (void*)((uint64_t)addr & ~0xFFF);
    for (int i = 0; i < length; i += 0x1000){
        void* page = GlobalAllocator.RequestPage();
        void* vaddr = (void*)((unsigned long)addr + i);
        ptm->MapMemory(vaddr, page);
        ptm->SetPageFlag(vaddr, PT_Flag::UserSuper, true);
    }
    return (unsigned long)addr;
}

int sys_mprotect(void* addr, size_t len, int prot){
    addr = (void*)((unsigned long)addr & ~(0x1000 - 1));
    len = (((unsigned long)addr + len + 0x1000 - 1) & ~(0x1000 - 1)) - (unsigned long)addr;
    PageTableManager* ptm = taskScheduler::currentTask->ptm;
    if (((unsigned long)addr & 0xFFF)) {
        serialPrint(COM1, "UNALIGNED\n\r");
        serialPrint(COM1, toHString((uint64_t)addr));

        return -1;
    };

    for (int i = 0; i < len; i += 0x1000){
        void* caddr = (void*)((unsigned long)addr + i);
        if (!ptm->isMapped(caddr)) {
            serialPrint(COM1, "UNMAPPED");
            return -1;
        }
        if (prot & PROT_READ){
            ptm->SetPageFlag(caddr, PT_Flag::ReadWrite, false);
        }

        if (prot & PROT_WRITE){
            ptm->SetPageFlag(caddr, PT_Flag::ReadWrite, true);
        }

        if ((prot & PROT_EXEC) == 0){
            ptm->SetPageFlag(caddr, PT_Flag::NX, true);
        }else{
            ptm->SetPageFlag(caddr, PT_Flag::NX, false);
        }
    }

    return 0;
}
int int_write(int fd, void* data, int len){
    /*
    0: stdin
    1: stdout
    2: stderr
    */
    if (fd == 1 || fd == 2){ 
        vnode_t* tty = vfs::resolve_path("/dev/tty1");
        if (tty == nullptr) return -EINVAL;
        tty->ops.write((const char*)data, len, tty);
    }
    return len;
}

int sys_writev(int fd, const struct iovec *iov, int iovcnt){
    int total_len = 0;
    for (int i = 0; i < iovcnt; i++){
        total_len += int_write(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return total_len;
}

int sys_ioctl(int fd, int op, char* argp){
    if (fd == 1 || fd == 2){
        if (op == TIOCGWINSZ){
            vnode_t* tty = vfs::resolve_path("/dev/tty1");
            if (tty == nullptr) return -EINVAL;

            uint32_t rows = (uint32_t)tty->ops.write((const char*)1, 0, tty);
            uint32_t cols = (uint32_t)tty->ops.write((const char*)2, 0, tty);
            winsize* w = (winsize*)argp;
            w->ws_col = cols;
            w->ws_row = rows;
            w->ws_xpixel = cols * 8;
            w->ws_ypixel = rows * 16;
            return 0; 
        }
    }
    return -EBADF;
}

void sys_exit_group(int status){
    // should exit all threads in a process
    taskScheduler::currentTask->exit = true;
    __asm__ ("sti");
    while(1) __asm__("hlt");
}


int64_t sys_read(int fd, char* buf, size_t count){
    size_t cnt = 0;
    if (fd == 0){ // stdin
        vnode_t* tty = vfs::resolve_path("/dev/tty1");
        if (tty == nullptr) return -EINVAL;

        asm ("sti");
        void* data = tty->ops.load(&cnt, tty);
        asm ("cli");

        if (cnt > count) cnt = count;
        memcpy(buf, data, cnt);
        free(data);
        return cnt;
    }
    return -EINVAL;
}

int64_t sys_lseek(int fd, uint64_t offset, int whence){
    return -ESPIPE; // we dont support seek yet :)
}

int64_t sys_clock_gettime(int clk_id, struct timespc* user_tp) {
    if (clk_id == NULL){
        user_tp->tv_sec = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
    }
    return 0;
}

extern "C" uint64_t syscall_entry_cpp(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t syscall_num
){

    switch (syscall_num){
        case SYSCALL_GETTIME:
            return sys_clock_gettime(arg0, (timespc*)arg1);
        case SYSCALL_LSEEK:
            return sys_lseek(arg0, arg1, arg2);
        case SYSCALL_READ:
            return sys_read(arg0, (char*)arg1, arg2);
        case SYSCALL_ARCH_PRCTL:
            return sys_arch_prctl(arg0, arg1);
        case SYSCALL_SET_TID_ADDR:
            return sys_set_tid_addr(arg0); //should save an address etc...
        case SYSCALL_BRK:
            return sys_brk(arg0);
        case SYSCALL_MMAP:
            return sys_mmap((void*)arg0, arg1, arg2, arg3, arg4, arg5);
        case SYSCALL_MPROTECT:
            return sys_mprotect((void*)arg0, arg1, arg2);
        case SYSCALL_WRITEV:
            return sys_writev(arg0, (const iovec*)arg1, arg2);
        case SYSCALL_IOCTL:
            return sys_ioctl(arg0, arg1, (char*)arg2);
        case SYSCALL_EXIT_GROUP:
            sys_exit_group(arg0);
            return 0;
        default:
            globalRenderer->Set(true);
            kprintf("SYSCALL %d %llx %llx %llx %llx %llx %llx\n", syscall_num, arg0, arg1, arg2, arg3, arg4, arg5);
            while(1);
            break;
    }
    
    return -1;
}


void setup_syscalls(){
    uint64_t efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | 1);

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)(0x10) << 48);
    write_msr(IA32_STAR, star);

    write_msr(IA32_SFMASK, 0x200);
}

