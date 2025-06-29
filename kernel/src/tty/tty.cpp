#include <tty/tty.h>
#include <scheduling/apic/apic.h>

namespace tty{
    tty_t* focused_tty = nullptr;

    int vnode_tty_write(const char* txt, size_t length, vnode* this_node){
        tty_t* tty = (tty_t*)this_node->fs_data;

        if ((uint64_t)txt == 1){
            return (int)tty->rows;
        }else if ((uint64_t)txt == 2){
            return (int)tty->cols;
        }

        tty->print(txt, length);
        return 0;
    }

    void* vnode_tty_read(size_t* length, vnode* this_node){
        tty_t* tty = (tty_t*)this_node->fs_data;
        while(tty->_waiting_for_read) asm ("hlt");
        tty->_waiting_for_read = true;

        tty->edit_offset = tty->current_column;
        while(tty->_data_to_read == false) asm ("hlt");
        tty->_waiting_for_read = false;
        tty->_data_to_read = false;
        
        *length = tty->output_length;
        return tty->output;
    }

    void toggle_cursor(tty_t* tty){
        while (1)
        {
            while(tty->_no_cursor);
            uint32_t row = tty->current_row;
            uint32_t col = tty->current_column;

            tty->set_chr(row, col, 0xFFFFFF, 0x000000, '_', 0);
            tty->draw_cell(row, col);
            Sleep(500);

            if (tty->get_cell(row, col)->ch != '_') continue;;
            tty->set_chr(row, col, 0, 0x000000, ' ', 0);
            tty->draw_cell(row, col);
            Sleep(500);
        }
        
    }

    int CreateTerminal(char* name){
        // Check if it exists
        char* full_path = new char[128];
        memset(full_path, 0, 128);
        strcat(full_path, "/dev/");
        strcat(full_path, name);

        if (vfs::resolve_path(full_path) != nullptr) return -1; // device already exists

        delete[] full_path;


        // Create the terminal
        vnode_t* dev = vfs::resolve_path("/dev");
        vnode_t* tty_node = vfs::mount_node(name, VCHR, dev);
        tty_node->ops.write = vnode_tty_write;
        tty_node->ops.load = vnode_tty_read;

        tty_t* tty = new tty_t;
        memset(tty, 0, sizeof(tty_t));

        tty_node->fs_data = (void*)tty;

        tty->node = tty_node;
        tty->mode = true; // Enable loop-back

        // We will use vga fonts that are 8x16
        tty->char_width = 8;
        tty->char_height = 16;

        tty->rows = globalRenderer->targetFramebuffer->common.framebuffer_height / tty->char_height;
        tty->cols = globalRenderer->targetFramebuffer->common.framebuffer_width / tty->char_width;

        tty->buffer = new char[1024];

        tty->fg = 0xFFFFFF;

        // Allocate the table
        tty->screen = new cell_t[tty->rows * tty->cols];

        tty->clear();

        taskScheduler::task_t* task = taskScheduler::CreateTask((void*)toggle_cursor, 0, false);
        task->Context = tty;
        task->valid = true;


        focused_tty = tty;
        return 0;
    }

    cell_t* tty_t::get_cell(uint32_t row, uint32_t col) {
        return &screen[(row * cols) + col];
    }


    void tty_t::draw_cell(uint32_t row, uint32_t col){
        uint32_t fb_x = col * this->char_width;
        uint32_t fb_y = row * this->char_height;

        cell_t* cell = get_cell(row, col);
        globalRenderer->draw_char_tty(cell->fg, cell->bg, cell->ch, fb_x, fb_y, (cell->attrs & TTY_UNDERLINE) != 0);
    }

    void tty_t::draw_screen(){
        for (uint32_t row = 0; row < this->rows; row++) {
            for (uint32_t col = 0; col < this->cols; col++) {
                draw_cell(row, col);
            }
        }
    }

    void tty::set_chr(uint32_t row, uint32_t col, Color fb, Color bg, char chr, uint32_t attrs){
        cell_t* cell = get_cell(row, col);
        cell->bg = bg;
        cell->fg = fb;
        cell->ch = chr;
        cell->attrs = attrs;
    }

    void tty::clear(){
        for (uint32_t row = 0; row < rows; row++){
            for (uint32_t col = 0; col < cols; col++){
                this->set_chr(row, col, 0xFFFFFF, 0, ' ', 0);
            }
        }

        this->current_column = 0;
        this->current_row = 0;
        this->draw_screen();
    }

