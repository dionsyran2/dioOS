#include <line_discipline/line_discipline.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <bits/poll.h>
#include <kerrno.h>

namespace line_discipline{
    extern devfs_ops_t vt_devfs_ops; // forward reference for the operations

    void input_handler(char chr, void *ctx);

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

        devfs::mknod(filename, DEVFS_CHR, &vt_devfs_ops, ctx);
    }

    void input_handler(char chr, void *context){
        __ld_vt_info *ctx = (__ld_vt_info*)context;
        uint64_t rflags = spin_lock(&ctx->lock);

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

        if (chr == '\r' && (ctx->termios_state.c_iflag & ICRNL)) {
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

    // devfs Stuff
    int vt_dev_poll(void *context, int events, poll_table_t *pt){
        __ld_vt_info *ctx = (__ld_vt_info*)context;

        // Change of plans, add it to the list immidiatelly
        if (pt != nullptr){
            ctx->polling_list.lock();
            
            bool found = false;

            for (int i = 0; i < ctx->polling_list.size(); i++){
                if (ctx->polling_list.get(i) != pt) continue;

                found = true;
                break;
            }

            if (!found){
                ctx->polling_list.add(pt);

                pt->poll_list.lock();
                pt->poll_list.add(&ctx->polling_list);
                pt->poll_list.unlock();

                ctx->polling_list.unlock();
            }
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

        ctx->wake_poll_list(POLLIN);

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
        
        int poll_res = 0;

        uint64_t rflags = 0;

        while (poll_res == 0){
            poll_res = vt_dev_poll(context, POLLIN, pt);
            self->block();

            if (poll_res){
                rflags = spin_lock(&ctx->lock);

                if (ctx->has_readable_data()){
                    break;
                }

                spin_unlock(&ctx->lock, rflags);
            }
        }

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


        // Now we have the lock and data so we can safely read
        bool icannon = ctx->termios_state.c_lflag & ICANON;
        char *dest = (char*)buffer;

        size_t bytes_read = 0;
        while (ctx->input_tail != ctx->input_head && bytes_read < size){
            char c = ctx->input_buffer[ctx->input_tail];
            ctx->input_tail = (ctx->input_tail + 1) % sizeof(ctx->input_buffer);

            dest[bytes_read++] = c;

            if (icannon && (c == '\n' || c == '\r')) {
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
        if (!ctx || !argp) return -EINVAL;

        switch (op) {
            case TCGETS: {
                // Copy kernel's current termios state back to userspace
                memcpy(argp, &ctx->termios_state, sizeof(termios));
                return 0;
            }
            case TCSETS: {
                // Copy new termios state from userspace
                memcpy(&ctx->termios_state, argp, sizeof(termios));
                
                // Tell the hw to update (Baud rate, parity, etc.)
                ctx->vt->apply_termios(&ctx->termios_state);
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