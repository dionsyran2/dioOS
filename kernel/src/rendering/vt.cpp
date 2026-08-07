/* Virtual Terminal Code */
#include <rendering/vt.h>
#include <memory/heap.h>
#include <cstr.h>
#include <rendering/psf.h>
#include <drivers/audio/pc_speaker/pc_speaker.h>
#include <math.h>
#include <scheduling/task_scheduler/task_scheduler.h>

void vt_blinker(virtual_terminal *term){
    task_t *self = task_scheduler::get_current_task();
    while (1){
        self->block(500, nullptr);

        if (!term->get_state() || term->cursor_disabled) continue;

        if (term->cursor_state){
            term->draw_cursor(true);
        } else {
            term->draw_cursor(false);
        }
    }
}

virtual_terminal::virtual_terminal(drivers::GraphicsDriver *driver){
    this->driver = driver;

    this->width = driver->width / VT_CELL_WIDTH;
    this->height = driver->height / VT_CELL_HEIGHT;

    size_t total_size = this->width * this->height * sizeof(__vt_cell);
    this->cell_table = (__vt_cell*)malloc(total_size);
    memset(this->cell_table, 0, total_size);

    task_t *blinker = task_scheduler::create_process("vt-blinker", (function)vt_blinker, false);
    blinker->registers.rdi = (uint64_t)this;

    task_scheduler::mark_as_ready(blinker);
}

bool virtual_terminal::get_state(){
    return this->active;
}

void virtual_terminal::activate(){
    this->active = true;
}

void virtual_terminal::deactivate(){
    this->active = false;
}

void virtual_terminal::refresh(){
    if (!this->active) return;

    for (size_t y = 0; y < this->height; y++){
        for (size_t x = 0; x < this->width; x++){
            this->render_cell(x, y);
        }
    }

    //this->driver->Update(); render_cell() already writes it directly to the fb
    
    this->dirty_min_x = 0;
    this->dirty_max_x = 0;
    this->dirty_min_y = 0;
    this->dirty_max_y = 0;
}

void virtual_terminal::render(){
    if (!this->active) return;

    // this->driver->Update(this->dirty_min_x, this->dirty_min_y, this->dirty_max_x - this->dirty_min_x, this->dirty_max_y - this->dirty_min_y);
    
    this->dirty_min_x = 0;
    this->dirty_max_x = 0;
    this->dirty_min_y = 0;
    this->dirty_max_y = 0;
}

void virtual_terminal::set_cell(size_t x, size_t y, wchar_t chr, uint32_t attributes, uint32_t fg, uint32_t bg){
    if (x >= width || y >= height) return;

    // Get the cell
    __vt_cell* cell = &this->cell_table[y * width + x];

    *cell = {chr, attributes, fg, bg};
}

__vt_cell virtual_terminal::get_cell(size_t x, size_t y){
    if (x >= width || y >= height) return __vt_cell();

    return this->cell_table[y * width + x];
}

void virtual_terminal::render_cell(size_t x, size_t y){
    // Sanity check
    if (x >= width || y >= height) return;

    // Get the cell
    __vt_cell* cell = &this->cell_table[y * width + x];

    // Print it
    uint32_t bg = cell->attributes & VT_INVERSE ? cell->fg : cell->bg;
    uint32_t fg = cell->attributes & VT_INVERSE ? cell->bg : cell->fg;

    draw_char(fg, bg, cell->chr, x * VT_CELL_WIDTH, y * VT_CELL_HEIGHT, cell->attributes & VT_BOLD, cell->attributes & VT_UNDERLINE, driver);
    mark_dirty(x * VT_CELL_WIDTH, y * VT_CELL_HEIGHT, VT_CELL_WIDTH, VT_CELL_HEIGHT);
};

