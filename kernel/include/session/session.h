#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/vfs.h>
#include <tty/tty.h>
#include <memory.h>
#include <memory/heap.h>
namespace session{
    typedef struct {
        vnode_t* tty;
        uint16_t session_id;

        void login();
    } session_t;

    session_t* CreateSession();
}