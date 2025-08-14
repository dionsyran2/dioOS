#pragma once
#include <stdint.h>
#include <stddef.h>
#include <limine.h>

class display {
public:
    virtual void set_pixel(uint32_t x, uint32_t y, uint32_t color) = 0;
    virtual uint32_t get_pixel(uint32_t x, uint32_t y) = 0;
    virtual void get_screen_size(uint32_t* width, uint32_t* height, uint32_t* pitch) = 0;
    virtual void update_screen() = 0;
    virtual void update_screen(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) = 0;
    virtual void scroll(uint32_t pixels) = 0;
    virtual void disable_backbuffer() = 0;
    virtual void enable_backbuffer() = 0;
    virtual ~display() = default;
};

class framebuffer : public display {
public:
    framebuffer(limine_framebuffer* fb);

    void set_pixel(uint32_t x, uint32_t y, uint32_t color) override;
    uint32_t get_pixel(uint32_t x, uint32_t y) override;
    void get_screen_size(uint32_t* width, uint32_t* height, uint32_t* pitch) override;
    void update_screen() override;
    void update_screen(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) override;
    void scroll(uint32_t pixels) override;
    void disable_backbuffer() override;
    void enable_backbuffer() override;

    bool backbuffer_disable; // writes will still be written to the backbuffer, this is for optimizations
    bool is_dirty;
    uint32_t dirty_x;
    uint32_t dirty_y;
    uint32_t dirty_w;
    uint32_t dirty_h;
private:
    limine_framebuffer* fb;
    uint8_t* back_buffer;
};