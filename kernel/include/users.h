#pragma once
#include <stdint.h>
#include <stddef.h>

#include <BasicRenderer.h>
#include <paging/PageFrameAllocator.h>
#include <paging/PageTableManager.h>
#include <memory/heap.h>

#include <VFS/vfs.h>

typedef struct {
    uint16_t UID;
    uint16_t GID;

    char username[32];
    char home[128];
    char comment[128];
    char shell[128];
    char pwd_hash[64];

    bool superuser;
} user_t;


bool load_users();
user_t* validate_user(char* user, char* passwd);