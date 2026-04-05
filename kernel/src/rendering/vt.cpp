/* Virtual Terminal Code */
#include <rendering/vt.h>
#include <drivers/serial/serial.h>
#include <scheduling/task_scheduler/task_scheduler.h>
#include <drivers/audio/pc_speaker/pc_speaker.h>
#include <memory/heap.h>
#include <rendering/psf.h>
#include <cstr.h>
#include <events.h>
#include <math.h>
#include <syscalls/files/ioctl.h>
#include <drivers/timers/common.h>

void dump_all_tasks() {
    serialf("--- SCHEDULER TASK DUMP ---\n");
    for (task_t* t = task_scheduler::task_list; t != nullptr; t = t->next) {
        serialf("NAME: %s | PID: %d | State: %d \n", t->name, t->pid, t->task_state);
    }
}

int vt_read(uint64_t offset, uint64_t length, void* buffer, vnode_t* this_node){
    virtual_terminal *vt = (virtual_terminal*)this_node->fs_identifier;

    while(!this_node->data_ready_to_read || vt->output_buffer_size == 0)
        task_scheduler::get_current_task()->ScheduleFor(10, BLOCKED);

    uint64_t rflags = spin_lock(&vt->input_lock);

    uint64_t to_read = min(vt->input_data_size, length);
    
    memcpy(buffer, vt->input_buffer, to_read);

    if (to_read != vt->input_data_size) 
        memmove(vt->input_buffer, vt->input_buffer + to_read, vt->input_data_size - to_read);

    vt->input_data_size -= to_read;
    if (vt->input_data_size == 0) this_node->data_ready_to_read = false;

    spin_unlock(&vt->input_lock, rflags);
    return to_read;
}

int vt_write(uint64_t offset, uint64_t length, const void* buffer, vnode_t* this_node){
    virtual_terminal *vt = (virtual_terminal*)this_node->fs_identifier;

    uint64_t rflags = spin_lock(&vt->output_lock);

    if ((vt->output_data_size + length) > vt->output_buffer_size || vt->output_buffer == nullptr){
        size_t required_size = vt->output_data_size + length;
        size_t new_size = vt->output_buffer_size ? vt->output_buffer_size * 2 : length;
        
        if (new_size < required_size) {
            new_size = required_size;
        }

        char* new_buffer = (char*)malloc(new_size);

        if (vt->output_buffer){
            memcpy(new_buffer, vt->output_buffer, vt->output_buffer_size);
            free(vt->output_buffer);
        }

        vt->output_buffer = new_buffer;
        vt->output_buffer_size = new_size;
    }

    memcpy(vt->output_buffer + vt->output_data_size, buffer, length);
    vt->output_data_size += length;

    spin_unlock(&vt->output_lock, rflags);

    
    if (vt->output_task->task_state == BLOCKED){
        vt->output_task->task_state = PAUSED;
    }

    return length;
}

struct key_entry {
    char normal;
    char shifted;
};

