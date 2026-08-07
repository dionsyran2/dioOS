#include <scheduling/task_scheduler/cpu_task_queue.h>

cpu_task_queue_t::cpu_task_queue_t(size_t size){
    this->queue = (task_t**)malloc(sizeof(task_t*) * size);
    this->queue_size = size;
};

void cpu_task_queue_t::push(task_t *task){
    uint64_t flags = this->_lock();
    task->cpu_queue = this;
    
    if (((this->write_ptr + 1) % this->queue_size) == this->read_ptr){ 
        _expand_ring(true);
    }

    this->queue[this->write_ptr] = task;
    this->write_ptr = (this->write_ptr + 1) % this->queue_size;
    this->entry_count++;

    this->_unlock(flags);
}

task_t *cpu_task_queue_t::pop(){
    uint64_t flags = this->_lock();

    if (this->write_ptr == this->read_ptr){ 
        this->_unlock(flags);
        return nullptr;
    }

    task_t *task = this->queue[this->read_ptr];
    this->read_ptr = (this->read_ptr + 1) % this->queue_size;
    this->entry_count--;
    
    this->_unlock(flags);
    return task;
}

task_t *cpu_task_queue_t::steal(cpu_task_queue_t *victim){
    uint64_t victim_flags = victim->_lock();

    size_t amount_to_steal = AMOUNT_TO_STEAL(victim->entry_count);
    if (amount_to_steal == 0) {
        victim->_unlock(victim_flags);
        return nullptr;
    }
    
    task_t *staged_buffer[64];
    if (amount_to_steal > 64) amount_to_steal = 64;

    size_t actual_stolen = 0;
    for (size_t i = 0; i < amount_to_steal; i++){
        if (victim->write_ptr == victim->read_ptr) break;

        victim->write_ptr = (victim->write_ptr - 1 + victim->queue_size) % victim->queue_size;
        staged_buffer[actual_stolen] = victim->queue[victim->write_ptr];
        
        victim->entry_count--;
        actual_stolen++;
    }

    victim->_unlock(victim_flags);

    if (actual_stolen == 0) return nullptr;

    task_t *ret = staged_buffer[0];

    for (size_t i = 1; i < actual_stolen; i++){
        this->push(staged_buffer[i]);
    }

    return ret;
}

void cpu_task_queue_t::_expand_ring(bool lock_held){
    uint64_t flags = 0;
    if (!lock_held) flags = this->_lock();

    size_t new_size = AMOUNT_TO_EXPAND(this->queue_size);
    task_t **new_queue = (task_t **)malloc(sizeof(task_t *) * new_size);

    size_t wp = 0;
    while (this->read_ptr != this->write_ptr){
        new_queue[wp] = this->queue[this->read_ptr];
        this->read_ptr = (this->read_ptr + 1) % this->queue_size;
        wp++;
    }

    free(this->queue);
    this->queue = new_queue;
    this->queue_size = new_size;
    this->write_ptr = wp;
    this->read_ptr = 0;

    if (!lock_held) this->_unlock(flags);
}

uint64_t cpu_task_queue_t::_lock(){
    return spin_lock(&this->lock);
}

void cpu_task_queue_t::_unlock(uint64_t flags){
    spin_unlock(&this->lock, flags);
}
