#include <uname.h>

utsname k_utsname = {
    "dioOS",
    "dioOS",
    "5.15.153.1",
    "BUILD " __DATE__ " " __TIME__,
    "x86_64",
    ""
};


utsname* get_uname(){
    return &k_utsname;
}