#include <events.h>
#include <scheduling/spinlock/spinlock.h>
#include <memory.h>
#include <kstdio.h>
#include <scheduling/task_scheduler/task_scheduler.h>

int event_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    event_node_t* evt = (event_node_t*)this_node->fs_identifier;
    
    return evt->read((char*)buffer, length);
}

int event_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    event_node_t* evt = (event_node_t*)this_node->fs_identifier;
    
    int count = length / sizeof(input_event);
    
    for (int i = 0; i < count; i++){
        evt->write((input_event*)((uint64_t)buffer + (i * sizeof(input_event))));
    }
    return count * sizeof(input_event);
}

event_node_t::event_node_t(vnode_t* node){
    memset(this, 0, sizeof(event_node_t));
    memset(&node->file_operations, 0, sizeof(node->file_operations));
    node->fs_identifier = (uint64_t)this;
    node->file_operations.read = event_read;
    node->file_operations.write = event_write;
}

event_node_t::~event_node_t(){

}

void event_node_t::write(input_event* event){
    uint64_t rflags = spin_lock(&this->lock);
    size_t next_head = (head + 1) % RING_SIZE;

    if (next_head == tail) {
        tail = (tail + 1) % RING_SIZE;
    }

    memcpy(&this->event_ring[head], event, sizeof(input_event));
    head = next_head;

    spin_unlock(&this->lock, rflags);
    
    task_scheduler::wake_blocked_tasks(WAITING_ON_EVENT);
}

size_t event_node_t::read(char* buffer, size_t count) {
    task_t* self = task_scheduler::get_current_task();
    size_t bytes_copied = 0;
    size_t event_size = sizeof(input_event);

    while (bytes_copied + event_size <= count) {
        uint64_t rflags = spin_lock(&this->lock);

        if (head == tail) {
            spin_unlock(&this->lock, rflags);
            
            if (bytes_copied > 0) {
                return bytes_copied;
            }

            if (self){
                self->Block(WAITING_ON_EVENT, 0);
            } else {
                asm ("hlt");
            }
            
            continue; 
        }

        input_event *evt = &this->event_ring[tail];
        tail = (tail + 1) % RING_SIZE;

        spin_unlock(&this->lock, rflags);

        // Copy to user space
        memcpy(buffer + bytes_copied, evt, event_size);
        bytes_copied += event_size;
    }

    return bytes_copied;
}

void init_event_fs(){
    vnode_t* evt = vfs::create_path("/dev/input/event0", VCHR);

    if (evt){
        new event_node_t(evt);
        evt->close();
    }
}