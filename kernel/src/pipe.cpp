#include <pipe.h>

int WritePipe(vnode_t* pipe, const char* data, size_t length){
    pipe_data* p = (pipe_data*)pipe->misc_data[0];
    spin_lock(&p->lock);

    if (p->buffer_size < (length + p->offset_in_buffer)){
        size_t new_size = p->buffer_size + (length * 2);
        char* d = new char[new_size];
        if (p->data != nullptr) {
            memcpy(d, p->data, p->buffer_size);
            delete[] p->data;
        }
        p->data = d;
        p->buffer_size = new_size;
    }
    memcpy(&p->data[p->offset_in_buffer], data, length);
    p->offset_in_buffer += length;
    pipe->data_read = true;
    spin_unlock(&p->lock);
    return length;
}

char* ReadPipe(vnode_t* pipe, size_t* length){
    while (!pipe->data_read) __asm__("pause");
    pipe_data* p = (pipe_data*)pipe->misc_data[0];
    spin_lock(&p->lock);

    size_t bsz = p->offset_in_buffer;
    char* out = new char[bsz];
    memcpy(out, p->data, bsz);
    memset(p->data, 0, bsz);
    p->offset_in_buffer = 0;
    *length = bsz;

    pipe->data_read = false;

    spin_unlock(&p->lock);
    return out;
}

void* default_pipe_load(size_t* cnt, vnode* this_node){
    return ReadPipe(this_node, cnt);
}

int default_pipe_write(const char* data, size_t length, vnode_t* this_node){
    return WritePipe(this_node, data, length);
}

int delete_pipe_data(vnode_t* pipe, size_t length){
    pipe_data* p = (pipe_data*)pipe->misc_data[0];
    spin_lock(&p->lock);

    if (length > p->offset_in_buffer) {
        p->offset_in_buffer = 0;
    }else{
        p->offset_in_buffer -= length;
    }
    if (p->offset_in_buffer == 0) pipe->data_read = false;

    spin_unlock(&p->lock);

    return length;
}
vnode_t* CreatePipe(const char* name, vnode_t* parent){
    vnode_t* node = new vnode_t;
    
    node->type = VNODE_TYPE::VFIFO;
    node->misc_data[0] = malloc(sizeof(pipe_data));
    node->ops.read = default_pipe_load;
    node->ops.write = default_pipe_write;
    node->parent = parent;

    
    memset(node->misc_data[0], 0, sizeof(pipe_data));

    strcpy(node->name, (char*)name);

    return node;
}

void ClosePipe(vnode_t* pipe){
    pipe_data* data = (pipe_data*)pipe->misc_data[0];
    if (data->data != nullptr) free(data->data);
    free(data);
    delete pipe; 
}