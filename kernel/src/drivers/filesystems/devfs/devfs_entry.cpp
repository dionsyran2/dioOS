#include <drivers/filesystems/devfs/devfs_entry.h>
#include <kerrno.h>
#include <cstr.h>

namespace devfs{
    devfs_entry_t::devfs_entry_t(char *name, __devfs_entry_type_t type, void *context, devfs_ops_t *operations){
        strncpy(this->name, name, sizeof(this->name) / sizeof(char));

        this->type = type;
        this->operation_context = context;
        this->operations = operations;

        if (type == DEVFS_DIR){
            this->children = new kstd::linked_list_t<devfs_entry_t *>();
        }
    }

    devfs_entry_t::~devfs_entry_t(){
        if (type == DEVFS_DIR){
            delete this->children;
        }
    }

    int devfs_entry_t::read(void *buffer, size_t size, size_t offset){
        if (!this->operations || !this->operations->read) return -EOPNOTSUPP;

        return this->operations->read(this->operation_context, buffer, size, offset);
    }

    int devfs_entry_t::write(const void *buffer, size_t size, size_t offset){
        if (!this->operations || !this->operations->write) return -EOPNOTSUPP;

        return this->operations->write(this->operation_context, buffer, size, offset);
    }

    int devfs_entry_t::ioctl(int op, char* argp){
        if (!this->operations || !this->operations->ioctl) return -EOPNOTSUPP;

        return this->operations->ioctl(this->operation_context, op, argp);
    }

    int devfs_entry_t::poll(int events, poll_table_t *pt){
        if (!this->operations || !this->operations->ioctl) return -EOPNOTSUPP;

        return this->operations->poll(this->operation_context, events, pt);
    }
}