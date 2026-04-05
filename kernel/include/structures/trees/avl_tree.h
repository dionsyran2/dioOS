/* AVL Tree (Its pretty efficient, O(logN) to be exact) */
#pragma once
#include <stdint.h>
#include <stddef.h>

#include <scheduling/spinlock/spinlock.h>
#include <structures/lists/linked_list.h>
#include <math.h>
#include <kstdio.h>

namespace kstd {
    template <typename T>
    class avl_node_t {
        public:
        uint64_t key;
        T data;
        
        private:
        int height = 1;
        avl_node_t *left_child = nullptr;
        avl_node_t *right_child = nullptr;

        public:
        avl_node_t<T> *get_left_child(){
            return this->left_child;
        }

        avl_node_t<T> *get_right_child(){
            return this->right_child;
        }

        int get_height(){
            return this->height;
        }

        void set_left_child(avl_node_t<T> *child){
            this->left_child = child;
        }

        void set_right_child(avl_node_t<T> *child){
            this->right_child = child;
        }

        void set_height(int value){
            this->height = value;
        }
    };

    template <typename T>
    class avl_tree_t {
        private:
        avl_node_t<T> *root = nullptr;
        spinlock_t tree_lock = 0;

        private:
        avl_node_t<T> *_insert_recursive(avl_node_t<T> *new_node, avl_node_t<T> *current){
            // Set it as the root
            if (current == nullptr){
                return new_node;
            }

            // Otherwise compare to find the right place
            if (new_node->key < current->key){ // If new_key < current_key, we go left
                avl_node_t<T> *child = current->get_left_child();
                avl_node_t<T> *result = _insert_recursive(new_node, child);

                current->set_left_child(result);
            }
            else if (new_node->key > current->key){ // If new_key > current_key, we go right
                avl_node_t<T> *child = current->get_right_child();
                avl_node_t<T> *result = _insert_recursive(new_node, child);

                current->set_right_child(result);
            } else { // If its equal we reject it... we can't have dublicates.
                delete new_node;
                return current;
            }

            _update_height(current);
            return _apply_rotation(current);
        }
        
        avl_node_t<T> *_remove_recursive(uint64_t key, avl_node_t<T> *current){
            if (current == nullptr)
            {
                return nullptr;
            }

            if (key < current->key){ // If key < current_key, we go left
                avl_node_t<T> *child = current->get_left_child();
                avl_node_t<T> *result = _remove_recursive(key, child);

                current->set_left_child(result);
            }
            else if (key > current->key){ // If key > current_key, we go right
                avl_node_t<T> *child = current->get_right_child();
                avl_node_t<T> *result = _remove_recursive(key, child);

                current->set_right_child(result);
            } else { // We found it!
                // One child or leaf
                if (current->get_left_child() == nullptr){
                    avl_node_t<T> *temp = current->get_right_child();
                    delete current;
                    return temp;
                } else if (current->get_right_child() == nullptr){
                    avl_node_t<T> *temp = current->get_left_child();
                    delete current;
                    return temp;
                }

                // both left & right Children
                avl_node_t<T> *min = _get_min(current->get_right_child());

                // Copy its metadata
                current->data = min->data;
                current->key = min->key;

                // Call delete again
                current->set_right_child(_remove_recursive(min->key, current->get_right_child()));
            }

            _update_height(current);
            return _apply_rotation(current);
        }

        avl_node_t<T> *_get_max(avl_node_t<T> *node){
            if (node->get_right_child()){
                return _get_max(node->get_right_child());
            }

            return node;
        }

        avl_node_t<T> *_get_min(avl_node_t<T> *node){
            if (node->get_left_child()){
                return _get_min(node->get_left_child());
            }

            return node;
        }

        int _height(avl_node_t<T> *node){
            return node != nullptr ? node->get_height() : 0;
        }

        void _update_height(avl_node_t<T> *node){
            int max_height = max(_height(node->get_left_child()), _height(node->get_right_child()));
            node->set_height(max_height + 1);
        }

        int _balance(avl_node_t<T> *node){
            return node != nullptr ? _height(node->get_left_child()) - _height(node->get_right_child()) : 0;
        }


        avl_node_t<T> *_apply_rotation(avl_node_t<T> *node){
            int balance = _balance(node);

            if (balance > 1){
                // Left-heavy
                if (_balance(node->get_left_child()) < 0){
                    node->set_left_child(_rotate_left(node->get_left_child()));
                }

                return _rotate_right(node);
            }

            if (balance < -1){
                // Right-heavy
                if (_balance(node->get_right_child()) > 0){
                    node->set_right_child(_rotate_right(node->get_right_child()));
                }

                return _rotate_left(node);
            }
            return node;
        }
        
