#include <random.h>
#include <memory.h>
#include <cstr.h>
#include <filesystem/vfs/vfs.h>
uint32_t random();

int random_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    uint32_t total_itterations = length / sizeof(uint32_t);

    uint32_t *array = (uint32_t*)buffer;
    for (uint32_t i = 0; i < total_itterations; i++){
        array[i] = random();
    }

    return length;
}

int random_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    return length;
}

uint32_t state;

void rand_init(uint32_t seed) {
    if (seed == 0) seed = 1;  // seed cannot be zero
    state = seed;

    vnode_t *rand = vfs::create_path("/dev/random", VCHR);
    rand->permissions = 0666;
    rand->file_operations.write = random_write;
    rand->file_operations.read = random_read;

    vnode_t *urand = vfs::create_path("/dev/urandom", VCHR);
    urand->permissions = 0666;
    urand->file_operations.write = random_write;
    urand->file_operations.read = random_read;

    rand->close();
    urand->close();
}

// Not really random, it can be guessed

uint32_t random() {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

uint32_t random(uint32_t max){
    return random() % max;
}

char random_string_buffer[257];
char* random_string(uint8_t length){
    memset(random_string_buffer, 0, 257);
    
    for (int i = 0; i < length; i += 2){
        uint8_t rand = random() & 0xFF;
        while (rand == 0) rand = random() & 0xFF;
        strcat(random_string_buffer, toHString(rand));
    }
    random_string_buffer[length] = '\0';

    return random_string_buffer;
}