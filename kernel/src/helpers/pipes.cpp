#include <helpers/pipes.h>
#include <memory.h>
#include <memory/heap.h>
#include <math.h>
#include <kerrno.h>
#include <kstdio.h>
int v_pipe_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* node){
    pipe_t *pipe = (pipe_t*)node->fs_identifier;
    int ret = 0;

    while (true) {
        uint64_t rflags = spin_lock(&pipe->pipe_lock);

        // Condition A: We have data!
        if (pipe->buffer && pipe->write_offset > 0) {
            ret = min(pipe->write_offset, length);
            memcpy(buffer, pipe->buffer, ret);
            memmove(pipe->buffer, (char*)pipe->buffer + ret, pipe->write_offset - ret);
            pipe->write_offset -= ret;

            if (pipe->write_offset == 0) {
                node->data_ready_to_read = false;
            }

            spin_unlock(&pipe->pipe_lock, rflags);
            return ret; 
        }

        if (!pipe->write_node) {
            spin_unlock(&pipe->pipe_lock, rflags);
            return 0;
        }

        spin_unlock(&pipe->pipe_lock, rflags);
        task_scheduler::_swap_tasks();
    }
}

int v_pipe_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* node){
    pipe_t *pipe = (pipe_t*)node->fs_identifier;
    uint64_t rflags = spin_lock(&pipe->pipe_lock);

    if (!pipe->read_node){
        spin_unlock(&pipe->pipe_lock, rflags);
        return -EPIPE;
    }

    uint64_t required_size = pipe->write_offset + length;

    if (required_size > pipe->buffer_size){
        uint64_t new_size = pipe->buffer_size == 0 ? 4096 : pipe->buffer_size;
        while (new_size < required_size) new_size *= 2;

        void *new_buffer = malloc(new_size);
        if (!new_buffer) {
            spin_unlock(&pipe->pipe_lock, rflags);
            return -ENOMEM;
        }
        
        if (pipe->buffer) {
            memcpy(new_buffer, pipe->buffer, pipe->write_offset);
            free(pipe->buffer);
        }
        
        pipe->buffer = new_buffer;
        pipe->buffer_size = new_size;
    }

    memcpy((char*)pipe->buffer + pipe->write_offset, buffer, length);
    pipe->write_offset += length;
    pipe->read_node->data_ready_to_read = true;
    
    spin_unlock(&pipe->pipe_lock, rflags);
    return length;
}

int v_close(vnode_t *node){
    pipe_t *pipe = (pipe_t*)node->fs_identifier;
    
    uint64_t rflags = spin_lock(&pipe->pipe_lock);
    pipe->ref_cnt--;
    bool should_free = (pipe->ref_cnt == 0);

    if (pipe->write_node == node && node->ref_count == 0){
        pipe->write_node = nullptr;

        if (pipe->read_node){
            pipe->read_node->data_ready_to_read = true;
        }
    }

    if (pipe->read_node == node && node->ref_count == 0){
        pipe->read_node = nullptr;
    }

    spin_unlock(&pipe->pipe_lock, rflags);

    if (should_free){
        if (pipe->buffer){
            free(pipe->buffer);
        }
        delete pipe;
    }

    return 0;
}

#include <cstr.h>
pipe_t *create_pipe(){
    pipe_t *pipe = new pipe_t;
    memset(pipe, 0, sizeof(pipe_t));

    pipe->read_node = new vnode_t(VPIPE);
    pipe->write_node = new vnode_t(VPIPE);

    strcpy(pipe->read_node->name, "pipe_read");
    strcpy(pipe->write_node->name, "pipe_write");

    pipe->read_node->open();
    pipe->write_node->open();

    pipe->ref_cnt = 2;

    pipe->read_node->fs_identifier = (uint64_t)pipe;
    pipe->write_node->fs_identifier = (uint64_t)pipe;

    pipe->read_node->file_operations.close = v_close;
    pipe->write_node->file_operations.close = v_close;

    pipe->read_node->file_operations.read = v_pipe_read;
    pipe->write_node->file_operations.write = v_pipe_write;

    pipe->read_node->data_ready_to_read = false;

    return pipe;
}