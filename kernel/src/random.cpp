#include <random.h>

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