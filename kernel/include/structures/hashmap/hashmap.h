#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory/heap.h>
#include <memory.h>
#include <scheduling/spinlock/spinlock.h>
#include <cstr.h>

namespace kstd {

    // Default Hash Functor (supports pointers, integers, and objects castable to uint64_t)
    template <typename T>
    struct hash {
        uint64_t operator()(const T& key) const {
            uint64_t x = (uint64_t)key;
            // Thomas Wang's 64-bit integer hash to prevent clustering
            x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
            x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
            x = x ^ (x >> 31);
            return x;
        }
    };

    // FNV-1a Hash Specialization for Null-Terminated C-Strings (Critical for VFS Paths)
    template <>
    struct hash<const char*> {
        uint64_t operator()(const char* str) const {
            uint64_t hash = 0xcbf29ce484222325;
            while (*str) {
                hash ^= (uint8_t)*str++;
                hash *= 0x00000100000001B3;
            }
            return hash;
        }
    };

    // Helper equality comparator
    template <typename T>
    struct equals {
        bool operator()(const T& a, const T& b) const {
            return a == b;
        }
    };

    // Specialization for C-Strings
    template <>
    struct equals<const char*> {
        bool operator()(const char* a, const char* b) const {
            return strcmp(a, b) == 0;
        }
    };

    template <typename K, typename V, typename Hash = hash<K>, typename KeyEqual = equals<K>>
    class hashmap {
    private:
        enum EntryState : uint8_t { EMPTY, IN_USE, DELETED };

        struct hash_entry_t {
            K key;
            V value;
            EntryState state;
        };

        hash_entry_t* table;
        size_t capacity;
        size_t active_entries;
        
        spinlock_t lock;
        Hash hash_fn;
        KeyEqual equal_fn;

        // Dynamic resizing logic
        void resize(size_t new_capacity) {
            hash_entry_t* old_table = this->table;
            size_t old_capacity = this->capacity;

            // Allocate a clean, larger bucket ring
            this->table = (hash_entry_t*)malloc(sizeof(hash_entry_t) * new_capacity);
            this->capacity = new_capacity;
            this->active_entries = 0;

            for (size_t i = 0; i < new_capacity; i++) {
                this->table[i].state = EMPTY;
            }

            // Rehash all existing elements into the new array
            if (old_table) {
                for (size_t i = 0; i < old_capacity; i++) {
                    if (old_table[i].state == IN_USE) {
                        // Insert directly bypassing size checks
                        insert_internal(old_table[i].key, old_table[i].value);
                    }
                }
                free(old_table); // Reclaim the old allocation
            }
        }

        // Unlocked insertion helper used during rehashing
        void insert_internal(const K& key, const V& value) {
            uint64_t index = hash_fn(key) % this->capacity;

            for (size_t i = 0; i < this->capacity; i++) {
                size_t probe = (index + i) % this->capacity;

                // We can reuse EMPTY or DELETED (tombstone) slots
                if (this->table[probe].state == EMPTY || this->table[probe].state == DELETED) {
                    this->table[probe].key = key;
                    this->table[probe].value = value;
                    this->table[probe].state = IN_USE;
                    this->active_entries++;
                    return;
                }

                // If the key already exists, overwrite the value
                if (this->table[probe].state == IN_USE && equal_fn(this->table[probe].key, key)) {
                    this->table[probe].value = value;
                    return;
                }
            }
        }

    public:
        hashmap(size_t initial_capacity = 32) {
            this->lock = 0;
            this->capacity = initial_capacity;
            this->active_entries = 0;
            this->table = nullptr;
            
            resize(initial_capacity);
        }

        ~hashmap() {
            if (this->table) {
                free(this->table);
            }
        }

        bool insert(const K& key, const V& value) {
            uint64_t flags = spin_lock(&this->lock);

            // Double the capacity if our load factor exceeds 70%
            if ((this->active_entries * 10) / this->capacity >= 7) {
                resize(this->capacity * 2);
            }

            insert_internal(key, value);

            spin_unlock(&this->lock, flags);
            return true;
        }

        bool search(const K& key, V& out_value) {
            uint64_t flags = spin_lock(&this->lock);
            uint64_t index = hash_fn(key) % this->capacity;

            for (size_t i = 0; i < this->capacity; i++) {
                size_t probe = (index + i) % this->capacity;

                if (this->table[probe].state == IN_USE && equal_fn(this->table[probe].key, key)) {
                    out_value = this->table[probe].value;
                    spin_unlock(&this->lock, flags);
                    return true;
                }

                // An EMPTY state guarantees the search is over (linear probing invariant)
                if (this->table[probe].state == EMPTY) {
                    break;
                }
            }

            spin_unlock(&this->lock, flags);
            return false;
        }

        bool remove(const K& key) {
            uint64_t flags = spin_lock(&this->lock);
            uint64_t index = hash_fn(key) % this->capacity;

            for (size_t i = 0; i < this->capacity; i++) {
                size_t probe = (index + i) % this->capacity;

                if (this->table[probe].state == IN_USE && equal_fn(this->table[probe].key, key)) {
                    // Mark as DELETED (Tombstone) so we don't break lookup probe chains
                    this->table[probe].state = DELETED;
                    this->active_entries--;
                    spin_unlock(&this->lock, flags);
                    return true;
                }

                if (this->table[probe].state == EMPTY) {
                    break;
                }
            }

            spin_unlock(&this->lock, flags);
            return false;
        }

        size_t size() const {
            return this->active_entries;
        }
    };
}