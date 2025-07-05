#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/vfs.h>
#include <syscalls/termios.h>

#define TTY_BOLD        1
#define TTY_UNDERLINE   2

namespace tty{
    typedef uint32_t Color;
    typedef uint8_t Attributes;
    typedef struct{
        char ch;
        Color fg;
        Color bg;
        Attributes attrs; // bold, underline, go fuck yourself
    } cell_t;

    typedef struct tty{
        vnode_t* node;

        uint32_t rows;
        uint32_t cols;
        uint32_t current_row;
        uint32_t current_column;

        uint8_t char_height; // lets use vga16 fonts since i do not wanna port my ttf code to this tty 'emulator' yet (8x16)
        uint8_t char_width;

        cell_t* screen;

        char* buffer;

        int offset_in_buffer;
        int edit_offset;

        int foreground_group_id;

        uint32_t style;
        uint32_t fg;
        uint32_t bg;

        bool _waiting_for_read;
        bool _data_to_read;
        bool _writing_data;
        bool _no_cursor;

        termios term; // FUCK SYSCALLS

        void draw_screen();
        void draw_cell(uint32_t row, uint32_t col);
        void set_chr(uint32_t row, uint32_t col, Color fb, Color bg, char chr, uint32_t attrs);
        cell_t* get_cell(uint32_t row, uint32_t col);
        void write_chr(char chr);
        void print(const char* str, uint32_t length);
        void clear();
        void scroll_up();
        void handle_key(char ascii);
        int handle_escape_sequence(const char*, int);
    } tty_t;


    extern tty_t* focused_tty;
    int CreateTerminal(char* name);
}