void virtual_terminal::draw_cursor(bool clear){
    if (this->cursor_state && !clear) this->draw_cursor(true); // Clear the cursor before redrawing

    if (clear){
        // Clear the cursor
        if (!this->cursor_state) return;

        this->set_cell(
            this->cursor_rendered_x,
            this->cursor_rendered_y,
            this->previous_cursor_cell_state.chr,
            this->previous_cursor_cell_state.attributes,
            this->previous_cursor_cell_state.fg,
            this->previous_cursor_cell_state.bg
        );

        this->render_cell(this->cursor_rendered_x, this->cursor_rendered_y);
        this->cursor_state = false;
    } else {
        this->previous_cursor_cell_state = get_cell(this->cursor_x, this->cursor_y);

        this->set_cell(this->cursor_x, this->cursor_y, ' ', 0, 0, 0xFFFFFF);
        this->cursor_rendered_x = this->cursor_x;
        this->cursor_rendered_y = this->cursor_y;

        this->cursor_state = true;
        this->render_cell(this->cursor_rendered_x, this->cursor_rendered_y);
    }
}

/* @brief Scrolls the terminal up by 1 row */
void virtual_terminal::scroll(){
    if (driver == nullptr) return;

    // Move height up
    memmove(this->cell_table, 
            &this->cell_table[1 * width], 
            sizeof(__vt_cell) * width * (height - 1));

    // Clear the LAST row (height - 1)
    memset(&this->cell_table[(height - 1) * width], 
           0, 
           sizeof(__vt_cell) * width);

    driver->Scroll(VT_CELL_HEIGHT);
    driver->Update();
    //mark_dirty(0, 0, this->width * VT_CELL_WIDTH, this->height * VT_CELL_HEIGHT);
}

void virtual_terminal::mark_dirty(size_t x, size_t y, size_t width, size_t height){
    this->dirty_min_x = min(this->dirty_min_x, x);
    this->dirty_min_y = min(this->dirty_min_y, y);
    this->dirty_max_x = max(this->dirty_max_x, x + width);
    this->dirty_max_y = max(this->dirty_max_y, y + height);
}

void virtual_terminal::clear(){
    this->clear(0, 0, this->width, this->height);
}

void virtual_terminal::clear(size_t start_x, size_t start_y, size_t end_x, size_t end_y){
    // Sanity check
    if (driver == nullptr) return;

    // Clear the array
    for (uint16_t y = start_y; y <= end_y; y++){
        for (uint16_t x = start_x; x <= end_x; x++){
            set_cell(x, y, ' ', current_attributes, current_fg, current_bg);
        }
    }

    // Clear the screen
    driver->DrawRectangle(start_x * VT_CELL_WIDTH, start_y * VT_CELL_HEIGHT, (end_x - start_x) * VT_CELL_WIDTH, (end_y - start_y) * VT_CELL_HEIGHT, current_bg);
    
    uint16_t px_x = start_x * VT_CELL_WIDTH;
    uint16_t px_y = start_y * VT_CELL_HEIGHT;
    uint16_t px_w = (end_x - start_x + 1) * VT_CELL_WIDTH;
    uint16_t px_h = (end_y - start_y + 1) * VT_CELL_HEIGHT;
    
    mark_dirty(px_x, px_y, px_w, px_h);
}

void virtual_terminal::write(wchar_t chr){
    /* Handle special cases */
    switch (chr){
        case '\a':
            beep();
            return;

        case '\b':
            if (this->cursor_x > 0){
                this->cursor_x--;
            }
            return;

        case '\f':
            clear();
            this->cursor_x = this->cursor_y = 0;
            return;

        case '\n':
            this->cursor_y++;

            if (this->cursor_y >= height){
                this->scroll();
                this->cursor_y = height - 1;
            }
            return;

        case '\r':
            this->cursor_x = 0;
            return;

        case '\t': {
            uint32_t tab_size = 8;
            uint32_t next_tab = ((this->cursor_x / tab_size) + 1) * tab_size;
            if (next_tab >= width)
                write('\n');
            else
                this->cursor_x = next_tab;
            return;
        }

        case '\v':
            write('\n');
            return;
        case '\x0e': // Shift out
            return;
        case '\x0f': //Shift in
            return;
    }
    
    /* Print */
    
    // Drop a line if necessary
    if (this->cursor_x >= width) {
        this->cursor_x = 0;
        this->cursor_y++;
    }

    // Scroll if necessary
    if (this->cursor_y >= height){
        scroll();
        this->cursor_y = height - 1;
    }

    // Otherwise print the character
    set_cell(this->cursor_x, this->cursor_y, chr, current_attributes, current_fg, current_bg);
    render_cell(this->cursor_x, this->cursor_y);

    // Increase the offset
    this->cursor_x++;
}

