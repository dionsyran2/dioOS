#include <syscalls/syscalls.h>
#include <memory/heap.h>


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
    self->is_executing_syscall = true;
    self->syscall_registers = registers;
    self->userspace_return_address = registers->rip;

    syscall_function entry = find_syscall(registers->rax);
    if (entry == nullptr){
        serialf("\e[0;31m[SYSCALL] (%d) #%d(%d, %d, %d, %d, %d, %d)\e[0m\n\r", self->pid, registers->rax, registers->rdi, 
            registers->rsi, registers->rdx, registers->r10, registers->r8, registers->r9);
        
        registers->rax = -ENOSYS;
        return -ENOSYS;
    }

    unsigned long ret = entry(registers->rdi, registers->rsi, registers->rdx,
        registers->r10, registers->r8, registers->r9);

    /************* TO BE REMOVED **************/
    uint64_t args[6] = {registers->rdi, registers->rsi, registers->rdx,
        registers->r10, registers->r8, registers->r9}; 

    serialf("%d | %s(", self->pid, syscall_to_name(registers->rax));
    int arg_type[6];
    memset(arg_type, 0, 6 * sizeof(int));
    syscall_args(registers->rax, arg_type);

    for (int i = 0; i < 6; i++){
        if (arg_type[i] == 0) break;
        switch (arg_type[i]){
            case 1:
                serialf("%p", args[i]);
                break;
            case 2:{
                char *string = self->read_string((char*)args[i]);
                serialf("'%s'", string ? string : "");


                if (string) {
                    free(string);
                }
                break;
            }
            case 3:
                serialf("%d", args[i]);
                break;
        }

        if (i != 6 && arg_type[i + 1]) serialf(", ");
    }

    serialf(") = %ld %s\n\r", ret, ((int) ret) >= 0 ? "" : ERRNO_NAME(- ((int)ret)));
    /********************************************/
    
    registers->rax = ret;

    asm ("cli");

    if (self->pending_signals & ~self->blocked_signals) {
        registers->rip = registers->rcx;
        registers->rflags = registers->r11;

        memcpy(&self->registers, registers, sizeof(__registers_t));

        if (!self->check_pending_signals()) return ret;

        self->registers.r11 = self->registers.rflags;
        self->registers.rcx = self->registers.rip;
        memcpy(registers, &self->registers, sizeof(__registers_t));
    }
    
    self->is_executing_syscall = false;
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
