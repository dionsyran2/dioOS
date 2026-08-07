#include <kstdio.h>
#include <rendering/multiplexer.h>

void kprint(const char* str, size_t len = 0){
   virtual_terminal *vt = global_multiplexer->get_active();
   if (!vt) return;

   vt->write((char*)str, len, true);
}

#include <printf.h>

spinlock_t kprint_lock;
void kprintfva(const char* str, va_list args) {
    uint64_t rflags = spin_lock(&kprint_lock);

    const uint64_t buffer_size = 1024;
    char buffer[buffer_size];
    int written = vsnprintf_(buffer, buffer_size, str, args);
    kprint(buffer, written);

    spin_unlock(&kprint_lock, rflags);
}

void kprintf(const char* str, ...){
    va_list args;
    va_start(args, str);
    
    kprintfva(str, args);
    
    va_end(args);
}