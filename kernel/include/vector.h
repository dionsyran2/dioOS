#pragma once
#include <stddef.h>
#include <stdint.h>
#include <memory/heap.h>

namespace kstl{

template<typename T>
    class Vector {
    private:
        T* data = nullptr;
        size_t _size = 0;
        size_t _capacity = 0;

        void grow() {
            size_t newCapacity = _capacity == 0 ? 2 : _capacity * 2;
            T* newData = (T*)malloc(newCapacity * sizeof(T));
            memset(newData, 0, newCapacity * sizeof(T));
            if (!newData) return;

            if (data != nullptr) {memcpy(newData, data, _capacity * sizeof(T)); free(data);}
            data = newData;
            _capacity = newCapacity;
        }

    public:
        Vector() {
            data = nullptr;
            _capacity = 0;
        }
        ~Vector() {
            if (data) free(data);
        }

        void push_back(const T& value) {
            if (_size >= _capacity) grow();
            data[_size++] = value;
        }

        void pop_back() {
            if (_size > 0) _size--;
        }

        T& operator[](size_t index) {
            return data[index];
        }

        const T& operator[](size_t index) const {
            return data[index];
        }

        size_t size() const { return _size; }
        size_t capacity() const { return _capacity; }

        T* begin() { return data; }
        T* end() { return data + _size; }

        void clear() { _size = 0; }

        // Optional: resize to a specific size
        void resize(size_t newSize, const T& defaultValue = T()) {
            if (newSize > _capacity) {
                while (_capacity < newSize) grow();
            }
            for (size_t i = _size; i < newSize; i++) {
                data[i] = defaultValue;
            }
            _size = newSize;
        }
    };
}