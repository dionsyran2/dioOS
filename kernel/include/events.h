#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <input-event-codes.h>
#include <filesystem/vfs/vfs.h>

#define RING_SIZE       256
#define EVDEV_VERSION   0x010001

struct input_event{
    struct timeval time;
    unsigned short type;
    unsigned short code;
    int value;
};

class event_node_t{
    public:
    event_node_t(vnode_t* node);
    ~event_node_t();

    void write(input_event* event);
    size_t read(char* user_buffer, size_t count);
    private:
    spinlock_t lock = 0;

    vnode_t* parent_node;
    input_event event_ring[RING_SIZE];
    volatile size_t head = 0;
    volatile size_t tail = 0;
};

vnode_t *create_event_file();