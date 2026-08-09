/* Contains the Virtual Terminal Definitions! */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <drivers/graphics/common.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <line_discipline/line_discipline.h>


#define VT_CELL_WIDTH 8
#define VT_CELL_HEIGHT 16


#define VT_INVERSE (1 << 0)
#define VT_BOLD (1 << 1)
#define VT_UNDERLINE (1 << 2)


#define VT_IS_INVERSE_SET(x) (x & VT_INVERSE);
#define VT_IS_BOLD_SET(x) (x & VT_BOLD);
#define VT_IS_UNDERLINE_SET(x) (x & VT_UNDERLINE);

enum __vt_state_t {
    VT_STATE_NORMAL,
    VT_STATE_ESC,
    VT_STATE_CSI,
    VT_STATE_CSI_PARAM,
};

struct __vt_cell{
    wchar_t chr;
    uint32_t attributes;

    uint32_t fg;
    uint32_t bg;
};


class virtual_terminal : public tty_device_t {
    public:
    virtual_terminal(drivers::GraphicsDriver *driver);

    // State management
    bool get_state();
    void activate();
    void deactivate();

    // Rendering
    void refresh();
    void render();
    void scroll();
    void mark_dirty(size_t x, size_t y, size_t width, size_t height);

    void clear();
    void clear(size_t start_x, size_t start_y, size_t end_x, size_t end_y);

    void set_cell(size_t x, size_t y, wchar_t chr, uint32_t attributes, uint32_t fg, uint32_t bg);
    __vt_cell get_cell(size_t x, size_t y);
    void render_cell(size_t x, size_t y);

    void draw_cursor(bool clear = false);

    void write(wchar_t chr);
    void write(const char *text, size_t length, bool onclr);

    // Escape sequences
    void handle_csi_command(char command);
    void handle_sgr(int *parameter_list, int parameter_count);

    // Stubs
    int ioctl(int op, char *argp);
    void apply_termios(termios *state);

    // Cursor stuff but public
    bool cursor_disabled = false;
    bool cursor_state = false;
    
    private:
    bool active = false;
    __vt_cell *cell_table;
    uint16_t width;
    uint16_t height;

    uint16_t current_attributes = 0;

    uint32_t current_fg = 0xFFFFFF;
    uint32_t current_bg = 0;

    drivers::GraphicsDriver* driver;

    /* Cursor stuff */
    
    size_t cursor_x = 0;
    size_t cursor_y = 0;

    size_t cursor_rendered_x = 0;
    size_t cursor_rendered_y = 0;
    __vt_cell previous_cursor_cell_state;

    /* State saving */
    __vt_state_t current_state;
    int csi_params[16];
    int csi_param_count;
    bool private_mode;


    /* Rendering optimisation */
    uint16_t dirty_min_x = 0;
    uint16_t dirty_min_y = 0;
    uint16_t dirty_max_x = 0;
    uint16_t dirty_max_y = 0;
};