#include <kernel.h>
#include <userspace/userspace.h>
__attribute__((used, section(".limine_requests")))
volatile LIMINE_BASE_REVISION(3);


void main(){
    InitSerial(COM1);
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        serialf("\e[0;31mLimine base revision not supported!\e[0m\n");
        while(1) __asm__ __volatile__ ("hlt");
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        serialf("\e[0;31mNo framebuffer available\e[0m\n");
        while(1);
    }

    init_kernel();
    void* new_stack = GlobalAllocator.RequestPages(DIV_ROUND_UP(1 * 1024 * 1024, 0x1000));
    asm ("mov %0, %%rsp" :: "r" (new_stack + (1 * 1024 * 1024)));

    flush_tss();
    setup_syscalls();
    

    task_scheduler::init();
    second_stage_init();

    task_scheduler::run_next_task(0, 0);
    while(1);
}