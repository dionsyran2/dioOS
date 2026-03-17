#include <paging/PageEntry.h>

#define FLAG_MASK 0xfff0000000000fff
#define ADDR_MASK 0xffffffffff000

bool PageEntry::is_flag_set(PT_Flag flag){
    bool ret = this->value & (1UL << flag);
    return ret;
};

void PageEntry::set_flag(PT_Flag flag, bool status){
    if (status){
        this->value |= (1ULL << flag);
    }else{
        this->value &= ~(1ULL << flag);
    }
}

uint64_t PageEntry::get_address(){
    uint64_t ret = this->value & ADDR_MASK;
    return ret;
}

void PageEntry::set_address(uint64_t address){
    this->value &= FLAG_MASK;
    this->value |= (address & ADDR_MASK);
}