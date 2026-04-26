#include <pty/pty.h>
#include <math.h>
#include <memory.h>
#include <memory/heap.h>
#include <cstr.h>
#include <syscalls/files/ioctl.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <kerrno.h>
#include <signum.h>

int pty_pipe::read(int size, void *buffer, bool *empty){
    uint64_t rflags = spin_lock(&this->lock);

    if (!this->buffer || !this->buffer_size || !this->offset) {
        spin_unlock(&this->lock, rflags); // No data
        return 0;
    }

    size_t amount = min(this->offset, size);
    memcpy(buffer, this->buffer, amount);

    size_t new_offset = this->offset - amount;

    *empty = false;

    if (new_offset != 0) {
        memmove(this->buffer, (char*)this->buffer + amount, new_offset);
    } else {
        *empty = true;
    }

    this->offset = new_offset;
    spin_unlock(&this->lock, rflags);
    return amount;
}

int pty_pipe::write(int size, const void *buffer){
    uint64_t rflags = spin_lock(&this->lock);

    if (this->offset + size > this->buffer_size){
        void *new_buffer = malloc(this->offset + size);

        if (this->buffer){
            memcpy(new_buffer, this->buffer, this->offset);
            free(this->buffer);
        }

        this->buffer = new_buffer;
        this->buffer_size = this->offset + size;
    }

    memcpy((char*)this->buffer + this->offset, buffer, size);
    this->offset += size;

    spin_unlock(&this->lock, rflags);
    return size;
}


int pty_master_read(uint64_t offset, uint64_t size, void* buf, vnode_t* node){
    task_t *self = task_scheduler::get_current_task();
    pty_pair* pair = (pty_pair*)node->file_identifier;

    // Check the actual buffer size, not the boolean flag
    while (true) {
        uint64_t rflags = spin_lock(&pair->to_master.lock);
        if (pair->to_master.offset > 0) {
            spin_unlock(&pair->to_master.lock, rflags);
            break; 
        }
        spin_unlock(&pair->to_master.lock, rflags);
        self->ScheduleFor(5, BLOCKED);
    }

    bool empty = false;
    int ret = pair->to_master.read(size, buf, &empty);

    if (empty){
        node->data_ready_to_read = false;
    }

    return ret;
}

int pty_slave_read(uint64_t offset, uint64_t size, void* buf, vnode_t* node){
    task_t *self = task_scheduler::get_current_task();
    pty_pair* pair = (pty_pair*)node->file_identifier;

    while (true) {
        uint64_t rflags = spin_lock(&pair->to_slave.lock);
        if (pair->to_slave.offset > 0) {
            spin_unlock(&pair->to_slave.lock, rflags);
            break; 
        }
        spin_unlock(&pair->to_slave.lock, rflags);
        self->ScheduleFor(5, BLOCKED); // Yield until data arrives
    }


    bool empty = false;

    int ret = pair->to_slave.read(size, buf, &empty);
    
    if (empty){
        node->data_ready_to_read = false;
    }

    return ret;
}


int pty_master_write(uint64_t offset, uint64_t size, const void* buf, vnode_t* node) {
    pty_pair* pair = (pty_pair*)node->file_identifier;
    const char* src = (const char*)buf;

    bool wrote_to_self = false;

    for(size_t i = 0; i < size; i++) {
        char c = src[i];

        if (pair->conf.c_lflag & ISIG) {
            if (c == '\x03') { // Ctrl+C (SIGINT)
                if (pair->foreground_pgrp > 0) {
                    // Send SIGINT to the foreground process group!
                    task_t *fg_task = task_scheduler::get_process(pair->foreground_pgrp);
                    if (fg_task) {
                        fg_task->pending_signals |= (1ULL << (SIGINT - 1));
                        
                        // Forcefully wake the process if it's waiting for input!
                        if (fg_task->task_state == BLOCKED) {
                            fg_task->woke_by_signal = true;
                            fg_task->task_state = PAUSED; // Move to Ready queue
                        }
                    }
                }
                
                continue; 
            }
        }
        
        // Handle Backspace
        if (c == 0x7f || c == 0x08) {
            if (pair->conf.c_lflag & ECHOE) {
                pair->to_master.write(3, "\b \b"); // Visually erase on xterm
                wrote_to_self = true;
            }
        } else {
            if (pair->conf.c_lflag & ECHO) {
                pair->to_master.write(1, &c); // Echo character back to xterm
                wrote_to_self = true;
            }
        }

        // Map CR to NL
        if (c == '\r' && (pair->conf.c_iflag & ICRNL)) {
            c = '\n';
        }

        pair->to_slave.write(1, &c);
    }

    pair->slave_node->data_ready_to_read = true;

    if (wrote_to_self){
        node->data_ready_to_read = true;
    }
    return size;
}