        avl_node_t<T> *_rotate_right(avl_node_t<T> *node){
            avl_node_t<T> *left_node = node->get_left_child();
            avl_node_t<T> *center_node = left_node->get_right_child();

            left_node->set_right_child(node);
            node->set_left_child(center_node);

            _update_height(node);
            _update_height(left_node);

            return left_node;
        }
        
        avl_node_t<T> *_rotate_left(avl_node_t<T> *node){
            avl_node_t<T> *right_node = node->get_right_child();
            avl_node_t<T> *center_node = right_node->get_left_child();

            right_node->set_left_child(node);
            node->set_right_child(center_node);

            _update_height(node);
            _update_height(right_node);

            return right_node;
        }

        void _deconstructor_recursive(avl_node_t<T> *node) {
            // Used to free any objects allocated by the tree during its deconstruction

            if (node == nullptr)
                return;

            _deconstructor_recursive(node->get_right_child());
            _deconstructor_recursive(node->get_left_child());

            delete node;
        }

        void _range_search_recursive(uint64_t min, uint64_t max, avl_node_t<T> *node, linked_list_t<uint64_t> *list){
            // If node is null, return
            if (node == nullptr) return;
            
            // Check if there can be children within our range in the left branch
            if (node->key > min) {
                this->_range_search_recursive(min, max, node->get_left_child(), list);
            }
            
            // Check this node
            if (node->key >= min && node->key <= max) {
                list->add(node->key);
            }
            
            // Check if there can be children within our range in the right branch
            if (node->key < max) {
                this->_range_search_recursive(min, max, node->get_right_child(), list);
            }
        }

        public:
        void insert(uint64_t key, T data){
            if (this->search(key))
                return; // It already exists

            avl_node_t<T> *new_node = new avl_node_t<T>();

            new_node->key = key;

            new_node->data = data;

            uint64_t rflags = spin_lock(&this->tree_lock);

            this->root = _insert_recursive(new_node, this->root);

            spin_unlock(&this->tree_lock, rflags);
        }

        void remove(uint64_t key){
            uint64_t rflags = spin_lock(&this->tree_lock);

            this->root = _remove_recursive(key, this->root);

            spin_unlock(&this->tree_lock, rflags);
        }
        
        T search(uint64_t key){
            uint64_t rflags = spin_lock(&this->tree_lock);

            avl_node_t<T> *focus = this->root;

            if (focus == nullptr){
                spin_unlock(&this->tree_lock, rflags);
                return nullptr; // The tree is empty
            }

            while (true) {

                if (key == focus->key) {
                    spin_unlock(&this->tree_lock, rflags);
                    return focus->data; // We found it
                }

                if (key < focus->key) {
                    // It should be on the left.
                    // If there is no left child, its not in the tree
                    if (focus->get_left_child() == nullptr) {
                        spin_unlock(&this->tree_lock, rflags);
                        return nullptr;
                    }
                    focus = focus->get_left_child();
                } else {
                    // Same goes for the right child
                    if (focus->get_right_child() == nullptr){
                        spin_unlock(&this->tree_lock, rflags);
                        return nullptr;
                    }

                    focus = focus->get_right_child();
                }
            }
        }

        linked_list_t<uint64_t> *range_search(uint64_t min, uint64_t max){
            linked_list_t<uint64_t> *list = new linked_list_t<uint64_t>();
            _range_search_recursive(min, max, this->root, list);

            return list;
        }

        T floor_search(uint64_t target_key) {
            uint64_t rflags = spin_lock(&this->tree_lock);
            avl_node_t<T>* focus = this->root;
            avl_node_t<T>* best_match = nullptr;
            
            while (focus != nullptr) {
                if (target_key == focus->key) {
                    best_match = focus;
                    break; 
                }
                if (target_key < focus->key) {
                    focus = focus->get_left_child();
                } else {
                    best_match = focus;
                    focus = focus->get_right_child();
                }
            }
            spin_unlock(&this->tree_lock, rflags);
            return best_match ? best_match->data : nullptr;
        }

        avl_tree_t(){

        }

        ~avl_tree_t() {
            // Recursively deconstruct
            _deconstructor_recursive(this->root);
        }
    };
}