/* Contains the Virtual Terminal Definitions! */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <drivers/graphics/common.h>
#include <drivers/ps2/keyboard.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <syscalls/files/termios.h>


#define VT_CELL_WIDTH 8
#define VT_CELL_HEIGHT 16

#define VT_INVERSE (1 << 0)
#define VT_BOLD (1 << 1)
#define VT_UNDERLINE (1 << 2)


enum vt_state_t {
    VT_STATE_NORMAL,
    VT_STATE_ESC,
    VT_STATE_CSI,
    VT_STATE_CSI_PARAM,
};

struct vt_cell{
    wchar_t chr;
    uint16_t attributes;

    uint32_t fg;
    uint32_t bg;
};

struct task_t;

struct virtual_terminal{
    vt_cell* cell_table;

    uint16_t width;
    uint16_t height;

    uint16_t current_attributes;

    uint32_t current_fg;
    uint32_t current_bg;
    
    uint16_t offset_x;
    uint16_t offset_y;

    uint16_t cursor_x;
    uint16_t cursor_y;

    bool cursor_state;
    bool insert_mode;
    
    uint16_t cursor_saved_cell_x;
    uint16_t cursor_saved_cell_y;
    vt_cell cursor_prev_cell_state;

    uint16_t saved_offset[2];

    bool disable_cursor, disable_tty;

    bool caps_lock, num_lock, scroll_lock, ctrl_held, shift_held;
    
    // Reading user input
    char input_buffer[256];
    volatile size_t input_data_size;
    spinlock_t input_lock;

    // Writing data to the user
    char *output_buffer;
    volatile size_t output_data_size;
    volatile size_t output_buffer_size;
    spinlock_t output_lock;

    task_t* output_task; // For writing data

    drivers::GraphicsDriver* driver;

    vnode_t* node;

    int foreground_pgrp;

    volatile termios settings;

    vt_state_t state;
    int csi_params[16];
    int csi_param_count;
    bool private_mode;

    uint16_t dirty_min_x;
    uint16_t dirty_min_y;
    uint16_t dirty_max_x;
    uint16_t dirty_max_y;

    virtual_terminal(drivers::GraphicsDriver* driver);

    void write(const char* str, uint32_t length);
    void write(wchar_t chr);

    void handle_csi_command(char buffer);
    void handle_sgr(int *parameter_list, int parameter_count);
    void set_cell(uint16_t x, uint16_t y, wchar_t chr, uint16_t attr, uint32_t fg, uint32_t bg);
    void print_cell(uint16_t x, uint16_t y);
    void scroll();

    char vt_process_event(struct input_event *ev);

    void clear();
    void clear(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);

    void mark_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void reset_dirty();
    void update_screen();

    void draw_cursor(bool clear = false);
};


void kprintf(const char* str, ...);
void kprintfva(const char* str, va_list args);
extern struct virtual_terminal* main_vt;