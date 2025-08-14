#include <drivers/framebuffer/framebuffer.h>
#include <memory/heap.h>
#include <memory.h>

void framebuffer::set_pixel(uint32_t x, uint32_t y, uint32_t color){
    if (x >= fb->width || y >= fb->height) return;
    if (backbuffer_disable) {
        *(uint32_t*)(fb->address + (fb->pitch * y) + (x * (fb->bpp / 8))) = color; 
    }

    if (is_dirty == false) {
        dirty_x = dirty_w = x;
        dirty_y = dirty_h = y;
        is_dirty = true;
    }else{
        if (x < dirty_x) dirty_x = x;
        if (x > dirty_w) dirty_w = x;
        if (y < dirty_y) dirty_y = y;
        if (y > dirty_h) dirty_h = y;
    }

    *(uint32_t*)(back_buffer + (fb->pitch * y) + (x * (fb->bpp / 8))) = color; 
}

uint32_t framebuffer::get_pixel(uint32_t x, uint32_t y){
    if (x >= fb->width || y >= fb->height) return 0;

    return *(uint32_t*)(back_buffer + (fb->pitch * y) + (x * (fb->bpp / 8))); 
}

void framebuffer::get_screen_size(uint32_t* width, uint32_t* height, uint32_t* pitch){
    if (width != nullptr) *width = fb->width;
    if (height != nullptr) *height = fb->height;
    if (pitch != nullptr) *pitch = fb->pitch;
}

void framebuffer::update_screen(){
    if (backbuffer_disable) return;
    if (is_dirty == false) return;
    is_dirty = true;
    update_screen(dirty_x, dirty_y, dirty_w - dirty_x + 1, dirty_h - dirty_y + 1);
    //memcpy_simd(fb->address, (void*)back_buffer, fb->pitch * fb->height);
}

void framebuffer::update_screen(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (backbuffer_disable) return;
    uint32_t bpp = fb->bpp / 8;
    uint8_t* base = (uint8_t*)fb->address;

    for (uint32_t Y = y; Y < y + height; Y++) {
        void* dst = base + Y * fb->pitch + x * bpp;
        void* src = (uint8_t*)back_buffer + Y * fb->pitch + x * bpp;
        memcpy_simd(dst, src, width * bpp);
    }
}


void framebuffer::draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color){
    for (int Y = y; Y < (y + height); Y++){
        for (int X = x; X < (x + width); X++){
            set_pixel(X, Y, color);
        }
    }
}

void framebuffer::scroll(uint32_t pixels){
    uint16_t BytesPerPixel = fb->bpp / 8;
    memmove(back_buffer,
        (void*)((uintptr_t)back_buffer + (pixels * BytesPerPixel) * (fb->pitch / BytesPerPixel)),
        (fb->height) * fb->width * sizeof(uint32_t));
    if (backbuffer_disable) {
        backbuffer_disable = false;
        update_screen();
        backbuffer_disable = true;
    }
}

void framebuffer::disable_backbuffer(){
    backbuffer_disable = true;
}

void framebuffer::enable_backbuffer(){
    backbuffer_disable = false;
}


framebuffer::framebuffer(limine_framebuffer* fb){
    this->fb = fb;
    back_buffer = (uint8_t*)malloc(fb->pitch * fb->height);
}