#pragma once
#include <stdint.h>
#include <stddef.h>
#include <rendering/vt.h>
#include <drivers/graphics/common.h>

class vt_multiplexer{
    public:
    vt_multiplexer(drivers::GraphicsDriver *driver, uint8_t amount);
    void select_vt(uint8_t id);
    virtual_terminal *get_active();

    void deactivate_all();
    
    private:
    virtual_terminal **virtual_terminals;
    virtual_terminal *active_vt;
    uint8_t vt_count;
};

extern vt_multiplexer *global_multiplexer;