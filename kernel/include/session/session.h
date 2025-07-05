#pragma once
#include <stdint.h>
#include <stddef.h>
#include <VFS/vfs.h>
#include <tty/tty.h>
#include <memory.h>
#include <memory/heap.h>
#include <users.h>

namespace session{
    typedef struct {
        vnode_t* tty;
        uint16_t session_id;

        user_t* user;
        void login();
    } session_t;

    session_t* CreateSession();
}