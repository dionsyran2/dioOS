#include <uname.h>

utsname k_utsname = {
    "dioOS",
    "dioOS",
    "5.0.1.0", // Some executables refuse to launch if this is too low... so I guess we are gonna follow the linux version scheme
    "BUILD " __DATE__ " " __TIME__,
    "x86_64",
    ""
};


utsname* get_uname(){
    return &k_utsname;
}