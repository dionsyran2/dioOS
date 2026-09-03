#include <line_discipline/line_discipline.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <bits/poll.h>
#include <kerrno.h>

namespace line_discipline{
    extern devfs_ops_t vt_devfs_ops; // forward reference for the operations

    void input_handler(char chr, void *ctx);
    void input_handler_evdev(evdev_t* evdev, input_event *event, void *context);

    void register_vt(tty_device_t *vt, const char* filename){
        __ld_vt_info *ctx = new __ld_vt_info;
        ctx->vt = vt;

        // Output Processing: Translate \n to \r\n so text doesn't stair-step
        ctx->termios_state.c_oflag = OPOST | ONLCR;

        // Input Processing: Translate Enter key (\r) into standard \n
        ctx->termios_state.c_iflag = ICRNL;

        // Local Modes: Buffer lines, echo text, and enable signals
        ctx->termios_state.c_lflag = ICANON | ECHO | ECHOE | ISIG;

        // Default Control Characters
        ctx->termios_state.c_cc[VINTR]  = '\x03'; // Ctrl+C
        ctx->termios_state.c_cc[VERASE] = '\x08'; // Backspace (\b)
        ctx->termios_state.c_cc[VEOF]   = '\x04'; // Ctrl+D

        vt->input_handler_ctx = ctx;
        vt->input_handler = input_handler;
        vt->input_handler_evdev = input_handler_evdev;

        devfs::mknod(filename, S_IFCHR | 0666, &vt_devfs_ops, ctx);
    }

    void input_handler(char chr, void *context){
        __ld_vt_info *ctx = (__ld_vt_info*)context;
        uint64_t rflags = spin_lock(&ctx->lock);


        if (ctx->termios_state.c_lflag & ISIG) {
            int sig_to_send = 0;

            if (chr == ctx->termios_state.c_cc[VINTR]) {
                sig_to_send = SIGINT;  // Ctrl+C
            } else if (chr == ctx->termios_state.c_cc[VQUIT]) {
                sig_to_send = SIGQUIT; // Ctrl+\ 
            } else if (chr == ctx->termios_state.c_cc[VSUSP]) {
                sig_to_send = SIGTSTP; // Ctrl+Z
            }

            if (sig_to_send != 0) {
                task_t *fgp = task_scheduler::search_by_pid(ctx->fg_pgid);
                if (fgp) {
                    fgp->signal(sig_to_send);
                }

                // Flush the input buffer so half-typed commands are destroyed
                ctx->input_head = 0;
                ctx->input_tail = 0;
                ctx->lines_available = 0;

                // Visually echo the control character (e.g., '^C')
                if (ctx->termios_state.c_lflag & ECHO) {
                    char echo_buf[2] = {'^', (char)(chr + 64)};
                    ctx->vt->write(echo_buf, 2, true); // true = process ONLCR to newline
                }

                spin_unlock(&ctx->lock, rflags);
                return;
            }
        }
        
        if ((chr == ctx->termios_state.c_cc[VERASE] || chr == '\x7F') && 
            (ctx->termios_state.c_lflag & ICANON)) {
            
            // Only erase if there is actually data in the current line buffer!
            if (ctx->input_head != ctx->input_tail) {
                ctx->input_head = (ctx->input_head - 1 + sizeof(ctx->input_buffer)) % sizeof(ctx->input_buffer);

                if (ctx->termios_state.c_lflag & ECHOE) {
                    ctx->vt->write("\b \b", 3, false); 
                } else {
                    ctx->vt->write(&chr, 1, false);
                }
            }
            spin_unlock(&ctx->lock, rflags);
            return;
        }

        if (chr == '\r' /*&& (ctx->termios_state.c_iflag & ICRNL)*/) {
            chr = '\n';
        }

        // Check if we should echo
        if (ctx->termios_state.c_lflag & ECHO){
            ctx->vt->write(&chr, 1, (ctx->termios_state.c_oflag & ONLCR) != 0);
        }

        // Insert into the ring buffer
        ctx->input_buffer[ctx->input_head] = chr;
        ctx->input_head = (ctx->input_head + 1) % sizeof(ctx->input_buffer);

        // Track lines for Canonical mode
        if ((chr == '\n' || chr == '\r' || chr == ctx->termios_state.c_cc[VEOF]) && ctx->termios_state.c_lflag & ICANON) {
            ctx->lines_available++;
        }

        bool readable = ctx->has_readable_data();
    
        spin_unlock(&ctx->lock, rflags);

        if (readable) {
            // Wake up any threads blocked in sys_poll
            ctx->wake_poll_list(POLLIN);
        }
    }

