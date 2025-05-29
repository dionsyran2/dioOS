#pragma once
#include <stdint.h>
#include <stddef.h>

typedef int PIPE;

struct DirEntry
{
    char name[255];
    uint8_t attributes;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t LastAccessed;
    uint16_t lastModificationTime;
    uint16_t lastModificationDate;
    uint32_t size;
    uint64_t rsv[2];
    // it has more but they are non accessible by apps
} __attribute__((packed));

struct DirectoryList{
    size_t numOfEntries = 0;
    DirEntry entries[256];
};

struct multiboot_color
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

struct framebuffer_t
{
    uint32_t type;
    uint32_t size;

    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2
    uint8_t fb_type;
    uint16_t reserved;

    union
    {
        struct
        {
            uint16_t framebuffer_palette_num_colors;
            struct multiboot_color framebuffer_palette[0];
        };
        struct
        {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
};

enum task_state_t{
    Stopped = 0,
    Running = 1,
    Paused = 2,
    Blocked = 3
};

struct thread_t{
    uint64_t pid;
    bool reserved;
    bool _exit = false; // Whether an exit was requested. if yes, any reserved memory will be unreserved and the task will be removed from the list.   
    char task_name[32];
    task_state_t state = Stopped;
    uint64_t stack = 0;
    uint64_t kernelStack = 0;
    uint8_t tasklist = 0;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t rip;
    uint8_t blockType = 0; //1 = Sleep, 2 = iowait.
    uint64_t block = 0;
    bool reducedCounter = 0; //If set, reduces the counter to 5ms
    bool userspace;
    void exit();
};

typedef struct time_t
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t day_of_week;
    uint8_t month;
    uint16_t year;
} time_t;

extern "C" framebuffer_t *GetFramebuffer();

extern "C" void *malloc(uint64_t size);
extern "C" void free(void *address);
void* realloc(void* oldBuffer, size_t newSize);

inline void* operator new(size_t size){return malloc(size);};
inline void* operator new[](size_t size){return malloc(size);};
inline void operator delete(void* p){free(p);};
inline void operator delete(void* p, long unsigned int){free(p);};
inline void operator delete [](void* p, long unsigned int){free(p);}
inline void operator delete[](void* p){free(p);}

extern "C" DirEntry *OpenFile(char* path);
extern "C" DirEntry *LoadFile(DirEntry* entry);
extern "C" void CloseFile(DirEntry* entry);


extern "C" void* RequestPage();
extern "C" void* RequestPages(uint64_t count);

extern "C" void* FreePage(void* address);
extern "C" void* FreePages(void* address, uint64_t count);

extern "C" void RegEvent(void** val);

extern "C" void SetTimeBuff(void* time);

extern "C" void AddInt(void* interrupt, uint8_t vector);

extern "C" void RunELF(DirEntry* entry);

extern "C" void Sleep(uint64_t milliseconds);

extern "C" thread_t* CreateThread(void* task, thread_t* parent);


extern "C" thread_t* GetThread();

extern "C" uint64_t GetSystemTicks();

extern "C" void RegQueryValue(char* keyPath, char* valueName, void* data);

extern "C" void sys_log(const char* str);

extern "C" PIPE CreatePipe(char* name);

extern "C" PIPE GetPipe(char* name);

extern "C" void DestroyPipe(PIPE hndl);

extern "C" char* ReadPipe(PIPE hndl);

extern "C" void WritePipe(PIPE hndl, char* wr);

extern "C" DirectoryList* GetDirectoryListing(char drive, DirEntry* directory);

void *memmove(void *dest, const void *src, size_t n);

void* memsetu(void* start, uint32_t value, uint64_t num);

void* memset(void* start, int value, uint64_t num);

void *memcpy(void* __restrict__ dest, const void* __restrict__ src, size_t n);


int memcmp(const void *s1, const void *s2, size_t n);

extern "C" void SetPower(int state);
