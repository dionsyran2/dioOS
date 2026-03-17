#pragma once
#include <stdint.h>


/* Page Entry Flags */
enum PT_Flag{
    Present = 0, /* Whether the page is present */
    Write = 1,  /* Whether its writable*/
    User = 2, /* Whether its accessible in user space (cpl = 3)*/
    WriteThrough = 3, /* I have no idea */
    CacheDisable = 4, /* Disables caching */
    Accessed = 5,
    LargerPages = 7,
    Custom0 = 9,
    Custom1 = 10,
    Custom2 = 11,
    NX = 63 /* Not Executable (Only if supported, otherwise Reserved!)*/
};

struct PageEntry{
    uint64_t value;

    bool is_flag_set(PT_Flag flag);
    void set_flag(PT_Flag flag, bool value);
    uint64_t get_address();
    void set_address(uint64_t);
};

struct PageTable{
    PageEntry entries[512];
};