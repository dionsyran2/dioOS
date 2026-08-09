#include <rendering/multiplexer.h>
#include <line_discipline/line_discipline.h>
#include <cstr.h>


vt_multiplexer *global_multiplexer = nullptr;

vt_multiplexer::vt_multiplexer(drivers::GraphicsDriver *driver, uint8_t amount){
    this->virtual_terminals = (virtual_terminal**)malloc(sizeof(virtual_terminal) * amount);
    this->vt_count = amount;
    for (uint8_t i = 0; i < amount; i++){
        // Create the VT
        this->virtual_terminals[i] = new virtual_terminal(driver);

        // Start with it deactivated
        this->virtual_terminals[i]->deactivate();

        // Format the identifier string
        char buffer[24];
        stringf(buffer, sizeof(buffer), "TTY %d\n\r", i);

        // Print the identifier
        this->virtual_terminals[i]->write((char*)buffer, strlen(buffer), false);

        // Create a devfs node for it
        stringf(buffer, sizeof(buffer), "/tty%d", i);

        line_discipline::register_vt(this->virtual_terminals[i], buffer);
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