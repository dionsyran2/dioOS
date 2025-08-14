#pragma once
#include <stdint.h>
#include <stddef.h>


void rand_init(uint32_t seed);

uint32_t random();

// Generate a random number in [0, max)
uint32_t random(uint32_t max);

char* random_string(uint8_t length);