#pragma once
#include <stdint.h>
#include <stddef.h>

namespace InEvents{
    struct EVENT_DEFAULT{
        uint8_t type;
        uint64_t data;
        uint8_t data2;
        void* nextEvent;
    };

    struct EVENT_KB{
        uint8_t type; // = 1
        uint8_t scancode;
        uint8_t data;
        uint8_t rsv[7];
        void* nextEvent;
    };

    struct EVENT_MOUSE{
        uint8_t type; // = 2
        uint32_t x;
        uint32_t y;
        uint8_t data;
        void* nextEvent;
    };

    void SendEvent(void* event);
    
    extern void** EVENT_BUFFER;
}