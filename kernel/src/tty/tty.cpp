#include <tty/tty.h>
#include <scheduling/apic/apic.h>
#include <kerrno.h>
#include <syscalls/syscalls.h>
#include <drivers/audio/pc_speaker/pc_speaker.h>

namespace tty{
    tty_t* focused_tty = nullptr;

    int vnode_tty_write(const char* txt, size_t length, vnode* this_node){
        tty_t* tty = (tty_t*)this_node->fs_data;

        tty->print(txt, length);
        return 0;
    }

    int vnode_tty_iocntl(int op, char* argp, vnode_t* node){
        tty_t* tty = (tty_t*)node->fs_data;
        switch (op){
            case TCGETS:{
                memcpy(argp, &tty->term, sizeof(termios));
                break;
            }
            case TCSETS:{
                if (argp == nullptr) break;
                memcpy(&tty->term, argp, sizeof(termios));
                serialf("TCSETS: %llx %llx %llx\n\r", tty->term.c_cflag, tty->term.c_iflag, tty->term.c_lflag);
                break;
            }
            case TCSETSW:{
                if (argp == nullptr) break;
                memcpy(&tty->term, argp, sizeof(termios));
                serialf("TCSETS: %llx %llx %llx\n\r", tty->term.c_cflag, tty->term.c_iflag, tty->term.c_lflag);
                break;
            }
            case TIOCGWINSZ:{ // get window size
                winsize* w = (winsize*)argp;
                w->ws_col = tty->cols;
                w->ws_row = tty->rows;
                w->ws_xpixel = tty->cols * tty->char_width;
                w->ws_ypixel = tty->rows * tty->char_height;
                break;
            }
            case TIOCSWINSZ:{ // set window size
                return 0;
            }
            case TIOCSPGRP: {
                int pgid = (*(int*)argp);
                tty->foreground_group_id = pgid;
                memset(&tty->term, 0, sizeof(termios));
                tty->term.c_cflag = CS8 | CREAD | CLOCAL;
                tty->term.c_lflag = ICANON | ECHO;
                serialf("set pgid to %d\n\r", pgid);
                break;
            }
            case TIOCGPGRP: {
                (*(int*)argp) = task_scheduler::get_current_task()->pgid;//tty->foreground_group_id;
                break;
            }
            default:
                serialf("TTY IOCNTL: %llx %llx\n\r", op, argp);
                return -ENOTTY;
        }
        return 0;
    }

    void* vnode_tty_read(size_t* length, vnode* this_node){
        tty_t* tty = (tty_t*)this_node->fs_data;
        
        if (tty->term.c_lflag & ICANON){// canonical mode
            tty->_waiting_for_read = true;
            while(this_node->data_available == false) __asm__ ("pause");
            this_node->data_available = false;
            tty->_waiting_for_read = false;
            char* buffer = new char[tty->offset_in_buffer + 1];
            memcpy(buffer, tty->buffer, tty->offset_in_buffer);
            buffer[tty->offset_in_buffer] = '\0';
            *length = tty->offset_in_buffer;
            tty->offset_in_buffer = 0;
            return buffer;
        }

        if (this_node->data_available == false){
            *length = 0;
            return nullptr;
        }

        while(tty->offset_in_buffer == 0);
        
        char* buffer = new char[tty->offset_in_buffer + 1];
        memcpy(buffer, tty->buffer, tty->offset_in_buffer);
        buffer[tty->offset_in_buffer] = '\0';
        *length = tty->offset_in_buffer;
        tty->offset_in_buffer = 0;
        return buffer;
    }

    void toggle_cursor(tty_t* tty){
        while (1)
        {
            while(tty->_no_cursor);
            uint32_t row = tty->current_row;
            uint32_t col = tty->current_column;

            globalRenderer->draw_cursor(col * tty->char_width, (row * tty->char_height) + 2);
            Sleep(500);
            
            while(tty->_no_cursor);

            globalRenderer->clear_cursor(col * tty->char_width, (row * tty->char_height) + 2);
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
        if (dev == nullptr){
            dev = vfs::mount_node("dev", VNODE_TYPE::VDIR, nullptr);
        }
        vnode_t* tty_node = vfs::mount_node(name, VCHR, dev);
        tty_node->ops.write = vnode_tty_write;
        tty_node->ops.load = vnode_tty_read;
        tty_node->ops.iocntl = vnode_tty_iocntl;

        tty_t* tty = new tty_t;
        memset(tty, 0, sizeof(tty_t));

        tty_node->fs_data = (void*)tty;

        tty->node = tty_node;

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

        tty->term.c_cflag |= CS8 | CREAD | CLOCAL;
        tty->term.c_lflag |= ICANON | ECHO;

        task_t* task = task_scheduler::create_thread(task_scheduler::get_current_task(), (function)toggle_cursor);
        if (task == nullptr) task = task_scheduler::create_process("Teletype Terminal Emulator Cursor", (function)toggle_cursor);
        task->rdi = (uint64_t)tty;
        task_scheduler::mark_ready(task);


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
                        case 32: fg = 0x16C60C; break;
                        case 33: fg = 0xFFFF00; break;
                        case 34: fg = 0x3B78FF; break;
                        case 35: fg = 0xFF00FF; break;
                        case 36: fg = 0x00FFFF; break;
                        case 37: fg = 0xFFFFFF; break;
                        case 40: bg = 0x000000; break;
                        case 41: bg = 0xFF0000; break;
                        case 42: bg = 0x16C60C; break;
                        case 43: bg = 0xFFFF00; break;
                        case 44: bg = 0x3B78FF; break;
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
        //if (chr != '\a') serialWrite(COM1, chr);
        bool prev = _no_cursor;
        _no_cursor = true;
        switch (chr){
            case '\a':
                beep();
                break;

            case '\b':
                if (current_column > 0){
                    current_column--;
                    set_chr(current_row, current_column, fg, bg, ' ', style);
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
        if (!_waiting_for_read && (term.c_lflag & ICANON)) return; // return if no input is requested (CANONICAL ONLY!)


        if (ascii != '\0' && ascii != '\b' && ascii > 5){
            if ((term.c_lflag & ECHO)) write_chr(ascii);
        }

        // add it to a buffer;
        if (ascii == '\b' && (term.c_lflag & ICANON)) {
            if (offset_in_buffer == 0) {write_chr('\a'); return;}
            offset_in_buffer--;
            if (edit_offset <= current_column){
                if ((term.c_lflag & ECHO)) write_chr('\b');
                if ((term.c_lflag & ECHO)) write_chr(' ');
                if ((term.c_lflag & ECHO)) write_chr('\b');
            }
            return;
        }

        if (ascii <= 5){
            buffer[offset_in_buffer] = '\x1B';
            offset_in_buffer++;

            buffer[offset_in_buffer] = '[';
            offset_in_buffer++;
            switch (ascii){
                case 1: // UP arrow
                    buffer[offset_in_buffer] = 'A';
                    break;
                case 2: // DOWN arrow
                    buffer[offset_in_buffer] = 'B';
                    break;
                case 3: // LEFT arrow
                    buffer[offset_in_buffer] = 'D';
                    break;
                case 4: // RIGHT arrow
                    buffer[offset_in_buffer] = 'C';
                    break;
            }
            offset_in_buffer++;
        }else{
            buffer[offset_in_buffer] = ascii;
            offset_in_buffer++;
        }
        
        if ((term.c_lflag & ICANON) == 0){
            node->data_available = true;
        }

        if (ascii == '\n' && (term.c_lflag & ICANON)){
            node->data_available = true;
        }
    }
}
