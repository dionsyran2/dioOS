#include <interfaces/evdev.h>
#include <memory.h>
#include <bits/poll.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <kerrno.h>
#include <drivers/filesystems/devfs/devfs.h>
#include <cstr.h>

#define EVIOCGNAME_BASE 0x82004506
#define EVIOCGBIT_BASE  0x80004520

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

int evdev_ioctl(void *context, int op, char* argp){
    return -EOPNOTSUPP;
}

int evdev_read(void *context, void *buffer, size_t size, size_t offset){
    evdev_t *evdev = (evdev_t *)context;

    return evdev->read(buffer, size);
}
int evdev_poll(void *context, int events, poll_table_t *pt){
    evdev_t *evdev = (evdev_t*)context;

    return evdev->poll(events, pt);
}

devfs_ops_t evdev_ops = {
    .read = evdev_read,
    .ioctl = evdev_ioctl,
    .poll = evdev_poll,
};

int evdev_instance_count = 0;

evdev_t::evdev_t(evdev_hw_callback_t hw_write_cb, void *context){
    this->hw_callback = hw_write_cb;
    this->hw_context = context;

    this->read_ptr = 0;
    this->write_ptr = 0;
    this->lock = 0;

    devfs::mknod("/input", S_IFCHR | 0666, nullptr, nullptr);
    char buffer[128];
    stringf(buffer, sizeof(buffer), "/input/evdev%d", __atomic_fetch_add(&evdev_instance_count, 1, __ATOMIC_SEQ_CST));
    devfs::mknod(buffer, S_IFCHR | 0666, &evdev_ops, this);
}

void evdev_t::push_event(input_event *event){
    uint64_t rflags = spin_lock(&this->lock);

    if (this->read_ptr == this->write_ptr + 1){
        spin_unlock(&this->lock, rflags);
        return; // Ring full
    }

    memcpy(&this->ring[this->write_ptr], event, sizeof(input_event));

    this->write_ptr = (this->write_ptr + 1) % EVDEV_RING_SIZE;

    spin_unlock(&this->lock, rflags);
}

void evdev_t::wake_poll_list(int event){
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

int evdev_t::poll(int events, poll_table_t *pt){
    if (pt != nullptr){
        this->polling_list.lock();
        
        bool found = false;

        for (int i = 0; i < this->polling_list.size(); i++){
            if (this->polling_list.get(i) != pt) continue;

            found = true;
            break;
        }

        if (!found){
            this->polling_list.add(pt);

            pt->poll_list.lock();
            pt->poll_list.add(&this->polling_list);
            pt->poll_list.unlock();

            this->polling_list.unlock();
        }
    }

    bool in = this->read_ptr != this->write_ptr; // Check if there is data to read
    bool out = false; // You cannot write to the evdev as far as i know

    int ret = 0;

    // Check in
    if ((events & POLLIN) && in){
        ret |= POLLIN;
    }

    // Check out
    if ((events & POLLOUT) && out){
        ret |= POLLOUT;
    }

    this->wake_poll_list(POLLIN);

    return ret;
}

void evdev_read_cb(poll_table_t *pt){
    task_t *task = (task_t*)pt->ctx;
    task->unblock();
}

int evdev_t::read(void *buffer, size_t size){
    task_t *self = task_scheduler::get_current_task();
    if (!self) return -1;

    if (size < sizeof(input_event)) {
        return -EINVAL;
    }

    poll_table_t *pt = new poll_table_t();
    pt->callback = evdev_read_cb;
    pt->ctx = self;
    pt->events = POLLIN;
    
    int poll_res = 0;

    uint64_t rflags = 0;

    while (poll_res == 0){
        poll_res = this->poll(POLLIN, pt);
        self->block();

        if (poll_res){
            rflags = spin_lock(&this->lock);

            if (this->read_ptr != this->write_ptr){
                break;
            }

            spin_unlock(&this->lock, rflags);
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

    int events_to_read = size / sizeof(input_event);
    int events_read = 0;
    
    input_event *dest = (input_event *)buffer;

    while (events_read < events_to_read && this->read_ptr != this->write_ptr) {
        dest[events_read] = this->ring[this->read_ptr];
        
        this->read_ptr = (this->read_ptr + 1) % EVDEV_RING_SIZE;
        
        events_read++;
    }

    spin_unlock(&this->lock, rflags);

    return events_read * sizeof(input_event);
}

int evdev_t::write(void *buffer, size_t size) {
    task_t *self = task_scheduler::get_current_task();
    if (!self || size < sizeof(input_event)) return -EINVAL;

    int events_to_write = size / sizeof(input_event);
    int events_written = 0;
    
    input_event *src = (input_event *)buffer;

    for (int i = 0; i < events_to_write; i++) {
        input_event ev;
        ev = src[i];

        if (this->hw_callback) {
            this->hw_callback(this->hw_context, &ev);
        }
        
        events_written++;
    }

    return events_written * sizeof(input_event);
}

int evdev_t::ioctl(int request, void *argp) {
    task_t *self = task_scheduler::get_current_task();
    if (!self) return -EFAULT;

    if ((request & ~0x1FFF0000) == (EVIOCGNAME_BASE & ~0x1FFF0000)) {
        int max_len = (request >> 16) & 0x1FFF; // Extract size from ioctl number
        int len = min(max_len, (int)strlen(this->name) + 1);
        
        self->write_to_userspace(argp, this->name, len);
        return len;
    }

    if ((request & ~0x1FFF0000) == (EVIOCGBIT_BASE & ~0x1FFF0000)) {
        self->write_to_userspace(argp, &this->supported_events_mask, sizeof(uint64_t));
        return 0;
    }

    return -ENOTTY;
}