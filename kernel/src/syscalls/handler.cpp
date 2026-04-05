#include <syscalls/syscalls.h>
#include <syscalls/syscall_to_name.h>
#include <memory/heap.h>
#include <cpu.h>

#define LOG_SYSCALLS

syscall_function find_syscall(int id){
    for (syscall_definition_t* def = __start_syscalls; 
        def < __stop_syscalls; 
        def++) 
    {   
        if (def->number == id) return def->entry;
    }
    
    return nullptr;
}


extern "C" uint64_t handle_syscall(__registers_t* registers){
    task_t* self = task_scheduler::get_current_task();
    save_fpu_state(self->saved_fpu_state); // Imagine chasing memory corruption, all because I never saved the fpu state
    self->executing_syscall = true;

    self->syscall_registers = registers;
    self->userspace_return_address = registers->rip;

    if (self->signal_count == 0 && self->woke_by_signal){
        self->woke_by_signal = false;
    }

    syscall_function entry = find_syscall(registers->rax);
    if (entry == nullptr){
        serialf("\e[0;31m[SYSCALL] (%d) #%d(%d, %d, %d, %d, %d, %d)\e[0m\n", self->pid, registers->rax, registers->rdi, 
            registers->rsi, registers->rdx, registers->r10, registers->r8, registers->r9);
        
        registers->rax = -ENOSYS;

        asm ("cli");
        self->executing_syscall = false;
        return -ENOSYS;
    }

    serialf("%d / %d\n", self->pid, registers->rax);
    unsigned long ret = entry(registers->rdi, registers->rsi, registers->rdx,
        registers->r10, registers->r8, registers->r9);

    
    // LOG THE SYSCALL
    #ifdef LOG_SYSCALLS
    uint64_t args[6] = {registers->rdi, registers->rsi, registers->rdx,
        registers->r10, registers->r8, registers->r9}; 

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    stringf(buffer, sizeof(buffer), "%d | %s(", self->pid, syscall_to_name(registers->rax));
    int arg_type[6];
    memset(arg_type, 0, 6 * sizeof(int));
    syscall_args(registers->rax, arg_type);

    for (int i = 0; i < 6; i++){
        if (arg_type[i] == 0) break;
        switch (arg_type[i]){
            case 1:
                stringf(buffer, sizeof(buffer), "%s%p", buffer, args[i]);
                break;
            case 2:{
                char *string = self->read_string((char*)args[i], 512);

                stringf(buffer, sizeof(buffer), "%s'%s'", buffer, string);

                free(string);
                break;
            }
            case 3:
                stringf(buffer, sizeof(buffer), "%s%d", buffer, args[i]);
                break;
        }

        if (i != 6 && arg_type[i + 1]) stringf(buffer, sizeof(buffer), "%s, ", buffer);
    }

    stringf(buffer, sizeof(buffer), "%s) = %ld %s\n\r", buffer, ret, ((int) ret) >= 0 ? "" : ERRNO_NAME(- ((int)ret)));
    serialf(buffer);
    #endif

    registers->rax = ret;
    restore_fpu_state(self->saved_fpu_state);

    asm ("cli");

    // Hijack the return process to return to the signal handler! (If a signal is pending ofcourse)
    if (self->pending_signals & ~self->signal_mask) {
        registers->rip = registers->rcx;
        registers->rflags = registers->r11;

        memcpy(&self->registers, registers, sizeof(__registers_t));
        
        task_scheduler::_handle_signals(self);
        
        self->registers.r11 = self->registers.rflags;
        self->registers.rcx = self->registers.rip;
        memcpy(registers, &self->registers, sizeof(__registers_t));
    }



    self->executing_syscall = false;
    return ret;
}




extern "C" void syscall_entry();
void setup_syscalls(){
    uint64_t efer = read_msr(IA32_EFER);
    write_msr(IA32_EFER, efer | 1);

    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)(0x10) << 48);
    write_msr(IA32_STAR, star);

    write_msr(IA32_SFMASK, 0x200);
}

