#include <kernel.h>
#include <limine.h>
#include <local.h>

/* LIMINE */
/* Set the base revision */
__attribute__((used, section(".limine_requests")))
volatile LIMINE_BASE_REVISION(3);


extern "C" uint64_t local_krnl_stack_offset = offsetof(cpu_local_data, kernel_stack_top);
extern "C" uint64_t local_user_stack_scratch_offset = offsetof(cpu_local_data, user_stack_scratch);

/* KERNEL */
void main(){
    /* Initialize the Serial Port (for debugging purposes!) */
    InitSerial(COM1);

    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        serialf("\e[0;31mLimine base revision not supported!\e[0m\n");
        while(1) __asm__ __volatile__ ("hlt");
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        serialf("\e[0;31mNo framebuffer available\e[0m\n");
    }
    
    init_kernel();

    cpu_local_data* local = get_cpu_local_data();
    task_scheduler::initialize_scheduler();

    local->disable_scheduling = false;
    while(1);
}