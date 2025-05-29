#pragma once
#include <stdint.h>
#include <stddef.h>
#include "memory.h"
#include "memory/heap.h"

template<typename T>
class ArrayList{
    private:
    T* data;
    size_t size;
    size_t length_val;

    void Resize(size_t newSize){
        T* new_data = new T[newSize];
        for (size_t i = 0; i < length_val; i++) {
            new_data[i] = data[i];
        }
        delete[] data;
        data = new_data;
        size = newSize;
    }

    public:
    ArrayList(){
        size = 4;
        length_val = 0;
        data = new T[size];
    }

    ~ArrayList() {
        delete[] data;
    }

    void add(T new_data){
        if (size == length_val) Resize(size + 4);

        data[length_val] = new_data;
        length_val++;
    }

    T get(size_t index){
        if (index >= length_val) return NULL;
        return data[index];
    }

    void remove(T dat){
        int index = -1;

        for (size_t i = 0; i < length_val; i++){
            if (dat == get(i)){
                index = i;
                break;
            }
        }

        for (size_t i = index; i < length_val - 1; i++){
            data[i] = data[i + 1];
        }

        length_val--;
        data[length_val] = T();
    }

    size_t length(){
        return length_val;
    }
};