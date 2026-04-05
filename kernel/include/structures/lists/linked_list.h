/* A simple list */
#pragma once
#include <stdint.h>
#include <stddef.h>

#include <scheduling/spinlock/spinlock.h>

namespace kstd{
    template <typename T>
    class _linked_list_entry_t{
        public:
        void set_data(T data){
            this->data = data;
        }

        T get_data(){
            return this->data;
        }

        _linked_list_entry_t<T> *get_next(){
            return this->next;
        }

        _linked_list_entry_t<T> *get_previous(){
            return this->previous;
        }

        void set_previous(_linked_list_entry_t<T> *node){
            this->previous = node;
        }

        void set_next(_linked_list_entry_t<T> *node){
            this->next = node;
        }

        private:
        T data;

        _linked_list_entry_t<T> *previous = nullptr;
        _linked_list_entry_t<T> *next = nullptr;
    };


    template <typename T>
    class linked_list_t{
        public:
        // For multithreaded operations
        void lock(){
            this->rflags = spin_lock(&this->spinlock);
        }
        
        void unlock(){
            spin_unlock(&this->spinlock, this->rflags);
        }

        void add(T data){
            _linked_list_entry_t<T> *entry = new _linked_list_entry_t<T>();
            entry->set_data(data);

            if (this->root == nullptr){
                this->root = entry;
            } else {
                this->last_node->set_next(entry);
                entry->set_previous(this->last_node);
            }

            this->last_node = entry;

            this->length++;
        }

        void remove(int index){
            _linked_list_entry_t<T> *target = _get_entry(index);

            if (!target) return;

            _linked_list_entry_t<T> *next = target->get_next();
            _linked_list_entry_t<T> *previous = target->get_previous();

            if (this->root == target){
                this->root = next;
            }

            if (this->last_node == target){
                this->last_node = previous;
            }

            if (next){
                next->set_previous(previous);
            }

            if (previous){
                previous->set_next(next);
            }

            this->length--;

            delete target;
        }

        T get(int index){
            _linked_list_entry_t<T> *target = _get_entry(index);

            if (!target) return T();

            return target->get_data();
        }

        int size(){
            return this->length;
        }

        linked_list_t(){

        }

        ~linked_list_t(){
            _linked_list_entry_t<T> *current = this->root;

            while(current){
                _linked_list_entry_t<T> *next = current->get_next();

                delete current;
                current = next;
            }
        }

        private:
        _linked_list_entry_t<T> *_get_entry(int index){
            _linked_list_entry_t<T> *current = this->root;

            for (int i = 0; i < index; i++){
                if (!current) return nullptr;

                current = current->get_next();
            }

            return current;
        }

        private:
        int length = 0;
        spinlock_t spinlock = 0;
        uint64_t rflags;

        _linked_list_entry_t<T> *root = nullptr;
        _linked_list_entry_t<T> *last_node = nullptr;
    };
}