void virtual_terminal::write(char *str, size_t length, bool return_on_newline){
    this->draw_cursor(true);
    
    for (uint32_t i = 0; i < length; i++) {
        char c = str[i];

        switch (this->current_state) {
            case VT_STATE_NORMAL:
                /* Normal character... check if its some weird utf8 character */
                if (c == '\033') {
                    this->current_state = VT_STATE_ESC;
                } else {
                    uint32_t codepoint;
                    size_t bytes = decode_utf8(&str[i], &codepoint);
                    if (bytes == 0) {
                        write(L'\ufffd');
                        continue;
                    }

                    if (return_on_newline && codepoint == '\n'){
                        write('\r');
                    }

                    write(codepoint);
                    i += bytes - 1; // Here we subtract size-1, since the for loop will increment it later on.
                }
                break;

            case VT_STATE_ESC:
                /* Parse the escape sequence */
                if (c == '[') {
                    this->current_state = VT_STATE_CSI;
                    memset(csi_params, 0, sizeof(csi_params));
                    csi_param_count = 0;
                    private_mode = false;
                } /*else if (c == 'M') { // Reverse Index
                    if (offset_y > 0) offset_y--;
                    state = VT_STATE_NORMAL;
                } else if (c == '7') { // Save Cursor
                    saved_offset[0] = offset_x;
                    saved_offset[1] = offset_y;
                    state = VT_STATE_NORMAL;
                } else if (c == '8') { // Restore Cursor
                    offset_x = saved_offset[0];
                    offset_y = saved_offset[1];
                    state = VT_STATE_NORMAL;
                    this->draw_cursor();
                }*/ else {
                    // Unknown ESC sequence, fall back to normal
                    this->current_state = VT_STATE_NORMAL;
                }
                break;

            case VT_STATE_CSI:
                if (c == '?') {
                    private_mode = true;
                } else if (isdigit(c)) {
                    // Found a digit, start parsing parameters
                    this->current_state = VT_STATE_CSI_PARAM;
                    csi_params[csi_param_count] = c - '0';
                } else if (c == ';') {
                    // Empty parameter before semicolon?
                    csi_param_count++; 
                } else {
                    // Its a command character
                    handle_csi_command(c);
                    this->current_state = VT_STATE_NORMAL;
                }
                break;

            case VT_STATE_CSI_PARAM:
                if (isdigit(c)) {
                    csi_params[csi_param_count] = (csi_params[csi_param_count] * 10) + (c - '0');
                } else if (c == ';') {
                    csi_param_count++;
                    if (csi_param_count >= 16) csi_param_count = 15; // Clamp
                    csi_params[csi_param_count] = 0; // Prepare next
                } else {
                    // Command character finished the sequence
                    csi_param_count++; // Count the last parameter we just finished
                    handle_csi_command(c);
                    this->current_state = VT_STATE_NORMAL;
                }
                break;
        }
    }

    this->draw_cursor();
}



/* Escape Sequence Handlers */

