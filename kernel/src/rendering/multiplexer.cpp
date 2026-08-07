#include <rendering/multiplexer.h>
#include <cstr.h>

vt_multiplexer *global_multiplexer = nullptr;

vt_multiplexer::vt_multiplexer(drivers::GraphicsDriver *driver, uint8_t amount){
    this->virtual_terminals = (virtual_terminal**)malloc(sizeof(virtual_terminal) * amount);
    this->vt_count = amount;
    for (uint8_t i = 0; i < amount; i++){
        this->virtual_terminals[i] = new virtual_terminal(driver);
        this->virtual_terminals[i]->deactivate();

        this->virtual_terminals[i]->write("TTY ", 4);
        const char *number = toString(i);

        this->virtual_terminals[i]->write((char*)number, strlen(number));
        this->virtual_terminals[i]->write("\n\r", 2);
    }
}

void vt_multiplexer::select_vt(uint8_t index){
    if (index >= this->vt_count) return;

    if (this->active_vt) this->active_vt->deactivate();
    this->active_vt = this->virtual_terminals[index];
    this->active_vt->activate();
    this->active_vt->refresh();
}

void vt_multiplexer::deactivate_all(){
    if (this->active_vt) this->active_vt->deactivate();
    this->active_vt = nullptr;
}

virtual_terminal *vt_multiplexer::get_active(){
    return this->active_vt;
}