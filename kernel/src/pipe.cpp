#include <pipe.h>

int64_t WritePipe(vnode_t* pipe, const void* data, size_t length){
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

int64_t ReadPipe(vnode_t* pipe, void* buffer, size_t cnt){
    while (!pipe->data_read) asm ("pause");
    pipe_data* p = (pipe_data*)pipe->misc_data[0];
    spin_lock(&p->lock);

    size_t offset = p->offset_in_buffer;
    size_t to_read = offset;
    if (offset == 0 || cnt == 0) {spin_unlock(&p->lock); return 0;}
    if (to_read > cnt) to_read = cnt;
    memcpy(buffer, p->data, to_read);

    if (to_read < offset){
        memmove(p->data, p->data + to_read, offset - to_read);
    }
    
    p->offset_in_buffer -= to_read;

    if (p->offset_in_buffer == 0) pipe->data_read = false;

    spin_unlock(&p->lock);
    return to_read;
}

int64_t default_pipe_load(void* buffer, size_t cnt, size_t offset, vnode* this_node){
    return ReadPipe(this_node, buffer, cnt);
}

int64_t default_pipe_write(const void* data, size_t cnt, size_t offset, vnode_t* this_node){
    return WritePipe(this_node, data, cnt);
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
    node->data_read = false;

    
    memset(node->misc_data[0], 0, sizeof(pipe_data));

    strcpy(node->name, (char*)name);

    spin_unlock(&((pipe_data*)node->misc_data[0])->lock);
    return node;
}

void ClosePipe(vnode_t* pipe){
    pipe_data* data = (pipe_data*)pipe->misc_data[0];
    if (data->data != nullptr) free(data->data);
    free(data);
    delete pipe; 
}