    void input_handler_evdev(evdev_t* evdev, input_event *event, void *context) {
        __ld_vt_info *ctx = (__ld_vt_info*)context;

        if (event->type != EV_KEY) return;

        uint16_t code = event->code;
        int value = event->value; // 0 = Release, 1 = Press, 2 = Repeat

        if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
            ctx->shift_held = (value != 0);
            return;
        }
        if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) {
            ctx->ctrl_held = (value != 0);
            return;
        }

        if (value == 0) return;

        if (value == 1) {
            bool led_changed = false;
            
            if (code == KEY_CAPSLOCK) {
                ctx->caps_lock = !ctx->caps_lock;
                led_changed = true;
            } else if (code == KEY_NUMLOCK) {
                ctx->num_lock = !ctx->num_lock;
                led_changed = true;
            } else if (code == KEY_SCROLLLOCK) {
                ctx->scroll_lock = !ctx->scroll_lock;
                led_changed = true;
            }

            if (led_changed) {
                input_event led_ev;
                led_ev.time = event->time;
                led_ev.type = EV_LED;
                
                if (code == KEY_CAPSLOCK) led_ev.code = LED_CAPSL;
                else if (code == KEY_NUMLOCK) led_ev.code = LED_NUML;
                else if (code == KEY_SCROLLLOCK) led_ev.code = LED_SCROLL;
                
                led_ev.value = (code == KEY_CAPSLOCK) ? ctx->caps_lock :
                            (code == KEY_NUMLOCK) ? ctx->num_lock : ctx->scroll_lock;
                            
                evdev->write(&led_ev, sizeof(input_event));
            }
            
            if (led_changed) return; 
        }

        char chr = 0;

        // --- NUMPAD HANDLING ---
        bool is_numpad = (code >= KEY_KP7 && code <= KEY_KPDOT) || 
                        code == KEY_KPENTER || code == KEY_KPSLASH || 
                        code == KEY_KPEQUAL || code == KEY_KPMINUS || 
                        code == KEY_KPPLUS || code == KEY_KPASTERISK;

        if (is_numpad) {
            // Operators are unaffected by Num Lock
            if (code == KEY_KPMINUS) chr = '-';
            else if (code == KEY_KPPLUS) chr = '+';
            else if (code == KEY_KPASTERISK) chr = '*';
            else if (code == KEY_KPSLASH) chr = '/';
            else if (code == KEY_KPEQUAL) chr = '=';
            else if (code == KEY_KPENTER) chr = '\n';
            else {
                // Numpad Numbers vs Navigation (XOR logic)
                bool type_numbers = ctx->num_lock != ctx->shift_held;

                if (type_numbers) {
                    switch(code) {
                        case KEY_KP7: chr = '7'; break;
                        case KEY_KP8: chr = '8'; break;
                        case KEY_KP9: chr = '9'; break;
                        case KEY_KP4: chr = '4'; break;
                        case KEY_KP5: chr = '5'; break;
                        case KEY_KP6: chr = '6'; break;
                        case KEY_KP1: chr = '1'; break;
                        case KEY_KP2: chr = '2'; break;
                        case KEY_KP3: chr = '3'; break;
                        case KEY_KP0: chr = '0'; break;
                        case KEY_KPDOT: chr = '.'; break;
                    }
                } else {
                    // Navigation mode: Send ANSI escape sequences to the terminal
                    const char *seq = nullptr;
                    switch(code) {
                        case KEY_KP8: seq = "\e[A"; break;  // Up
                        case KEY_KP2: seq = "\e[B"; break;  // Down
                        case KEY_KP6: seq = "\e[C"; break;  // Right
                        case KEY_KP4: seq = "\e[D"; break;  // Left
                        case KEY_KP7: seq = "\e[H"; break;  // Home
                        case KEY_KP1: seq = "\e[F"; break;  // End
                        case KEY_KP9: seq = "\e[5~"; break; // PgUp
                        case KEY_KP3: seq = "\e[6~"; break; // PgDn
                        case KEY_KP0: seq = "\e[2~"; break; // Insert
                        case KEY_KPDOT: seq = "\e[3~"; break; // Delete
                    }
                    
                    if (seq) {
                        // Feed the sequence into the line discipline one byte at a time
                        for (int i = 0; seq[i] != '\0'; i++) {
                            input_handler(seq[i], context);
                        }
                    }
                    return;
                }
            }
        } else {
            const char *seq = nullptr;
            switch(code) {
                case KEY_UP:       seq = "\e[A"; break;
                case KEY_DOWN:     seq = "\e[B"; break;
                case KEY_RIGHT:    seq = "\e[C"; break;
                case KEY_LEFT:     seq = "\e[D"; break;
                case KEY_HOME:     seq = "\e[H"; break;
                case KEY_END:      seq = "\e[F"; break;
                case KEY_PAGEUP:   seq = "\e[5~"; break;
                case KEY_PAGEDOWN: seq = "\e[6~"; break;
                case KEY_INSERT:   seq = "\e[2~"; break;
                case KEY_DELETE:   seq = "\e[3~"; break;
            }
            
            if (seq) {
                for (int i = 0; seq[i] != '\0'; i++) {
                    input_handler(seq[i], context);
                }
                return;
            }
            
            if (code >= 128) return;

            bool use_upper = ctx->shift_held;
            
            bool is_letter = (keymap_lower[code] >= 'a' && keymap_lower[code] <= 'z');
            if (ctx->caps_lock && is_letter) {
                use_upper = !use_upper;
            }

            chr = use_upper ? keymap_upper[code] : keymap_lower[code];
        }

        if (chr == 0) return;

        if (ctx->ctrl_held && chr >= 'a' && chr <= 'z') {
            chr = chr - 'a' + 1; 
        }

        input_handler(chr, context);
    }

    // devfs Stuff
    int vt_dev_poll(void *context, int events, poll_table_t *pt){
        __ld_vt_info *ctx = (__ld_vt_info*)context;

        // Change of plans, add it to the list immidiatelly
        if (pt != nullptr){
            pt->poll_list.lock();
            ctx->polling_list.lock();
            
            bool found = false;

            for (int i = 0; i < ctx->polling_list.size(); i++){
                if (ctx->polling_list.get(i) != pt) continue;

                found = true;
                break;
            }

            if (!found){
                ctx->polling_list.add(pt);

                pt->poll_list.add(&ctx->polling_list);

            }

            ctx->polling_list.unlock();
            pt->poll_list.unlock();
        }

        bool in = ctx->has_readable_data(); // Check if there is data to read
        bool out = true; // Well yes we can write to the vt

        int ret = 0;

        // Check in
        if ((events & POLLIN) && in){
            ret |= POLLIN;
        }

        // Check out
        if ((events & POLLOUT) && out){
            ret |= POLLOUT;
        }

        return ret;
    }

    void vt_read_cb(poll_table_t *pt){
        task_t *task = (task_t*)pt->ctx;
        task->unblock();
    }

    int vt_dev_read(void *context, void *buffer, size_t size, size_t offset){
        __ld_vt_info *ctx = (__ld_vt_info*)context;
        if (!ctx || !ctx->vt || !buffer) return -1;

        task_t *self = task_scheduler::get_current_task();
        if (!self) return -1;

        poll_table_t *pt = new poll_table_t();
        pt->callback = vt_read_cb;
        pt->ctx = self;
        pt->events = POLLIN;
        
        uint64_t rflags = 0;

        while (true) {
            self->current_state = INTERRUPTABLE;
            
            int poll_res = vt_dev_poll(context, POLLIN, pt);
            
            if (poll_res & POLLIN) {
                rflags = spin_lock(&ctx->lock);
                if (ctx->has_readable_data()) {
                    self->current_state = RUNNING; 
                    break;
                }
                spin_unlock(&ctx->lock, rflags);
            }

            if (self->current_state == INTERRUPTABLE) self->block();
            if (self->block_status < 0) return self->block_status;
        }

        // --- Cleanup Poll Table ---
        pt->poll_list.lock();
        kstd::linked_list_t<poll_table_t *> *q = pt->poll_list.get(0);
        q->lock();
        for (int i = 0; i < q->size(); i++){
            if (q->get(i) == pt){
                q->remove(i);
                break;
            }
        }
        q->unlock();
        pt->poll_list.unlock();
        delete pt;

        bool icannon = ctx->termios_state.c_lflag & ICANON;
        char *dest = (char*)buffer;
        size_t bytes_read = 0;

        while (ctx->input_tail != ctx->input_head && bytes_read < size){
            char c = ctx->input_buffer[ctx->input_tail];
            ctx->input_tail = (ctx->input_tail + 1) % sizeof(ctx->input_buffer);

            dest[bytes_read++] = c;

            if (icannon && (c == '\n' || c == '\r' || c == ctx->termios_state.c_cc[VEOF])) {
                ctx->lines_available--;
                break;
            }
        }

        spin_unlock(&ctx->lock, rflags);

        return bytes_read;
    }

    int vt_dev_write(void *context, const void *buffer, size_t size, size_t offset){
        __ld_vt_info *ctx = (__ld_vt_info*)context;
        if (!ctx || !ctx->vt || !buffer) return -1;

        const char *str = (const char*)buffer;
        bool do_onlcr = (ctx->termios_state.c_oflag & ONLCR) != 0;

        ctx->vt->write((char*)str, size, do_onlcr);

        return size;
    }

    bool __ld_vt_info::has_readable_data() {
        if (termios_state.c_lflag & ICANON) {
            return lines_available > 0; // Wait for a full line (\n)
        } else {
            return input_head != input_tail; // Any byte will do
        }
    }

    void __ld_vt_info::wake_poll_list(int event){
        this->polling_list.lock();

        for (int i = 0; i < this->polling_list.size();) {
            poll_table_t *pt = this->polling_list.get(i);

            if (pt->events & event){ // If its listening for this event
                pt->callback(pt);
                this->polling_list.remove(i);
                continue;
            }

            i++;
        }

        this->polling_list.unlock();
    }

    int vt_dev_ioctl(void *context, int op, char* argp){
        __ld_vt_info *ctx = (__ld_vt_info*)context;
        task_t *self = task_scheduler::get_current_task();
        if (!ctx || !argp) return -EINVAL;

        switch (op) {
            case TCGETS: {
                // Copy kernel's current termios state back to userspace
                if (self) {
                    self->write_to_userspace(argp, &ctx->termios_state, sizeof(termios));
                } else {
                    memcpy(argp, &ctx->termios_state, sizeof(termios));
                }
                return 0;
            }
            case TCSETS: {
                // Copy new termios state from userspace
                if (self) {
                    self->read_from_userspace(&ctx->termios_state, argp, sizeof(termios));
                } else {
                    memcpy(argp, &ctx->termios_state, sizeof(termios));
                }
                
                // Tell the hw to update (Baud rate, parity, etc.)
                ctx->vt->apply_termios(&ctx->termios_state);
                return 0;
            }
            case TIOCGWINSZ: {
                if (self) {
                    self->write_to_userspace(argp, &ctx->vt->ws, sizeof(struct winsize));
                } else {
                    memcpy(argp, &ctx->vt->ws, sizeof(struct winsize));
                }
                return 0;
            }

            case TIOCGPGRP: {
                // Get the foreground process group ID
                if (self) {
                    // yeah i dont wanna implement SIGTTIN & SIGTTOU rn so this will do
                    self->write_to_userspace(argp, &self->pgid, sizeof(int));
                } else {
                    memcpy(argp, &ctx->fg_pgid, sizeof(int));
                }
                return 0;
            }
            case TIOCSPGRP: {
                int new_pgid = 0;
                if (self) {
                    self->read_from_userspace(&new_pgid, argp, sizeof(int));
                } else {
                    memcpy(&new_pgid, argp, sizeof(int));
                }
                
                ctx->fg_pgid = new_pgid;
                return 0;
            }
            default:
                // If the serial port or VT has custom hardware ioctls, let them handle it
                return ctx->vt->ioctl(op, argp);
        }
    }

    devfs_ops_t vt_devfs_ops = {
        .read = vt_dev_read,
        .write = vt_dev_write,
        .ioctl = vt_dev_ioctl,
        .poll = vt_dev_poll
    };
}