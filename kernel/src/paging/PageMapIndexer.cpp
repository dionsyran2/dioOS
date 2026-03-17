#include <paging/PageMapIndexer.h>

PageMapIndexer::PageMapIndexer(uint64_t virtualAddress){
    virtualAddress >>= 12;
    P_i = virtualAddress & 0x1ff;  // Page Table Index (Bits 12-20)
    virtualAddress >>= 9;
    L1_i = virtualAddress & 0x1ff; // Page Directory Index (Bits 21-29)
    virtualAddress >>= 9;
    L2_i = virtualAddress & 0x1ff; // Page Directory Pointer Table Index (Bits 30-38)
    virtualAddress >>= 9;
    L3_i = virtualAddress & 0x1ff; // PML4 Index (Bits 39-47)
}