void virtual_terminal::handle_csi_command(char command) {
    // Default parameter to 1 if missing
    int arg0 = (csi_param_count > 0) ? csi_params[0] : 1; 
    
    // Handle Private Mode Sequences (e.g. ?25h)
    if (this->private_mode) {
        switch (command) {
            case 'h': // SET Mode
                if (arg0 == 25) this->cursor_disabled = false;
                break;
            case 'l': // RESET Mode
                if (arg0 == 25) this->cursor_disabled = true;
                break;
        }
        return;
    }

    // Standard CSI
    switch (command) {
        case 'A': // Cursor Up
            this->cursor_y = max(0, (int)this->cursor_y - arg0);
            this->draw_cursor();
            break;
        case 'B': // Cursor Down
            this->cursor_y = min((int)this->height - 1, (int)this->cursor_y + arg0);
            draw_cursor();
            break;
        case 'C': // Cursor Forward
            this->cursor_x = min((int)this->width - 1, (int)this->cursor_x + arg0);
            draw_cursor();
            break;
        case 'D': // Cursor Back
            this->cursor_x = max(0, (int)this->cursor_x - arg0);
            draw_cursor();
            break;
        case 'H': // Cursor Position
        case 'f':
            {
                int row = (csi_param_count > 0 && csi_params[0] > 0) ? csi_params[0] - 1 : 0;
                int col = (csi_param_count > 1 && csi_params[1] > 0) ? csi_params[1] - 1 : 0;
                this->cursor_y = clamp(row, 0, height - 1);
                this->cursor_x = clamp(col, 0, width - 1);
            }
            break;
        case 'J': // Erase in Display
            arg0 = (csi_param_count > 0) ? csi_params[0] : 0; 
            if (arg0 == 2) this->clear();
            else if (arg0 == 1) this->clear(0, 0, this->cursor_x, this->cursor_y);
            else this->clear(this->cursor_x, this->cursor_y, width, height); // Arg 0
            break;
        case 'K': // Erase in Line
            arg0 = (csi_param_count > 0) ? csi_params[0] : 0;
            if (arg0 == 2) {
                // Clear entire line
                this->clear(0, this->cursor_y, width - 1, this->cursor_y);
            } else if (arg0 == 1) {
                // Clear from beginning to cursor
                this->clear(0, this->cursor_y, this->cursor_x, this->cursor_y);
            } else {
                // Clear from cursor to end of line (Default)
                this->clear(this->cursor_x, this->cursor_y, width - 1, this->cursor_y);
            }
            break;
        case 'm': // SGR
            if (csi_param_count == 0) {
                int array[1];
                array[0] = 0;
                this->handle_sgr(array, 1);
            } else {
                this->handle_sgr(csi_params, csi_param_count);
            }
            break;

        case '@': { // ICH - Insert Character(s)
            int num = arg0;
            if (num > width - this->cursor_x) num = width - this->cursor_x;
            int remaining = width - this->cursor_x - num;
            if (remaining > 0) {
                // Shift the rest of the line to the right
                memmove(&cell_table[this->cursor_y * width + this->cursor_x + num],
                        &cell_table[this->cursor_y * width + this->cursor_x],
                        remaining * sizeof(__vt_cell));
            }
            // Clear the newly opened space
            for (int i = 0; i < num && (this->cursor_x + i) < width; i++) {
                this->set_cell(this->cursor_x + i, this->cursor_y, ' ', current_attributes, current_fg, current_bg);
            }


            for (int x = this->cursor_x; x < width; x++) {
                render_cell(x, this->cursor_y);
            }

            // Redraw the whole line
            mark_dirty(0, this->cursor_y * VT_CELL_HEIGHT, width * VT_CELL_WIDTH, VT_CELL_HEIGHT);
            break;
        }
        case 'P': { // DCH - Delete Character(s)
            int num = arg0;
            if (num > width - this->cursor_x) num = width - this->cursor_x;
            int remaining = width - this->cursor_x - num;
            if (remaining > 0) {
                // Shift the rest of the line left (pulling it backwards)
                memmove(&cell_table[this->cursor_y * width + this->cursor_x],
                        &cell_table[this->cursor_y * width + this->cursor_x + num],
                        remaining * sizeof(__vt_cell));
            }

            for (int i = 0; i < num && (width - 1 - i) >= this->cursor_x; i++) {
                set_cell(width - 1 - i, this->cursor_y, ' ', current_attributes, current_fg, current_bg);
            }

            for (int x = this->cursor_x; x < width; x++) {
                render_cell(x, this->cursor_y);
            }

            mark_dirty(0, this->cursor_y * VT_CELL_HEIGHT, width * VT_CELL_WIDTH, VT_CELL_HEIGHT);
            break;
        }
    }
}

