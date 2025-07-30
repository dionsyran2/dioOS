#include <devices/devices.h>
#include <devices/null.h>

int init_vfs_dev(){
    initialize_null_dev();
    return 0;
}
