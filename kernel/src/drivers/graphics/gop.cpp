/* Graphics Output Protocol (provided by the uefi firmware... its just a normal framebuffer) */
#include <drivers/graphics/gop.h>
#include <paging/PageFrameAllocator.h>
#include <filesystem/vfs/vfs.h>
#include <memory.h>
#include <cstr.h>
#include <math.h>
#include <kerrno.h>
#include <kstdio.h>


unsigned long mmap_fb_gop(void *addr, size_t length, int prot, int flags, int fd, uint64_t offset, vnode_t* this_node) {
    drivers::GOP *gop = (drivers::GOP*)this_node->file_identifier;

    task_t* self = task_scheduler::get_current_task();
    
    size_t page_count = (length + 0xFFF) / 0x1000;
    length = page_count * 0x1000;

    bool find_new_addr = (addr == nullptr);
    if (!find_new_addr && (flags & MAP_FIXED) == 0) {
        find_new_addr = true;
    }

    if (find_new_addr) {
        addr = (void*)(RMAP_DEFAULT_BASE + self->vm_tracker->rmap_offset);
        self->vm_tracker->rmap_offset += length;
    }

    self->vm_tracker->mark_allocation((uint64_t)addr, length, VM_FLAG_DONT_FREE | VM_FLAG_SHARED); // Don't free the vram... that would be bad

    uint64_t phys_base = virtual_to_physical((uint64_t)gop->framebuffer) + offset;

    for (size_t i = 0; i < length; i += 0x1000) {
        void* vaddr = (void*)((unsigned long)addr + i);
        uint64_t physical = phys_base + i;

        self->ptm->MapMemory(vaddr, (void*)physical);
        
        self->ptm->SetFlag(vaddr, PT_Flag::User, true);
        self->ptm->SetFlag(vaddr, PT_Flag::Write, true);
        self->ptm->SetFlag(vaddr, PT_Flag::CacheDisable, true); 
        self->ptm->SetFlag(vaddr, PT_Flag::WriteThrough, true);
    }

    return (unsigned long)addr;
}

int fb_gop_ioctl(int op, char* argp, vnode_t* this_node){
    task_t *self = task_scheduler::get_current_task();

    drivers::GOP *gop = (drivers::GOP*)this_node->file_identifier;
    
    if (op == FBIOGET_FSCREENINFO) {
        fb_fix_screeninfo fix;
        memset(&fix, 0, sizeof(fix));
        
        strncpy(fix.id, "dioOS FB", 15);
        fix.smem_start = virtual_to_physical((uint64_t)gop->framebuffer); // The physical address of the framebuffer
        fix.smem_len = gop->pitch * gop->height;
        fix.type = 0;   // FB_TYPE_PACKED_PIXELS
        fix.visual = 2; // FB_VISUAL_TRUECOLOR
        fix.line_length = gop->pitch;
        
        self->write_memory(argp, &fix, sizeof(fix));
        return 0;
    }

    if (op == FBIOGET_VSCREENINFO) {
        fb_var_screeninfo var;
        memset(&var, 0, sizeof(var));
        
        var.xres = gop->width;
        var.yres = gop->height;
        var.xres_virtual = gop->width;
        var.yres_virtual = gop->height;
        var.bits_per_pixel = gop->bits_per_pixel;
        
        // Define the BGRA/RGBA color offsets
        var.red.offset = 16;    var.red.length = 8;
        var.green.offset = 8;   var.green.length = 8;
        var.blue.offset = 0;    var.blue.length = 8;
        var.transp.offset = 24; var.transp.length = 8;
        
        // Copy to userspace
        self->write_memory(argp, &var, sizeof(var));
        return 0; // Success!
    }

    if (op == FBIOPUT_VSCREENINFO) {
        // The user application is trying to set the resolution. 
        // We can't dynamically change GOP resolution, 
        // so we just pretend we successfully applied it!
        // What could possibly go wrong? Have it render only on
        // part of the screen?
        return 0;
    }

    return -EINVAL;
}


namespace drivers{
    GOP::GOP(limine_framebuffer* fb){
        // Set the width
        this->width = fb->width;

        // Set the height
        this->height = fb->height;

        // Set the pitch
        this->pitch = fb->pitch;

        // Set the Bit Depth
        this->bits_per_pixel = fb->bpp;

        // Set the framebuffer address
        this->framebuffer = (uint32_t*)fb->address;

        // Allocate the backbuffer
        // TODO: Once i write a heap allocator, this should be allocated on the heap!
        size_t total_size = this->pitch * this->height;

        this->backbuffer = (uint32_t*)GlobalAllocator.RequestPages(DIV_ROUND_UP(total_size, 0x1000));
        memset(this->backbuffer, 0, total_size);
        memset(this->framebuffer, 0, total_size);

        char buffer[64];
        
        // Register the framebuffer to the filesystem
        int i = 0;
        while (1){
            stringf(buffer, sizeof(buffer), "/dev/fb%d", i);

            vnode_t *file = vfs::resolve_path(buffer);
            if (!file){
                file = vfs::create_path(buffer, VCHR);
                file->file_identifier = (uint64_t)this;
                file->file_operations.mmap = mmap_fb_gop;
                file->file_operations.ioctl = fb_gop_ioctl;
                file->permissions = 0777;
                stringf(file->name, sizeof(file->name), "fb%d", i);
                
                file->close();

                stringf(buffer, sizeof(buffer), "/sys/class/graphics/fb%d", i);
                vnode_t *sysfile = vfs::create_path(buffer, VREG);
                sysfile->type = VLNK;

                int r = stringf(buffer, sizeof(buffer), "/sys/devices/fb%d", i);
                sysfile->write(0, r, buffer);
                sysfile->close();

                stringf(buffer, sizeof(file->name), "/sys/devices/fb%d", i);
                vnode_t *sys_devices = vfs::create_path(buffer, VDIR);
                sys_devices->close();
                break;
            }
            
            file->close();
            i++;
        }
    }

