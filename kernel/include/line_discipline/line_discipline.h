#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <structures/trees/avl_tree.h>
#include <bits/termbits.h>
#include <input.h>

// Standard Linux evdev keycodes (0 to 58)
static const char keymap_lower[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\x7F',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0 // 57 is space
};

static const char keymap_upper[128] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\x7F',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0
};

class tty_device_t {
    public:
    virtual void write(const char *data, size_t size, bool onlcr) = 0;
    virtual int ioctl(int op, char *argp) = 0;
    virtual void apply_termios(termios *state) = 0;

    winsize ws;

    void *input_handler_ctx;
    void (*input_handler)(char chr, void *ctx);
    void (*input_handler_evdev)(evdev_t* evdev, input_event *event, void *ctx);
};

struct poll_table_t;

namespace line_discipline{
    struct __ld_vt_info{
        tty_device_t *vt;

        termios termios_state;
        char input_buffer[1024];
        int input_head = 0;
        int input_tail = 0;
        int lines_available = 0;
        spinlock_t lock;

        bool caps_lock = false;
        bool num_lock = false;
        bool scroll_lock = false;
        bool ctrl_held = false;
        bool shift_held = false;

        kstd::linked_list_t<poll_table_t*> polling_list;

        void wake_poll_list(int event);
        bool has_readable_data();
    };

    void register_vt(tty_device_t *vt, const char *filename);
}