int pty_slave_write(uint64_t offset, uint64_t size, const void* buf, vnode_t* node) {
    pty_pair* pair = (pty_pair*)node->file_identifier;
    const char* src = (const char*)buf;

    for (size_t i = 0; i < size; i++) {
        if (src[i] == '\n' && (pair->conf.c_oflag & ONLCR)) {
            char cr = '\r';
            pair->to_master.write(1, &cr);
        }
        pair->to_master.write(1, &src[i]);
    }
    
    pair->master_node->data_ready_to_read = true;
    return size;
}

int pty_common_ioctl(int op, char* argp, vnode_t* node){
    pty_pair* pair = (pty_pair*)node->file_identifier;
    task_t* self = task_scheduler::get_current_task();

    switch (op) {
        case TCGETS:
            self->write_memory(argp, &pair->conf, sizeof(struct termios));
            return 0;
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            self->read_memory(argp, &pair->conf, sizeof(struct termios));
            return 0;
        case TIOCGWINSZ:
            self->write_memory(argp, &pair->windowsize, sizeof(struct winsize));
            return 0;
        case TIOCSWINSZ:
            self->read_memory(argp, &pair->windowsize, sizeof(struct winsize));
            
            if (pair->foreground_pgrp > 0) {
                // You'll need to call whatever signal delivery function your OS uses
                // Usually something like:
                // signal_send_group(pair->foreground_pgrp, SIGWINCH);
                task_scheduler::get_process(pair->foreground_pgrp)->pending_signals |= (1 << (SIGWINCH - 1));
            }
            return 0;
        case (int)TIOCGPTN: // Get PTY Number (called on Master)
            self->write_memory(argp, &pair->id, sizeof(int));
            return 0;
        case TIOCSPTLCK: // Set PTY Lock (usually just 0 to unlock)
            return 0;

        case TIOCSCTTY: {
            node->open();

            if (self->ctty){
                self->ctty->close();
            }

            self->ctty = node;
            return 0;
        }

        case FIONREAD: {
            pty_pipe* target_pipe = (node == pair->master_node) ? &pair->to_master : &pair->to_slave;
            
            uint64_t rflags = spin_lock(&target_pipe->lock);
            int size = target_pipe->offset; 
            spin_unlock(&target_pipe->lock, rflags);
            
            self->write_memory(argp, &size, sizeof(int));
            return 0;
        }

        case TIOCSPGRP: {
            // Set the foreground process group
            pid_t pgrp;
            if (self->read_memory(argp, &pgrp, sizeof(pid_t)) < 0) return -EFAULT;
            
            pair->foreground_pgrp = pgrp;
            return 0;
        }

        case TIOCGPGRP: {
            // Get the foreground process group
            self->write_memory(argp, &pair->foreground_pgrp, sizeof(pid_t));
            return 0;
        }
    }

    return -ENOTTY;
}


int ptmxid = 0;

int ptmx_special_open(vnode_t *node) {
    task_t *self = task_scheduler::get_current_task();

    int id = __atomic_fetch_add(&ptmxid, 1, __ATOMIC_SEQ_CST);

    pty_pair* pair = new pty_pair;
    memset(pair, 0, sizeof(pty_pair));

    pair->id = id;
    //pair->conf.c_oflag = ONLCR;
    //pair->conf.c_iflag = ICRNL;
    pair->conf.c_lflag = ECHO;
    
    // Create an abstract, anonymous vnode for the Master
    vnode_t* master_node = new vnode_t(VCHR);
    master_node->open();
    master_node->open();

    strcpy(master_node->name, "pty_master");

    master_node->file_operations.read = pty_master_read;
    master_node->file_operations.write = pty_master_write;
    master_node->file_operations.ioctl = pty_common_ioctl;
    
    master_node->file_identifier = (uint64_t)pair;
    
    pair->master_node = master_node;
    
    // Link the slave into your devfs/memfs
    char slave_path[32];
    stringf(slave_path, sizeof(slave_path), "/dev/pts/%d", pair->id);
    vnode_t *slave_node = vfs::create_path(slave_path, VCHR);

    slave_node->file_operations.read = pty_slave_read;
    slave_node->file_operations.write = pty_slave_write;
    slave_node->file_operations.ioctl = pty_common_ioctl;

    slave_node->file_identifier = (uint64_t)pair;

    pair->slave_node = slave_node;
    
    fd_t *ofd = self->open_node(master_node);
    return ofd->num;
}

void initialize_pty_multiplexer(){
    vnode_t *ptmx = vfs::create_path("/dev/ptmx", VCHR);
    ptmx->file_operations.special_open = ptmx_special_open;

    ptmx->close();
}