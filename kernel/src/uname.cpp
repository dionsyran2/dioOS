#include <uname.h>
#include <CONFIG.h>

utsname k_utsname = {
    "dioOS",
    "dioOS",
    KERNEL_VERSION_STRING, // Some executables refuse to launch if this is too low... so I guess we are gonna follow the linux version scheme
    "BUILD " __DATE__ " " __TIME__,
    "x86_64",
    ""
};


utsname* get_uname(){
    return &k_utsname;
}