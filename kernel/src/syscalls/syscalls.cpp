#include <syscalls/syscalls.h>
#include <cpu.h>
#include <BasicRenderer.h>
#include <kerrno.h>
#include <kernel.h>
#include <scheduling/task/scheduler.h>
#include <uname.h>
#include <syscalls/syscall_to_name.h>

syscall_handler_t syscall_table[MAX_SYSCALLS] = {0};


uint64_t next_syscall_stack = 0;
uint64_t user_syscall_stack = 0;

int register_syscall(int num, syscall_handler_t handler) {
    if (num < 0 || num >= MAX_SYSCALLS) return -1; // Invalid syscall number
    if (syscall_table[num] != NULL) return -2;     // Already registered
    syscall_table[num] = handler;
    return 0;
}

extern "C" uint64_t syscall_entry_cpp(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t num) {
    task_t* ctask = task_scheduler::get_current_task();

    if (num >= MAX_SYSCALLS || syscall_table[num] == NULL) {
        #ifdef LOG_SYSCALLS
        globalRenderer->Set(true);
        serialf("\e[0;31mSYSCALL %d %llx %llx %llx %llx %llx %llx\e[0m\n", num, a1, a2, a3, a4, a5, a6);
        //while(1);
        #endif
        return -ENOSYS; // Syscall not implemented
    }

    // put the args in an array (makes it easier to log them)
    uint64_t args[6] = {a1, a2, a3, a4, a5, a6};
    
    // call the handler
    uint64_t ret = syscall_table[num](a1, a2, a3, a4, a5, a6);

    #ifdef LOG_SYSCALLS
    // log the call
    serialf("%s(", syscall_to_name(num));
    int arg_type[6] = { 0 };
    syscall_args(num, arg_type);

    for (int i = 0; i < 6; i++){
        if (arg_type[i] == 0) break;
        switch (arg_type[i]){
            case 1:
                serialf("%llx", args[i]);
                break;
            case 2:
                serialf("'%s'", args[i]);
                break;
            case 3:
                serialf("%d", args[i]);
                break;
        }

        if (i != 6 && arg_type[i + 1]) serialf(", ");
    }

    serialf(") = %ll %s\n\r", ret, ((int) ret) >= 0 ? "" : ERRNO_NAME(- ((int)ret)));
    #endif

    return ret;
}

int sys_uname(struct utsname *buf){
    memcpy(buf, get_uname(), sizeof(utsname));
    return 0;
}

int sys_clock_gettime(int clk_id, struct timespc* user_tp) {
    if (clk_id == 0){
        user_tp->tv_sec = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
        user_tp->tv_nsec = c_time->msec * 1000000;
    }
    return 0;
}

int sys_nanosleep(const struct timespc *duration, struct timespc* rem){
    // Placeholder, should block instead
    asm ("sti");
    Sleep((duration->tv_sec * 1000) + (duration->tv_nsec / 1000000));
    asm ("cli");
    return 0;
}

int sys_time(uint64_t* time){
    if (time == nullptr) return -EFAULT;
    *time = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
    return 0;
}

int sys_gettimeofday(struct timeval* tv, struct timezone* tz){
    if (tv != nullptr) tv->tv_sec = to_unix_timestamp(c_time->second, c_time->minute, c_time->hour, c_time->day, c_time->month, c_time->year);
    tv->tv_usec = c_time->msec * 1000;
    
    if (tz != nullptr) {
        tz->tz_dsttime = 0; // At some point I may add timezones
        tz->tz_minuteswest = 0;
    }
    return 0;
}



void register_syscall(){
    register_thread_syscalls();
    register_fs_syscalls();
    register_mem_syscalls();
    register_sig_syscalls();
    register_syscall(SYSCALL_UNAME, (syscall_handler_t)sys_uname);
    register_syscall(SYSCALL_GETTIME, (syscall_handler_t)sys_clock_gettime);
    register_syscall(SYSCALL_NANOSLEEP, (syscall_handler_t)sys_nanosleep);
    register_syscall(SYSCALL_TIME, (syscall_handler_t)sys_time);
    register_syscall(SYSCALL_GETTIMEOFDAY, (syscall_handler_t)sys_gettimeofday);


    int cnt = 0;

    for (int i = 0; i < MAX_SYSCALLS; i++){
        if (syscall_table[i] != NULL) cnt++;
    }

    kprintf(0x00FFF0, "[SYSCALLS] ");
    kprintf("registered %d syscalls\n", cnt);
}


void setup_syscalls(){
    uint64_t efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | 1);

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)(0x10) << 48);
    write_msr(IA32_STAR, star);

    write_msr(IA32_SFMASK, 0x200);

    register_syscall();
}

