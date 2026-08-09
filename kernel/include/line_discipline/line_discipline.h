#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <structures/trees/avl_tree.h>
#include <bits/termbits.h>

struct evdev_t;

class tty_device_t {
    public:
    virtual void write(const char *data, size_t size, bool onlcr) = 0;
    virtual int ioctl(int op, char *argp) = 0;
    virtual void apply_termios(termios *state) = 0;

    void *input_handler_ctx;
    void (*input_handler)(char chr, void *ctx);
    void (*input_handler_evdev)(evdev_t* evdev, void *ctx);
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

        kstd::linked_list_t<poll_table_t*> polling_list;

        void wake_poll_list(int event);
        bool has_readable_data();
    };

    void register_vt(tty_device_t *vt, const char *filename);
}