    void tty::scroll_up(){
        for (int row = 1; row < (int)rows; row++){
            for (uint32_t col = 0; col < cols; col++){
                *get_cell(row - 1, col) = *get_cell(row, col);
            }
        }

        for (uint32_t col = 0; col < cols; col++){
            set_chr(rows - 1, col, 0xFFFFFF, 0, ' ', 0);
        }

        draw_screen();
    }

    int tty::handle_escape_sequence(const char* buffer, int rem) {
        int params[8] = {0};
        int count = 0;
        size_t i = 0;
        const char* p = buffer;

        while (count < 8 && isdigit(*p)) {
            params[count++] = kstrtol(p, (char**)&p, 10);
            if (*p == ';') p++;
        }

        char command = *p;
        i = (p - buffer) + 1; // +1 for the final command character

        switch (command) {
            case 'm': // Set Graphics Rendition
                for (int j = 0; j < count; j++) {
                    if (count == 0) {
                        params[count++] = 0;
                    }
                    switch (params[j]) {
                        case 0:
                            fg = 0xFFFFFF;
                            bg = 0x000000;
                            style = 0;
                            break;
                        case 1:
                            style |= TTY_BOLD;
                            break;
                        case 4:
                            style |= TTY_UNDERLINE;
                            break;
                        case 30: fg = 0x000000; break;
                        case 31: fg = 0xFF0000; break;
                        case 32: fg = 0x00FF00; break;
                        case 33: fg = 0xFFFF00; break;
                        case 34: fg = 0x0000FF; break;
                        case 35: fg = 0xFF00FF; break;
                        case 36: fg = 0x00FFFF; break;
                        case 37: fg = 0xFFFFFF; break;
                        case 40: bg = 0x000000; break;
                        case 41: bg = 0xFF0000; break;
                        case 42: bg = 0x00FF00; break;
                        case 43: bg = 0xFFFF00; break;
                        case 44: bg = 0x0000FF; break;
                        case 45: bg = 0xFF00FF; break;
                        case 46: bg = 0x00FFFF; break;
                        case 47: bg = 0xFFFFFF; break;
                    }
                }
                break;

            // You can add other cases like 'H', 'J', etc. later

            default:
                // Unknown sequence — ignore
                break;
        }

        return i;
    }

    void tty::print(const char* str, uint32_t length){
        _writing_data = true;
        for (uint32_t i = 0; i < length && str[i] != '\0'; i++){
            if (str[i] == '\033' && str[i + 1] == '['){
                i += 2;
                i += handle_escape_sequence(&str[i], length - i) - 1;
                continue;
            }
            write_chr(str[i]);
        }
        edit_offset = current_column;
        _writing_data = false;
    }


    void tty::write_chr(char chr){
        serialWrite(COM1, chr);
        bool prev = _no_cursor;
        _no_cursor = true;
        switch (chr){
            case '\a':
                print("[ALERT CODE]\n", 13);
                break;

            case '\b':
                if (current_column > 0){
                    current_column--;
                    set_chr(current_row, current_column, 0xFFFFFF, 0, ' ', 0);
                    draw_cell(current_row, current_column);
                }
                break;

            case '\f':
                clear();
                break;

            case '\n':
                current_column = 0;
                current_row++;
                break;

            case '\r':
                current_column = 0;
                break;

            case '\t': {
                uint32_t tab_size = 8;
                uint32_t next_tab = ((current_column / tab_size) + 1) * tab_size;
                if (next_tab >= cols)
                    write_chr('\n');
                else
                    current_column = next_tab;
                break;
            }

            case '\v':
                write_chr('\n');
                break;

            default:
                set_chr(current_row, current_column, fg, bg, chr, style);
                draw_cell(current_row, current_column);
                current_column++;
                break;
        }

        if (current_column >= cols) {
            current_column = 0;
            current_row++;
        }

        if (current_row >= rows) {
            scroll_up();
            current_row = rows - 1;
        }
        _no_cursor = prev;
    }


    void tty::handle_key(char ascii){
        while(_writing_data);
        if (!_waiting_for_read) return; // return if no input is requested

        if (ascii != '\0' && ascii != '\b'){
            write_chr(ascii);
        }

        // add it to a buffer;
        if (ascii == '\b') {
            if (offset_in_buffer == 0) return;
            offset_in_buffer--;
            if (edit_offset <= current_column){
                write_chr('\b');
            }

            return;
        }
        buffer[offset_in_buffer] = ascii;
        offset_in_buffer++;

        if (ascii == '\n'){
            output_length = offset_in_buffer;
            output = new char[output_length];

            memcpy(output, buffer, output_length);
            memset(buffer, 0, offset_in_buffer);
            offset_in_buffer = 0;
            _data_to_read = true;
        }
    }
}
