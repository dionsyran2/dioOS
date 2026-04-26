#include <helpers/sockets.h>
#include <syscalls/sockets.h>
#include <kerrno.h>
#include <memory/heap.h>
#include <memory.h>

int socket_open(vnode_t* this_node){
    return 0;
}

int socket_close(vnode_t* this_node){
    socket_t *info = (socket_t *)this_node->file_identifier;
    if (!info) return 0;
    
    if (info->domain == AF_UNIX){
        unix_socket_t *usock = (unix_socket_t *)info;
        if (usock->outbound) usock->outbound->eof = true;

        if (usock->inbound) usock->inbound->close();
        if (usock->outbound) usock->outbound->close();
    }

    

    delete info;

    this_node->file_identifier = 0;

    return 0;
}


int socket_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    unix_socket_t *info = (unix_socket_t *)this_node->file_identifier;
    if (!info) return -EFAULT;

    if (info->inbound) return info->inbound->read(buffer, length);
    return -EOPNOTSUPP;
}

int socket_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    unix_socket_t *info = (unix_socket_t *)this_node->file_identifier;
    if (!info) return -EFAULT;

    if (info->outbound) return info->outbound->write(buffer, length);
    return -EOPNOTSUPP;
}

bool socket_pollin(vnode_t* this_node){
    unix_socket_t *info = (unix_socket_t *)this_node->file_identifier;
    if (!info) return false;

    bool ret = info->outbound->pollin;

    return ret;
}

bool socket_pollout(vnode_t* this_node){
    unix_socket_t *info = (unix_socket_t *)this_node->file_identifier;
    if (!info) return false;

    bool ret = info->inbound->pollout;
    
    return ret;
}

void socket_transfer::open(){
    __atomic_fetch_add(&this->ref_cnt, 1, __ATOMIC_SEQ_CST);
}

void socket_transfer::close(){
    int ref = __atomic_sub_fetch(&this->ref_cnt, 1, __ATOMIC_SEQ_CST);

    if (ref == 0){
        this->clean();
        delete this;
    }
}

void socket_transfer::clean(){
    if (this->buffer) free(this->buffer);
}


void socket_transfer::wait(task_t *self){
    this->waiter = self;
    self->Block(UNSPECIFIED, 0);
}


long socket_transfer::write(const void *buf, size_t size){
    this->lock();
    if ((this->buffer_size - this->offset) < size || this->buffer == nullptr){
        size_t new_size = this->buffer_size ? this->buffer_size + (size * 2) : (size * 2);

        void *newbuf = malloc(new_size);

        if (this->buffer && this->offset){
            memcpy(newbuf, this->buffer, this->offset);
            free(this->buffer);
        }

        this->buffer = newbuf;
        this->buffer_size = new_size;
    }

    memcpy((char *)this->buffer + this->offset, buf, size);

    this->offset += size;
    this->pollout = this->offset != 0;
    this->unlock();

    if (this->waiter) this->waiter->Unblock();
    return size;
}

long socket_transfer::read(void *buf, size_t size){
    task_t *self = task_scheduler::get_current_task();

    this->lock();

    while (this->offset == 0 && !this->eof){
        this->waiter = self;

        this->unlock();

        self->Block(UNSPECIFIED, 0);

        this->lock();
        
        this->waiter = nullptr; 
    }

    if (this->offset == 0 && this->eof){
        this->unlock();
        return 0;
    }

    size_t to_read = min(this->offset, size);
    memcpy(buf, this->buffer, to_read);

    if (this->offset > to_read) {
        memmove(this->buffer, (char*)this->buffer + to_read, this->offset - to_read);
    }

    this->offset -= to_read;

    this->pollout = this->offset != 0;

    this->unlock();
    
    return to_read;
}

void socket_transfer::lock(){
    this->rflags = spin_lock(&this->slock);
}

void socket_transfer::unlock(){
    spin_unlock(&this->slock, this->rflags);
}


int connect_socket_structs(unix_socket_t *s1, unix_socket_t *s2, socket_transfer *t1, socket_transfer* t2){
    s1->inbound = t1;
    s1->outbound = t2;

    s2->inbound = t2;
    s2->outbound = t1;

    t1->open();
    t1->open();
    t2->open();
    t2->open();
    

    s1->file->file_operations.open = socket_open;
    s1->file->file_operations.close = socket_close;
    s1->file->file_operations.read = socket_read;
    s1->file->file_operations.write = socket_write;
    s1->file->file_operations.pollin = socket_pollin;
    s1->file->file_operations.pollout = socket_pollout;

    s2->file->file_operations.open = socket_open;
    s2->file->file_operations.close = socket_close;
    s2->file->file_operations.read = socket_read;
    s2->file->file_operations.write = socket_write;
    s2->file->file_operations.pollin = socket_pollin;
    s2->file->file_operations.pollout = socket_pollout;

    return 0;
}