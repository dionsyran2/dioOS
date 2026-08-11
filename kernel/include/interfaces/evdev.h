#pragma once
#include <stdint.h>
#include <stddef.h>
#include <scheduling/spinlock/spinlock.h>
#include <sys/input_event_codes.h>
#include <structures/lists/linked_list.h>
#include <time.h>

#define EVDEV_RING_SIZE 256 

struct poll_table_t;


struct input_event{
    struct timeval time;
    unsigned short type;
    unsigned short code;
    int value;
};

typedef void (*evdev_hw_callback_t)(void *hw_context, input_event *event);


class evdev_t{
    public:
    evdev_t(evdev_hw_callback_t hw_write_cb, void *context);

    void push_event(input_event *event);
    int poll(int events, poll_table_t *pt);
    void wake_poll_list(int event);
    int read(void *buffer, size_t size);
    int write(void *buffer, size_t size);
    int ioctl(int request, void *argp);

    char name[128];
    int supported_events_mask;
        
    private:
    input_event ring[EVDEV_RING_SIZE];

    spinlock_t lock;

    size_t read_ptr;
    size_t write_ptr;

    kstd::linked_list_t<poll_table_t*> polling_list;

    evdev_hw_callback_t hw_callback = nullptr;
    void *hw_context = nullptr; 
};