    void GOP::PlotPixel(uint16_t x, uint16_t y, uint32_t color){
        /* Limit x / y */
        x = min(x, width - 1);
        y = min(y, height - 1);

        /* Calculate the address */
        uint32_t bytes_per_pixel = this->bits_per_pixel / 8;
        size_t byte_offset = y * this->pitch + x * bytes_per_pixel;
        uint32_t* pixel = (uint32_t*)((uint8_t*)framebuffer + byte_offset);
        uint32_t* bpixel = (uint32_t*)((uint8_t*)backbuffer + byte_offset);

        /* Set the pixel */
        *bpixel = color;
    }

    uint32_t GOP::GetPixel(uint16_t x, uint16_t y){
        /* Limit x / y */
        x = min(x, width - 1);
        y = min(y, height - 1);

        /* Calculate the address */
        uint32_t bytes_per_pixel = this->bits_per_pixel / 8;
        size_t byte_offset = y * this->pitch + x * bytes_per_pixel;
        uint32_t* pixel = (uint32_t*)((uint8_t*)backbuffer + byte_offset);

        /* Read the value */
        return *pixel;
    }
    void GOP::DrawRectangle(uint16_t x_start, uint16_t y_start, uint16_t w, uint16_t h, uint32_t color){
        /* If start outside screen, nothing to do */
        if (x_start >= this->width || y_start >= this->height) return;

        /* Clip width/height */
        uint32_t rect_w = (uint32_t)w;
        uint32_t rect_h = (uint32_t)h;
        if (rect_w == 0 || rect_h == 0) return;
        if (rect_w > this->width - x_start) rect_w = this->width - x_start;
        if (rect_h > this->height - y_start) rect_h = this->height - y_start;

        uint32_t bytes_per_pixel = this->bits_per_pixel / 8;

        uint32_t y_end = y_start + rect_h;
        for (uint32_t y = y_start; y < y_end; y++){
            size_t row_offset = y * this->pitch + x_start * bytes_per_pixel;
            uint32_t* start_px = (uint32_t*)((uint8_t*)framebuffer + row_offset);
            uint32_t* bf_start_px = (uint32_t*)((uint8_t*)backbuffer + row_offset);

            for (uint32_t x = 0; x < rect_w; x++){
                start_px[x] = color;
                bf_start_px[x] = color;
            }
        }
    }

    void GOP::ClearScreen(uint32_t color){
        /* Calculate the length in pixels */
        uint32_t bytes_per_pixel = this->bits_per_pixel / 8;
        uint32_t pitch_pixels = this->pitch / bytes_per_pixel;
        size_t total_pixels = pitch_pixels * this->height;

        uint32_t* fb = this->framebuffer;
        uint32_t* bb = this->backbuffer;

        for (size_t i = 0; i < total_pixels; i++){
            fb[i] = color;
            bb[i] = color;
        }
    }

    void GOP::Scroll(int y_pixels){
        // Cast to uint8_t* to perform byte-accurate math
        uint8_t* byte_buffer = (uint8_t*)backbuffer;
        
        // Calculate offsets in bytes
        size_t row_bytes = pitch;
        size_t total_height_bytes = height * pitch;
        size_t scroll_bytes = y_pixels * pitch;
        size_t remaining_bytes = total_height_bytes - scroll_bytes;

        // Move the memory up
        memmove(byte_buffer, byte_buffer + scroll_bytes, remaining_bytes);

        // Clear the bottom area
        memset(byte_buffer + remaining_bytes, 0, scroll_bytes);
    }

    void GOP::Update(){
        size_t total_size = this->height * this->pitch;
        memcpy(this->framebuffer, this->backbuffer, total_size);
    }

    void GOP::Update(int x, int y, int w, int h){
        // Lets not copy outside our vram space
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > width) w = width - x;
        if (y + h > height) h = height - y;
        
        if (w <= 0 || h <= 0) return;

        // Calculate Offsets
        uint32_t bpp_bytes = this->bits_per_pixel / 8;
        size_t copy_width_bytes = w * bpp_bytes;
        size_t start_offset = (y * this->pitch) + (x * bpp_bytes);

        // Get byte-pointers to the start of the dirty region
        uint8_t* dest_ptr = (uint8_t*)this->framebuffer + start_offset;
        uint8_t* src_ptr  = (uint8_t*)this->backbuffer + start_offset;

        // If the width of our update matches the screen pitch, the memory is mathematically contiguous. We can do one massive copy.
        if (copy_width_bytes == this->pitch) {
            memcpy(dest_ptr, src_ptr, h * this->pitch);
        } 
        else {
            // We are updating a box in the middle of the screen. We must skip the bytes between the right side
            // of the box and the left side of the next row.
            for (int i = 0; i < h; i++) {
                memcpy(dest_ptr, src_ptr, copy_width_bytes);
                dest_ptr += this->pitch;
                src_ptr += this->pitch;
            }
        }
    }
}