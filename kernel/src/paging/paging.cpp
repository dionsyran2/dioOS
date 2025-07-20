#include <paging/paging.h>

void PageDirectoryEntry::SetFlag(PT_Flag flag, bool enabled){
    uint64_t bitSelector = (uint64_t)1 << flag;
    Value &= ~bitSelector;
    if (enabled){
        Value |= bitSelector;
    }
}

bool PageDirectoryEntry::GetFlag(PT_Flag flag){
    uint64_t bitSelector = (uint64_t)1 << flag;
    return (Value & bitSelector) != 0;
}

uint64_t PageDirectoryEntry::GetAddress(){
    return (Value & 0x000FFFFFFFFFF000);
}

void PageDirectoryEntry::SetAddress(uint64_t address){
    Value &= 0xfff0000000000fff;
    Value |= (address & 0x000FFFFFFFFFF000);
}