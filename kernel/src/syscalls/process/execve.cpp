#include <syscalls/syscalls.h>
#include <memory.h>
#include <elf/elf.h>

long sys_execve(const char *pathname, char *const argv[], char *const envp[]){
    task_t* self = task_scheduler::get_current_task();
    
    char *kpath = self->read_string(pathname);
    if (!kpath) return -EFAULT;

    serialf("%d | execve('%s', %p, %p)\n\r", self->pid, kpath, argv, envp);

    // Count argc
    int argc = 0;
    while(true){
        uintptr_t arg_addr = 0;
        self->read_from_userspace(&arg_addr, (char*)argv + (argc * sizeof(uintptr_t)), sizeof(uintptr_t));
        if (arg_addr == 0) break;
        argc++;
    }

    // Count envc
    int envc = 0;
    while(true){
        uintptr_t env_addr = 0;
        self->read_from_userspace(&env_addr, (char*)envp + (envc * sizeof(uintptr_t)), sizeof(uintptr_t));
        if (env_addr == 0) break;
        envc++;
    }

    // Safely heap-allocate arrays (avoids kernel stack overflow!)
    char **kargv = new char*[argc + 1];
    char **kenvp = new char*[envc + 1];
    kargv[argc] = nullptr;
    kenvp[envc] = nullptr;

    for (int i = 0; i < argc; i++){
        uintptr_t arg_addr = 0;
        self->read_from_userspace(&arg_addr, (char*)argv + (i * sizeof(uintptr_t)), sizeof(uintptr_t));
        kargv[i] = self->read_string((char*)arg_addr);
    }

    for (int i = 0; i < envc; i++){
        uintptr_t env_addr = 0;
        self->read_from_userspace(&env_addr, (char*)envp + (i * sizeof(uintptr_t)), sizeof(uintptr_t));
        kenvp[i] = self->read_string((char*)env_addr);
    }

    int result = kexecve(kpath, argc, kargv, (const char**)kenvp);

    // Clean up heap allocations on failure (on success, this code is never reached)
    for (int i = 0; i < argc; i++) free(kargv[i]);
    for (int i = 0; i < envc; i++) free(kenvp[i]);
    delete[] kargv;
    delete[] kenvp;
    free(kpath);

    return result;
}
REGISTER_SYSCALL(SYS_execve, sys_execve);