uint32_t get_ansi_256_color(int index) {
    // Standard Colors (0-15)
    
    uint32_t standard_colors[] = {
        0x0C0C0C, 0xC50F1F, 0x13A10E, 0xC19C00, 
        0x0037DA, 0x881798, 0x3A96DD, 0xCCCCCC,

        // Intense
        0x767676, 0xE74856, 0x16C60C, 0xF9F1A5, 
        0x3B78FF, 0xB4009E, 0x61D6D6, 0xF2F2F2 
    };
    if (index < 16) return standard_colors[index];

    // Grayscale Ramp (232-255)
    if (index >= 232) {
        int gray = (index - 232) * 10 + 8; // scaling to 0-255 roughly
        return (gray << 16) | (gray << 8) | gray;
    }

    // 6x6x6 Color Cube (16-231)
    index -= 16;
    int r = (index / 36) * 51; // 0..5 -> 0..255
    int g = ((index / 6) % 6) * 51;
    int b = (index % 6) * 51;

    // Adjust 0 to remain 0, but other steps might need slight tweaking
    
    return (r << 16) | (g << 8) | b;
}

//  Select Graphic Rendition
void virtual_terminal::handle_sgr(int *parameter_list, int parameter_count){
    for (int j = 0; j < parameter_count; j++) {
        int parameter = parameter_list[j];

        switch (parameter) {
            case 0:
                current_fg = 0xFFFFFF;
                current_bg = 0x000000;
                current_attributes = 0;
                break;
            
            case 1: /*current_attributes |= VT_BOLD;*/ break;
            case 4: current_attributes |= VT_UNDERLINE; break;
            case 7: current_attributes |= VT_INVERSE; break;

            case 22: current_attributes &= ~VT_BOLD; break;
            case 24: current_attributes &= ~VT_UNDERLINE; break;
            case 27: current_attributes &= ~VT_INVERSE; break;

            // Standard Foreground
            case 30: current_fg = 0x000000; break; // Black
            case 31: current_fg = 0xC50F1F; break; // Red
            case 32: current_fg = 0x13A10E; break; // Green
            case 33: current_fg = 0xC19C00; break; // Yellow
            case 34: current_fg = 0x0037DA; break; // Blue
            case 35: current_fg = 0x881798; break; // Purple
            case 36: current_fg = 0x3A96DD; break; // Cyan
            case 37: current_fg = 0xCCCCCC; break; // Whatever this is

            // Extended Foreground (38;5;n)
            case 38: {
                if (j + 2 < parameter_count && parameter_list[j+1] == 5) {
                    int color_index = parameter_list[j+2];
                    current_fg = get_ansi_256_color(color_index); // See helper below
                    j += 2; // Skip the '5' and the 'index' so loop doesn't process them
                }
                break;
            }

            case 39: current_fg = 0xFFFFFF; break; // Reset FG

            // Standard Background
            case 40: current_bg = 0x000000; break; // Black
            case 41: current_bg = 0xC50F1F; break; // Red
            case 42: current_bg = 0x13A10E; break; // Green
            case 43: current_bg = 0xC19C00; break; // Yellow
            case 44: current_bg = 0x0037DA; break; // Blue
            case 45: current_bg = 0x881798; break; // Purple
            case 46: current_bg = 0x3A96DD; break; // Cyan
            case 47: current_bg = 0xCCCCCC; break; // Whatever this is

            // Extended Background (48;5;n)
            case 48: {
                if (j + 2 < parameter_count && parameter_list[j+1] == 5) {
                    int color_index = parameter_list[j+2];
                    current_bg = get_ansi_256_color(color_index); // See helper below
                    j += 2; // Skip ahead
                }
                break;
            }
            
            case 49: current_bg = 0x000000; break; // Reset BG
        }
    }
}