#pragma once
#include <stdint.h>

class PageMapIndexer {
    public:
        PageMapIndexer(uint64_t virtualAddress);
        uint64_t L3_i;
        uint64_t L2_i;
        uint64_t L1_i;
        uint64_t P_i;
};