static const struct key_entry keymap[128] = {
    {0, 0},             // 0: KEY_RESERVED
    {0x1B, 0x1B},       // 1: KEY_ESC
    {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, 
    {'6', '^'}, {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, // 2-11
    {'-', '_'}, {'=', '+'}, {'\b', '\b'}, {'\t', '\t'},         // 12-15
    
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, 
    {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, {'o', 'O'}, {'p', 'P'}, // 16-25
    {'[', '{'}, {']', '}'}, {'\n', '\n'},                       // 26-28
    
    {0, 0},             // 29: KEY_LEFTCTRL
    
    {'a', 'A'}, {'s', 'S'}, {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, 
    {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'},             // 30-38
    {';', ':'}, {'\'', '"'}, {'`', '~'},                        // 39-41
    
    {0, 0},             // 42: KEY_LEFTSHIFT
    {'\\', '|'},        // 43: KEY_BACKSLASH
    
    {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, {'b', 'B'}, 
    {'n', 'N'}, {'m', 'M'},                                     // 44-50
    {',', '<'}, {'.', '>'}, {'/', '?'},                         // 51-53
    
    {0, 0},             // 54: KEY_RIGHTSHIFT
    {0, 0},             // 55: KEY_KPASTERISK
    {0, 0},             // 56: KEY_LEFTALT
    {' ', ' '},         // 57: KEY_SPACE
    
    // 58+: Everything else is {0,0} by default
};

struct virtual_terminal* main_vt = nullptr;

// @brief A helper to decode utf8 into a codepoint (e.g. wide char)
size_t decode_utf8(const char* str, uint32_t* codepoint) {
    uint8_t c = (uint8_t)str[0];

    if (c <= 0x7F) {
        // 1-byte (ASCII)
        *codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte
        uint8_t c1 = (uint8_t)str[1];
        if ((c1 & 0xC0) != 0x80) return 0;

        *codepoint = ((c & 0x1F) << 6) |
                    (c1 & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        // 3-byte
        uint8_t c1 = (uint8_t)str[1];
        uint8_t c2 = (uint8_t)str[2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return 0;

        *codepoint = ((c & 0x0F) << 12) |
                    ((c1 & 0x3F) << 6) |
                    (c2 & 0x3F);
        return 3;
    } else if ((c & 0xF8) == 0xF0) {
        // 4-byte
        uint8_t c1 = (uint8_t)str[1];
        uint8_t c2 = (uint8_t)str[2];
        uint8_t c3 = (uint8_t)str[3];
        if ((c1 & 0xC0) != 0x80 ||
            (c2 & 0xC0) != 0x80 ||
            (c3 & 0xC0) != 0x80) return 0;

        *codepoint = ((c & 0x07) << 18) |
                    ((c1 & 0x3F) << 12) |
                    ((c2 & 0x3F) << 6) |
                    (c3 & 0x3F);
        return 4;
    }

    // UNREACHABLE
    return 0;
}

#include <kerrno.h>
int vt_ioctl(int op, char* argp, vnode_t* this_node){
    task_t* self = task_scheduler::get_current_task();
    virtual_terminal *vt = (virtual_terminal*)this_node->fs_identifier;

    switch (op){
        case TCGETS:{
            self->write_memory(argp, (void*)&vt->settings, sizeof(termios));
            break;
        }
        case TCSETSF:
        case TCSETS:{
            if (argp == nullptr) break;
            self->read_memory(argp, (void*)&vt->settings, sizeof(termios));
            break;
        }
        case TCSETSW:{
            if (argp == nullptr) break;
            self->read_memory(argp, (void*)&vt->settings, sizeof(termios));
            break;
        }
        case 0x4B33:
        case TIOCGWINSZ:{ // get window size
            winsize w;
            w.ws_col = vt->width;
            w.ws_row = vt->height;
            w.ws_xpixel = vt->width * 16;
            w.ws_ypixel = vt->height * 8;

            self->write_memory(argp, &w, sizeof(w));
            break;
        }
        case TIOCSWINSZ:{ // set window size
            return 0;
        }
        case TIOCSPGRP: {
            int pgid;
            self->read_memory(argp, &pgid, sizeof(int));
            //memset(&tty->term, 0, sizeof(termios));
            //vt->foreground_group_id = pgid;

            vt->settings.c_cflag |= (CS8 | CREAD | CLOCAL);
            vt->settings.c_lflag |= (ICANON | ECHO | ECHOE | ISIG);
            vt->settings.c_oflag |= ONLCR;
            break;
        }
        case TIOCGPGRP: {
            self->write_memory(argp, &task_scheduler::get_current_task()->pgid, sizeof(int));//tty->foreground_group_id;
            break;
        }
        case TIOCGSID: {
            self->write_memory(argp, &self->sid, sizeof(int));
            break;
        }

        case TIOCSCTTY: {
            break;
        }
        default:
            return -ENOTTY;
    }
    return 0;
}


// Returns an ASCII char, or 0 if no character should be printed
char virtual_terminal::vt_process_event(struct input_event *ev) {
    if (ev->type != EV_KEY) return 0;

    // --- Update Modifier State ---
    if (ev->code == KEY_LEFTSHIFT || ev->code == KEY_RIGHTSHIFT) {
        shift_held = (ev->value == 1);
        return 0;
    }
    if (ev->code == KEY_LEFTCTRL || ev->code == KEY_RIGHTCTRL) {
        ctrl_held = (ev->value == 1);
        return 0;
    }
    if (ev->code == KEY_CAPSLOCK && ev->value == 1) {
        caps_lock = !caps_lock;
        ps2_kb::set_lights(caps_lock, scroll_lock, num_lock);
        return 0;
    }
    if (ev->code == KEY_NUMLOCK && ev->value == 1) {
        num_lock = !num_lock;
        ps2_kb::set_lights(caps_lock, scroll_lock, num_lock);
        return 0;
    }
    if (ev->code == KEY_SCROLLLOCK && ev->value == 1) {
        scroll_lock = !scroll_lock;
        ps2_kb::set_lights(caps_lock, scroll_lock, num_lock);
        return 0;
    }

    if (ev->value == 0) return 0; // Ignore releases for printing

    if (ev->code == KEY_F12){
        dump_all_tasks();
    }

    // --- Handle Numpad Specifics ---
    switch (ev->code) {
        // Constant Arithmetic Keys
        case KEY_KPSLASH:    return '/';
        case KEY_KPASTERISK: return '*';
        case KEY_KPMINUS:    return '-';
        case KEY_KPPLUS:     return '+';
        case KEY_KPENTER:    return '\n';
        
        // Context-Sensitive Keys (NumLock ON = Char, OFF = Navigation)
        case KEY_KPDOT: return num_lock ? '.' : 0; // Del if off
        case KEY_KP0:   return num_lock ? '0' : 0; // Ins if off
        case KEY_KP1:   return num_lock ? '1' : 0; // End
        case KEY_KP2:   return num_lock ? '2' : 0; // Down
        case KEY_KP3:   return num_lock ? '3' : 0; // PgDn
        case KEY_KP4:   return num_lock ? '4' : 0; // Left
        case KEY_KP5:   return num_lock ? '5' : 0; // Center (usually nothing)
        case KEY_KP6:   return num_lock ? '6' : 0; // Right
        case KEY_KP7:   return num_lock ? '7' : 0; // Home
        case KEY_KP8:   return num_lock ? '8' : 0; // Up
        case KEY_KP9:   return num_lock ? '9' : 0; // PgUp
    }

    // --- Handle Standard Keys ---
    if (ev->code >= 128) return 0; // Out of bounds

    struct key_entry entry = keymap[ev->code];
    char final_char = 0;

    bool is_alpha = (entry.normal >= 'a' && entry.normal <= 'z');

    if (is_alpha) {
        bool active_shift = shift_held ^ caps_lock; 
        final_char = active_shift ? entry.shifted : entry.normal;
    } else {
        final_char = shift_held ? entry.shifted : entry.normal;
    }
    
    // Control sequences (Ctrl+C, etc.)
    if (ctrl_held && final_char) {
        if (final_char >= 'a' && final_char <= 'z') {
            final_char = final_char - 'a' + 1; 
        } else if (final_char >= 'A' && final_char <= 'Z') {
             final_char = final_char - 'A' + 1;
        }
    }

    return final_char;
}

void vt_input_task(virtual_terminal* vt){
    task_t* self = task_scheduler::get_current_task();
    vnode_t* tst = vfs::resolve_path("/dev/input/event0");

    input_event t;

    while(1){
        if (!tst) {
            self->exit(-1);
        }

        tst->read(0, sizeof(t), &t);
        
        char c = vt->vt_process_event(&t);

        const char* seq = nullptr;
        
        if (t.value >= 1) { 
            switch (t.code) {
                // --- Cursor Movement ---
                case KEY_UP:    seq = "\e[A"; break;
                case KEY_DOWN:  seq = "\e[B"; break;
                case KEY_RIGHT: seq = "\e[C"; break;
                case KEY_LEFT:  seq = "\e[D"; break;
                case KEY_HOME:  seq = "\e[H"; break;
                case KEY_END:   seq = "\e[F"; break;

                case KEY_F1:    seq = "\eOP"; break;
                case KEY_F2:    seq = "\eOQ"; break;
                case KEY_F3:    seq = "\eOR"; break;
                case KEY_F4:    seq = "\eOS"; break;
                case KEY_F5:    seq = "\e[15~"; break;
                case KEY_F6:    seq = "\e[17~"; break;
                case KEY_F7:    seq = "\e[18~"; break;
                case KEY_F8:    seq = "\e[19~"; break;
                case KEY_F9:    seq = "\e[20~"; break;
                case KEY_F10:   seq = "\e[21~"; break;
                case KEY_F11:   seq = "\e[23~"; break;
                case KEY_F12:   seq = "\e[24~"; break;

                // --- Page Control ---
                case KEY_PAGEUP:   seq = "\e[5~"; break;
                case KEY_PAGEDOWN: seq = "\e[6~"; break;
                case KEY_INSERT:   seq = "\e[2~"; break;
                case KEY_DELETE:   seq = "\e[3~"; break;
            }
        }

        if (!c && !seq) continue;
        
        uint64_t rflags = spin_lock(&vt->input_lock);

        if (vt->input_data_size >= sizeof(vt->input_buffer) - 6) { // Reserve space for longest seq
            spin_unlock(&vt->input_lock, rflags);
            continue;
        }

        if (seq) {
            while (*seq) {
                vt->input_buffer[vt->input_data_size++] = *seq++;
            }
            vt->node->data_ready_to_read = true;
            spin_unlock(&vt->input_lock, rflags);
            continue;
        } 
        
        if (c == '\b' && (vt->settings.c_lflag & ICANON) == ICANON) {
            if (vt->input_data_size != 0) {
                vt->input_data_size--;
            }
        } else {
            vt->input_buffer[vt->input_data_size++] = c;
        }

        if ((vt->settings.c_lflag & ICANON) == ICANON) {
            if (c == '\n') vt->node->data_ready_to_read = true;
        } else {
            vt->node->data_ready_to_read = vt->input_data_size > 0;
        }
        
        if ((c >= ' ' || c == '\b' || c == '\n') && (vt->settings.c_lflag & ECHO) == ECHO) {
            vt->write(c);
        }
        
        spin_unlock(&vt->input_lock, rflags);
    }
}

void vt_output_task(virtual_terminal* vt){
    task_t* self = task_scheduler::get_current_task();
    bool cursor_state = false;
    bool needs_render = false;

    uint64_t last_cursor_update_time = GetTicks();

    while(1){
        uint64_t rflags = spin_lock(&vt->output_lock);

        if (vt->output_data_size){
            vt->write(vt->output_buffer, vt->output_data_size);
            vt->output_data_size = 0;

            needs_render = true;
        }


        // Blink the cursor every 500 ms
        if ((last_cursor_update_time + 500) <= GetTicks()){
            last_cursor_update_time = GetTicks();
            if (cursor_state){
                // Revert the cell to its previous state
                cursor_state = false;

                vt_cell* cursor_cell = &vt->cell_table[vt->cursor_y * vt->width + vt->cursor_x];
                if (cursor_cell->bg == 0xFFFFFFFF && cursor_cell->fg == 0 && cursor_cell->chr == ' '){
                    //vt->cursor_prev_cell_state.chr = 0;
                    vt->set_cell(
                        vt->cursor_x, vt->cursor_y, vt->cursor_prev_cell_state.chr == 0 ? ' ' : vt->cursor_prev_cell_state.chr, 
                        vt->cursor_prev_cell_state.attributes, vt->cursor_prev_cell_state.fg, 
                        vt->cursor_prev_cell_state.bg
                    );
                    vt->print_cell(vt->cursor_x, vt->cursor_y);
                    needs_render = true;
                }
            } else if (!vt->disable_cursor) {
                // Draw the cursor
                cursor_state = true;

                vt_cell* cell = &vt->cell_table[vt->offset_y * vt->width + vt->offset_x];
                vt->cursor_y = vt->offset_y;
                vt->cursor_x = vt->offset_x;
                
                vt->cursor_prev_cell_state.chr = cell->chr;
                vt->cursor_prev_cell_state.attributes = cell->attributes;
                vt->cursor_prev_cell_state.fg = cell->fg; 
                vt->cursor_prev_cell_state.bg = cell->bg;

                cell->bg = 0xFFFFFFFF;
                cell->fg = 0;
                cell->chr = ' ';
                vt->print_cell(vt->cursor_x, vt->cursor_y);
                needs_render = true;
            }
        }

        if (needs_render && vt->driver) {
            needs_render = false;
            vt->update_screen();
        }
        
        spin_unlock(&vt->output_lock, rflags);

        self->ScheduleFor(16, BLOCKED);
    }
}

virtual_terminal::virtual_terminal(drivers::GraphicsDriver* driver){
    if (driver == nullptr) return;
    memset(this, 0, sizeof(virtual_terminal));

    // Set the display driver
    this->driver = driver;

    // Calculate the size
    this->width = driver->width / VT_CELL_WIDTH;
    this->height = driver->height / VT_CELL_HEIGHT;

    // Allocate the array
    size_t total_size = this->width * this->height * sizeof(vt_cell);
    this->cell_table = (vt_cell*)malloc(total_size);
    memset(this->cell_table, 0, total_size);

    // Reset some values
    this->current_attributes = 0;
    this->current_bg = 0;
    this->current_fg = 0xFFFFFFFF;
    this->offset_x = 0;
    this->offset_y = 0;

    char path[128];

    int i = 0;
    while(1){
        stringf(path, sizeof(path), "/dev/tty%d", i);
        
        node = vfs::resolve_path(path);
        if (node){
            node->close();
            i++;
            continue;
        }

        node = vfs::create_path(path, VCHR);
        break;
    }
    node->ready_to_receive_data = true;
    node->data_ready_to_read = false;
    node->fs_identifier = (uint64_t)this;
    node->file_operations.write = vt_write;
    node->file_operations.read = vt_read;
    node->file_operations.ioctl = vt_ioctl;

    this->settings.c_cflag |= (CS8 | CREAD | CLOCAL);
    this->settings.c_lflag |= (ICANON | ECHO | ECHOE | ISIG);
    this->settings.c_oflag |= ONLCR;

    
    task_t* child = task_scheduler::create_process("VT Input Handler", (function)vt_input_task, false, false);
    child->registers.rdi = (uint64_t)this;
    task_scheduler::mark_ready(child);

    this->output_task = task_scheduler::create_process("VT Output Handler", (function)vt_output_task, false, false);
    this->output_task->registers.rdi = (uint64_t)this;
    task_scheduler::mark_ready(this->output_task);

    // return
    return;
}

// @brief Changes a cell
void virtual_terminal::set_cell(uint16_t x, uint16_t y, wchar_t chr, uint16_t attributes, uint32_t current_fg, uint32_t current_bg){
    // Sanity check
    if (x >= width || y >= height || this->driver == nullptr) return;

    // Get the cell
    vt_cell* cell = &this->cell_table[y * width + x];

    *cell = {chr, attributes, current_fg, current_bg}; // Update the cell
}

// @brief Draws a single cell on the screen
void virtual_terminal::print_cell(uint16_t x, uint16_t y){
    // Sanity check
    if (driver == nullptr) return;
    if (x >= width || y >= height || this->driver == nullptr) return;

    // Get the cell
    vt_cell* cell = &this->cell_table[y * width + x];

    // Print it
    uint32_t bg = cell->attributes & VT_INVERSE ? cell->fg : cell->bg;
    uint32_t fg = cell->attributes & VT_INVERSE ? cell->bg : cell->fg;

    draw_char(fg, bg, cell->chr, x * VT_CELL_WIDTH, y * VT_CELL_HEIGHT, cell->attributes & VT_BOLD, cell->attributes & VT_UNDERLINE, driver);
    mark_dirty(x * VT_CELL_WIDTH, y * VT_CELL_HEIGHT, VT_CELL_WIDTH, VT_CELL_HEIGHT);
};

// @brief Clears from (start_x, start_y) until (end_x, end_y)
void virtual_terminal::clear(uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y){
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

void virtual_terminal::clear(){
    // Sanity check
    if (driver == nullptr) return;

    // Clear the array
    for (uint16_t y = 0; y < height; y++){
        for (uint16_t x = 0; x < width; x++){
            set_cell(x, y, ' ', current_attributes, current_fg, current_bg);
        }
    }

    // Clear the screen
    driver->ClearScreen(current_bg);

    mark_dirty(0, 0, this->width * VT_CELL_HEIGHT, this->height * VT_CELL_HEIGHT);
    //driver->Update();
}

void virtual_terminal::mark_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h){
    if (x < dirty_min_x) dirty_min_x = x;
    if (y < dirty_min_y) dirty_min_y = y;
    if (x + w > dirty_max_x) dirty_max_x = x + w;
    if (y + h > dirty_max_y) dirty_max_y = y + h;
}

void virtual_terminal::reset_dirty(){
    dirty_min_x = width * VT_CELL_WIDTH;
    dirty_min_y = height * VT_CELL_HEIGHT; 
    dirty_max_x = 0;
    dirty_max_y = 0;
}

// It will update the 'dirty' region of the screen
void virtual_terminal::update_screen(){
    if (dirty_min_x > dirty_max_x || dirty_min_y > dirty_max_y) return;

    this->driver->Update(dirty_min_x, dirty_min_y, dirty_max_x - dirty_min_x, dirty_max_y - dirty_min_y);
    reset_dirty();
}

/* @brief Scrolls the terminal up by 1 row */
void virtual_terminal::scroll(){
    if (driver == nullptr) return;

    // Move height up
    memmove(this->cell_table, 
            &this->cell_table[1 * width], 
            sizeof(vt_cell) * width * (height - 1));

    // Clear the LAST row (height - 1)
    memset(&this->cell_table[(height - 1) * width], 
           0, 
           sizeof(vt_cell) * width);

    driver->Scroll(VT_CELL_HEIGHT);
    mark_dirty(0, 0, this->width * VT_CELL_WIDTH, this->height * VT_CELL_HEIGHT);
}

void virtual_terminal::write(const char* str, uint32_t length) {
    for (uint32_t i = 0; i < length; i++) {
        char c = str[i];

        switch (state) {
            case VT_STATE_NORMAL:
                if (c == '\033') {
                    state = VT_STATE_ESC;
                } else {
                    uint32_t codepoint;
                    size_t bytes = decode_utf8(&str[i], &codepoint);
                    if (bytes == 0) {
                        write(L'\ufffd');
                        continue;
                    }

                    write(codepoint);
                    i += bytes - 1; // Here we subtract size-1, since the for loop will increment it later on.
                }
                break;

            case VT_STATE_ESC:
                if (c == '[') {
                    state = VT_STATE_CSI;
                    memset(csi_params, 0, sizeof(csi_params));
                    csi_param_count = 0;
                    private_mode = false;
                } else if (c == 'M') { // Reverse Index
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
                } else {
                    // Unknown ESC sequence, fall back to normal
                    state = VT_STATE_NORMAL;
                }
                break;

            case VT_STATE_CSI:
                if (c == '?') {
                    private_mode = true;
                } else if (isdigit(c)) {
                    // Found a digit, start parsing parameters
                    state = VT_STATE_CSI_PARAM;
                    csi_params[csi_param_count] = c - '0';
                } else if (c == ';') {
                    // Empty parameter before semicolon?
                    csi_param_count++; 
                } else {
                    // Its a command character
                    handle_csi_command(c);
                    state = VT_STATE_NORMAL;
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
                    state = VT_STATE_NORMAL;
                }
                break;
        }
    }
}

void virtual_terminal::write(wchar_t chr){
    /* Handle special cases */
    switch (chr){
        case '\a':
            beep();
            return;

        case '\b':
            if (offset_x > 0){
                offset_x--;
                if ((this->settings.c_lflag & ECHOE) == ECHOE){ // ECHO-Erase
                    set_cell(offset_x, offset_y, ' ', current_attributes,current_fg, current_bg);
                    print_cell(offset_x, offset_y);
                }
            }
            return;

        case '\f':
            clear();
            offset_x = offset_y = 0;
            return;

        case '\n':
            if (this->settings.c_oflag & ONLCR) offset_x = 0;
            offset_y++;

            if (offset_y >= height){
                scroll();
                offset_y = height - 1;
            }
            return;

        case '\r':
            offset_x = 0;
            return;

        case '\t': {
            uint32_t tab_size = 8;
            uint32_t next_tab = ((offset_x / tab_size) + 1) * tab_size;
            if (next_tab >= width)
                write('\n');
            else
                offset_x = next_tab;
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
    if (offset_x >= width) {
        offset_x = 0;
        offset_y++;
    }

    // Scroll if necessary
    if (offset_y >= height){
        scroll();
        offset_y = height - 1;
    }

    // Otherwise print the character
    set_cell(offset_x, offset_y, chr, current_attributes, current_fg, current_bg);
    print_cell(offset_x, offset_y);

    // Increase the offset
    offset_x++;
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

void virtual_terminal::handle_csi_command(char command) {
    // Default parameter to 1 if missing
    int arg0 = (csi_param_count > 0) ? csi_params[0] : 1; 
    
    // Handle Private Mode Sequences (e.g. ?25h)
    if (private_mode) {
        switch (command) {
            case 'h': // SET Mode
                if (arg0 == 25) disable_cursor = false;
                break;
            case 'l': // RESET Mode
                if (arg0 == 25) disable_cursor = true;
                break;
        }
        return;
    }

    // Standard CSI
    switch (command) {
        case 'A': // Cursor Up
            offset_y = max(0, (int)offset_y - arg0);
            break;
        case 'B': // Cursor Down
            offset_y = min((int)height - 1, (int)offset_y + arg0);
            break;
        case 'C': // Cursor Forward
            offset_x = min((int)width - 1, (int)offset_x + arg0);
            break;
        case 'D': // Cursor Back
            offset_x = max(0, (int)offset_x - arg0);
            break;
        case 'H': // Cursor Position
        case 'f':
            {
                int row = (csi_param_count > 0 && csi_params[0] > 0) ? csi_params[0] - 1 : 0;
                int col = (csi_param_count > 1 && csi_params[1] > 0) ? csi_params[1] - 1 : 0;
                offset_y = clamp(row, 0, height - 1);
                offset_x = clamp(col, 0, width - 1);
            }
            break;
        case 'J': // Erase in Display
            arg0 = (csi_param_count > 0) ? csi_params[0] : 0; 
            if (arg0 == 2) this->clear();
            else if (arg0 == 1) this->clear(0, 0, offset_x, offset_y);
            else this->clear(offset_x, offset_y, width, height); // Arg 0
            break;
        case 'm': // SGR
            if (csi_param_count == 0) {
                int array[1];
                array[0] = 0;
                handle_sgr(array, 1);
            } else {
                handle_sgr(csi_params, csi_param_count);
            }
            break;
    }
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

void kprintchar(char chr){
    if (main_vt == nullptr) return serialf("%c", chr);

    main_vt->write(chr);
    main_vt->update_screen();
}

void kprint(const char* str, size_t len = 0){
    if (main_vt == nullptr) return serialf("%s", str);

    if (len){
        main_vt->write(str, len);
    }else{
        main_vt->write(str, strlen(str));
    }
}

#include <printf.h>

void kprintfva(const char* str, va_list args) {
    uint64_t rflags = spin_lock(&main_vt->output_lock);

    const uint64_t buffer_size = 1024;
    char buffer[buffer_size];
    int written = vsnprintf_(buffer, buffer_size, str, args);
    kprint(buffer, written);

    spin_unlock(&main_vt->output_lock, rflags);
}

void kprintf(const char* str, ...){
    va_list args;
    va_start(args, str);
    
    kprintfva(str, args);
    
    va_end(args);
}