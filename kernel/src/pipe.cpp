#include <pipe.h>

int WritePipe(vnode_t* pipe, const char* data, size_t length){
    pipe_data* p = (pipe_data*)pipe->fs_data;
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

    memcpy(p->data + p->offset_in_buffer, data, length);
    p->offset_in_buffer += length;
    pipe->data_available = true;
    return length;
}

char* ReadPipe(vnode_t* pipe, size_t* length){
    pipe_data* p = (pipe_data*)pipe->fs_data;
    while (p->offset_in_buffer == 0) __asm__("pause");

    size_t bsz = p->offset_in_buffer;
    char* out = new char[bsz];
    memcpy(out, p->data, bsz);
    p->offset_in_buffer = 0;
    *length = bsz;

    pipe->data_available = false;

    return out;
}

void* default_pipe_load(size_t* cnt, vnode* this_node){
    return ReadPipe(this_node, cnt);
}

int default_pipe_write(const char* data, size_t length, vnode_t* this_node){
    return WritePipe(this_node, data, length);
}

vnode_t* CreatePipe(const char* name, vnode_t* parent){
    vnode_t* node = new vnode_t;
    
    node->type == VNODE_TYPE::VPIPE;
    node->fs_data = malloc(sizeof(pipe_data));
    node->ops.load = default_pipe_load;
    node->ops.write = default_pipe_write;
    node->parent = parent;

    if (parent){
        parent->children[parent->num_of_children] = node;
        parent->num_of_children++;
    }
    memset(node->fs_data, 0, sizeof(pipe_data));

    strcpy(node->name, (char*)name);

    return node;
}

void ClosePipe(vnode_t* pipe){
    pipe_data* data = (pipe_data*)pipe->fs_data;
    if (data->data != nullptr) free(data->data);
    free(data);
    delete pipe; 
}