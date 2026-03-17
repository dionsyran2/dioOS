#include <random.h>
#include <memory.h>
#include <cstr.h>

uint32_t state;

void rand_init(uint32_t seed) {
    if (seed == 0) seed = 1;  // seed cannot be zero
    state = seed;
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