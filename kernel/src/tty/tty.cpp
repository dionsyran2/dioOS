#include <tty/tty.h>
#include <scheduling/apic/apic.h>
#include <kerrno.h>
#include <syscalls/syscalls.h>
#include <drivers/audio/pc_speaker/pc_speaker.h>
//#define SERIAL_TTY    // Dump writes to the serial


namespace tty{
    tty_t* focused_tty = nullptr;

    int vnode_tty_iocntl(int op, char* argp, vnode_t* node){
        tty_t* tty = (tty_t*)node->misc_data[0];
        switch (op){
            case TCGETS:{
                memcpy(argp, &tty->term, sizeof(termios));
                break;
            }
            case TCSETSF:
            case TCSETS:{
                if (argp == nullptr) break;
                memcpy(&tty->term, argp, sizeof(termios));
                break;
            }
            case TCSETSW:{
                if (argp == nullptr) break;
                memcpy(&tty->term, argp, sizeof(termios));
                break;
            }
            case 0x4B33:
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
                memset(&tty->term, 0, sizeof(termios));
                tty->foreground_group_id = pgid;

                tty->term.c_cflag |= CS8 | CREAD | CLOCAL;
                tty->term.c_lflag |= ICANON | ECHO | ECHOE | ISIG;
                tty->term.c_oflag |= ONLCR;
                //serialf("set pgid to %d\n\r", tty->foreground_group_id);
                break;
            }
            case TIOCGPGRP: {
                (*(int*)argp) = task_scheduler::get_current_task()->pgid;//tty->foreground_group_id;
                break;
            }
            default:
                //serialf("TTY IOCNTL: %llx %llx\n\r", op, argp);
                return -ENOTTY;
        }
        return 0;
    }

    /* CALLS */
    int vnode_tty_write(const char* txt, size_t length, vnode_t* node){
        tty_t* tty = (tty_t*)node->misc_data[0];
        return tty->in_pipe->write(txt, length); // why have i named every data buffer a txt... idc anymore, txt it is.
    }

    void* vnode_tty_read(size_t* length, vnode* node){
        tty_t* tty = (tty_t*)node->misc_data[0];

        bool canonical = tty->term.c_lflag & ICANON;
        while(!tty->out_pipe->data_read) __asm__ ("pause"); // wait for data
        if (canonical) while(!tty->newline_entered) __asm__ ("pause"); // if canonical mode is enabled, buffer the data till enter is pressed
        tty->newline_entered = false; // clear the flag

        void* ret;
        tty->out_pipe->read(length, &ret); // return the data
        node->data_read = tty->out_pipe->data_read;
        return ret;
    }

    uint32_t prev_cursor_pos[2];
    bool prev_cursor_state = false;
    uint64_t prev_cursor_ticks;
    void term_task(tty_t* tty){
        while(1){
            if (tty->in_pipe->data_read){
                size_t cnt = 0;
                pipe_data* p = (pipe_data*)tty->in_pipe->misc_data[0];
                void* buffer;
                tty->in_pipe->read(&cnt, &buffer); // WHY HAVE I NAMED IT LOAD?!?!
                if (prev_cursor_state) {globalRenderer->clear_cursor(prev_cursor_pos[0], prev_cursor_pos[1]); prev_cursor_state = false;}
                tty->print((const char*)buffer, cnt);
            }

            uint64_t ticks = APICticsSinceBoot;
            if ((ticks - 200) >= prev_cursor_ticks && !tty->disable_cursor){
                if (prev_cursor_state){
                    globalRenderer->clear_cursor(prev_cursor_pos[0], prev_cursor_pos[1]);
                }else{
                    prev_cursor_pos[0] = tty->current_column * tty->char_width;
                    prev_cursor_pos[1] = (tty->current_row * tty->char_height) + 2;
                    globalRenderer->draw_cursor(prev_cursor_pos[0], prev_cursor_pos[1], tty->fg);
                }
                prev_cursor_state = !prev_cursor_state;
                prev_cursor_ticks = ticks;
            }else if (tty->disable_cursor && prev_cursor_state){
                globalRenderer->clear_cursor(prev_cursor_pos[0], prev_cursor_pos[1]);
                prev_cursor_state = false;
            }
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
        tty_node->ops.read = vnode_tty_read;
        tty_node->ops.iocntl = vnode_tty_iocntl;
        tty_node->data_write = true;
        tty_node->is_tty = true;
        
        tty_t* tty = new tty_t;
        memset(tty, 0, sizeof(tty_t));

        tty_node->misc_data[0] = (void*)tty;

        tty->node = tty_node;

        // We will use vga fonts that are 8x16
        tty->char_width = 8;
        tty->char_height = 16;

        tty->rows = globalRenderer->targetFramebuffer->common.framebuffer_height / tty->char_height;
        tty->cols = globalRenderer->targetFramebuffer->common.framebuffer_width / tty->char_width;

        // Allocate the table
        tty->screen = new cell_t[tty->rows * tty->cols];
        tty->saved_screen = new cell_t[tty->rows * tty->cols];
        tty->fg = 0x39B54A; // idk why, make it green
        tty->bg = 0x000000;


        tty->clear();

        tty->term.c_cflag |= CS8 | CREAD | CLOCAL;
        tty->term.c_lflag |= ICANON | ECHO | ECHOE;
        tty->term.c_oflag |= ONLCR;



        tty->in_pipe = CreatePipe(name, nullptr);
        tty->out_pipe = CreatePipe(name, nullptr);

        task_t* main = task_scheduler::create_process("Teletype Terminal Emulator (TTY)", (function)term_task);
        main->rdi = (uint64_t)tty;
        main->jmp = true;
        task_scheduler::mark_ready(main);

        focused_tty = tty; // used for the keyboard driver
        return 0;
    }

    cell_t* tty_t::get_cell(uint32_t row, uint32_t col) {
        return &screen[(row * cols) + col];
    }


    void tty_t::draw_cell(uint32_t row, uint32_t col){
        uint32_t fb_x = col * this->char_width;
        uint32_t fb_y = row * this->char_height;

        cell_t* cell = get_cell(row, col);
        globalRenderer->draw_char_tty(reverse_video ? cell->bg : cell->fg, reverse_video ? cell->fg : cell->bg, cell->ch, fb_x, fb_y, (cell->attrs & TTY_UNDERLINE) != 0);
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
        reverse_video = false;
        for (uint32_t row = 0; row < rows; row++){
            for (uint32_t col = 0; col < cols; col++){
                this->set_chr(row, col, 0, 0, ' ', 0);
            }
        }

        this->current_column = 0;
        this->current_row = 0;
        this->draw_screen();
    }

    void tty::clear(uint32_t start[2], uint32_t end[2]) {
        reverse_video = false;
        if (end[1] >= cols) end[1] = cols - 1;
        if (end[0] >= rows) end[0] = rows - 1;


        for (uint32_t row = start[0]; row <= end[0]; row++) {
            uint32_t col_start = (row == start[0]) ? start[1] : 0;
            uint32_t col_end = (row == end[0]) ? (end[1] + 1) : cols;
            for (uint32_t col = col_start; col < col_end; col++) {
                this->set_chr(row, col, 0xFFFFFF, 0, ' ', 0);
                this->draw_cell(row, col);
            }
        }
    }

    void tty::scroll_up() {
        memmove(
            screen,
            screen + cols,
            (rows - 1) * cols * sizeof(cell_t)
        );

        // Clear the last row
        for (uint32_t col = 0; col < cols; col++) {
            set_chr(rows - 1, col, 0xFFFFFF, 0, ' ', 0);
        }

        draw_screen();
    }

    int tty::handle_csi_escape_sequence(const char* buffer, int rem) {
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
            case 'J':{ // screen erase?
                int arg = params[0];
                switch(arg){
                    case 0:{ // clear from cursor till screen end
                        uint32_t start[2] = {current_row, current_column};
                        uint32_t end[2] = {rows, cols};
                        this->clear(start, end);
                        break;
                    }
                    case 1:{ // clear from screen start till cursor
                        uint32_t start[2] = { 0 };
                        uint32_t end[2] = {current_row, current_column};
                        this->clear(start, end);
                        break;
                    }
                    case 2:{ // clear the entire screen
                        this->clear();
                        break;
                    }
                }
                break;
            }
            case 'f': {
                int row = params[0] - 1;
                int col = params[1] - 1;

                if (row < 0) row = 0;
                if (row > rows) row = rows - 1;
                if (col < 0) col = 0;
                if (col > cols) col = 0;

                this->current_row = row;
                this->current_column = col;
                //serialf("Moved the cursor to %d %d\n\r", current_column, current_row);
                break;
            }
            case 'H': {
                if (count == 2){ // move to that position
                    int row = params[0] - 1;
                    int col = params[1] - 1;

                    if (row < 0) row = 0;
                    if (row > rows) row = rows - 1;
                    if (col < 0) col = 0;
                    if (col > cols) col = 0;

                    this->current_row = row;
                    this->current_column = col;
                    //serialf("Moved the cursor to %d %d\n\r", current_column, current_row);
                    break;
                }
                // home
                this->current_row = 0;
                this->current_column = 0;
                //serialf("Moved the cursor to %d %d (home)\n\r", current_column, current_row);
                break;
            }
            case 'A': { // UP
                int row = (int)current_row - params[0];
                if (row < 0) row = 0;
                current_row = row;
                //serialf("Moved the cursor to %d %d up\n\r", current_column, current_row);
                break;
            }
            case 'B': { // DOWN
                int row = (int)current_row + params[0];
                if (row >= rows) row = rows - 1;
                current_row = row;
                //serialf("Moved the cursor to %d %d down\n\r", current_column, current_row);
                break;
            }
            case 'C': { // RIGHT
                int column = (int)current_column + params[0];
                if (column >= cols) column = cols - 1;
                current_column = column;
                //serialf("Moved the cursor to %d %d right\n\r", current_column, current_row);
                break;
            }
            case 'D': { // LEFT
                int column = (int)current_column - params[0];
                if (column < 0) column = 0;
                current_column = column;
                //serialf("Moved the cursor to %d %d left\n\r", current_column, current_row);
                break;
            }
            case 'E': { // Move to the begining # lines down
                int row = (int)current_row + params[0];
                if (row >= rows) row = rows - 1;
                current_row = row;
                current_column = 0;
                //serialf("Moved the cursor to %d %d bdown\n\r", current_column, current_row);
                break;
            }
            case 'F': { // Move to the begining # lines up
                int row = (int)current_row - params[0];
                if (row < 0) row = 0;
                current_row = row;
                current_column = 0;
                //serialf("Moved the cursor to %d %d bup\n\r", current_column, current_row);
                break;
            }
            case 'G': { // Move to # column
                int col = params[0] - 1;
                if (col < 0) col = 0;
                if (col >= cols) col = cols -1;
                current_column = col;
                //serialf("Moved the cursor to %d %d col\n\r", current_column, current_row);
                break;
            }
            case 'n': {
                if (count == 0) break; 
                switch (params[0]){
                    case 6:{ // request position
                        char array[15] = { 0 }; // should be enough
                        array[0] = '\033';
                        array[1] = '[';
                        strcat(array, toString(current_row));
                        strcat(array, ";");
                        strcat(array, toString(current_column));
                        strcat(array, "R");
                        //out_pipe->write(array, strlen(array));
                        //node->data_read = true;
                        //serialf("replied with \\%s %d %d\n\r", array, current_column, current_row);
                        break;
                    }
                }
                break;
            }
            
            case 'K':{ // same thing as above? i am not sure anymore
                int arg = params[0];
                switch (arg)
                {
                    case 0:{ // clear from cursor to line end
                        uint32_t start[2] = {current_row, current_column};
                        uint32_t end[2] = {current_row, cols};
                        this->clear(start, end);
                        break;
                    }
                    case 1:{ // clear from line start till the cursor
                        uint32_t start[2] = {current_row, 0};
                        uint32_t end[2] = {current_row, current_column};
                        this->clear(start, end);
                        break;
                    }
                    case 2:{ // clear the entire line
                        uint32_t start[2] = {current_row, 0};
                        uint32_t end[2] = {current_row, cols};
                        this->clear(start, end);
                        break;
                    }
                }
                break;
            }
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
                        case 7: // inverse
                            reverse_video = true;
                            break;
                        case 27:
                            reverse_video = false;
                            break;
                        case 30: fg = 0x000000; break;
                        case 31: fg = 0xFF0000; break;
                        case 32: fg = 0x39B54A; break; // 0x16C60C
                        case 33: fg = 0xFFFF00; break;
                        case 34: fg = 0x3B78FF; break;
                        case 35: fg = 0xFF00FF; break;
                        case 36: fg = 0x00FFFF; break;
                        case 37: fg = 0xFFFFFF; break;
                        case 40: bg = 0x000000; break;
                        case 41: bg = 0xFF0000; break;
                        case 42: bg = 0x39B54A; break; // 0x16C60C
                        case 43: bg = 0xFFFF00; break;
                        case 44: bg = 0x3B78FF; break;
                        case 45: bg = 0xFF00FF; break;
                        case 46: bg = 0x00FFFF; break;
                        case 47: bg = 0xFFFFFF; break;
                        //default: return 0;
                    }
                }
                break;

            default:
                //return 0;
                break;
        }

        return i;
    }

    int tty::handle_private_escape_sequence(const char* buffer, int rem) {
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

            
            case 'l':{ // same thing as above? i am not sure anymore
                int arg = params[0];
                switch (arg){
                    case 25: // disable cursor
                        disable_cursor = true;
                        break;
                    case 47: // restore screen
                        memcpy(screen, saved_screen, rows * cols * sizeof(cell_t));
                        current_column = saved_screen_pos[0];
                        current_row = saved_screen_pos[1];
                        break;
                }
                break;
            }

            case 'h':{
                int arg = params[0];
                switch (arg){
                    case 25:
                        disable_cursor = false;
                        break;
                    case 47:
                        memcpy(saved_screen, screen, rows * cols * sizeof(cell_t));
                        saved_screen_pos[0] = current_column;
                        saved_screen_pos[1] = current_row;
                        break;
                }
                break;
            }
            
            default:
                //return 0;
                break;
        }

        return i;
    }

    void tty::print(const char* str, uint32_t length){
        #ifdef SERIAL_TTY
        for (uint32_t i = 0; i < length; i++){
            if (str[i] != '\a') serialf("%c", str[i]);
        }
        #endif
        for (uint32_t i = 0; i < length; i++){
            if (str[i] == '\0') continue;
            if (str[i] == '\033'){
                if (str[i + 1] == '['){ // csi escape sequence
                    if (str[i + 2] == '?'){ // csi escape sequence
                        i += 3;
                        i += handle_private_escape_sequence(&str[i], length - i) - 1;
                        continue;
                    }
                    i += 2;
                    i += handle_csi_escape_sequence(&str[i], length - i) - 1;
                    continue;
                }else if (str[i + 1] == 'M'){ // move 1 line up
                    if (current_row > 0) current_row--;
                    i += 2;
                    continue;
                }else if (str[i + 1] == '7'){ // DEC Save
                    saved_position[0] = current_column;
                    saved_position[1] = current_row;
                    i += 2;
                    continue;
                }else if (str[i + 1] == '8'){ // DEC restore
                    current_column = saved_position[0];
                    current_row = saved_position[1];
                    i += 2;
                    continue;
                }
            }
            write_chr(str[i]);
        }
    }


    void tty::write_chr(char chr){
        switch (chr){
            case '\a':
                beep();
                break;

            case '\b':
                if (current_column > 0){
                    current_column--;
                    if (term.c_lflag & ECHOE){ // ECHO-Erase
                        set_chr(current_row, current_column, fg, bg, ' ', style);
                        draw_cell(current_row, current_column);
                    }
                }
                break;

            case '\f':
                clear();
                break;

            case '\n':
                if (term.c_oflag & ONLCR) current_column = 0;
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
    }


    void tty::handle_key(char ascii, bool is_escape){
        if (is_escape) {
            switch (ascii){
                case 'A':{
                    out_pipe->write("\x1B[A", 3);
                    break;
                }
                case 'B':{
                    out_pipe->write("\x1B[B", 3);
                    break;
                }
                case 'C':{
                    out_pipe->write("\x1B[C", 3);
                    break;
                }
                case 'D':{
                    out_pipe->write("\x1B[D", 3);
                    break;
                }
                case 'E':{
                    //serialf("escape\n");
                    out_pipe->write("\x1B", 1);
                    break;
                }
                case 0x03:{ // Ctrl + C
                    //serialf("ctrl + c\n");
                    //if (term.c_lflag & ISIG) { // Ctrl+C
                        //serialf("isig\n");
                        if (foreground_group_id > 0) 
                            task_scheduler::send_signal_to_group(foreground_group_id, SIGINT);
                        //return;
                    //}
                    //out_pipe->write("\x03", 1);

                    break;
                }
                case 'q':{ // Ctrl + Q
                    out_pipe->write("\x11", 1);
                    break;
                }
                case 'd':{ // Ctrl + D
                    out_pipe->write("\x04", 1);
                    break;
                }
                case 'x':{ // Ctrl + X
                    out_pipe->write("\x18", 1);
                    break;
                }
                case '~' : { // Delete
                    out_pipe->write("\x1B[3~", 1);
                    break;
                }
                default:
                    write_chr('\a');
                    break;
            }
            
            
            node->data_read = out_pipe->data_read;

            return; // Ignore for now
        }

        bool echo = term.c_lflag & ECHO;
        bool canonical = term.c_lflag & ICANON;

        if (ascii == '\b') {
            cell_t* prev_cell = get_cell(current_row, current_column > 0 ? current_column - 1 : 0);

            if (current_column > 0 && prev_cell->is_usr) {
                // Delete last user-entered char if any
                delete_pipe_data(out_pipe, 1);
                node->data_read = out_pipe->data_read;
                if (echo) write_chr('\b');
            } else {
                // Beep if backspace when nothing to delete
                if (echo) {
                    write_chr('\a');
                }
            }

            if (!canonical){
                out_pipe->write(&ascii, 1);
                node->data_read = out_pipe->data_read;
            }

            return;
        }

        // Printable character
        if (ascii >= 32 || ascii == '\n' || ascii == '\t' || ascii == '\r') {
            if (echo) {
                cell_t* cell = get_cell(current_row, current_column);
                cell->is_usr = true;
                write_chr(ascii);
            }

            out_pipe->write(&ascii, 1);
            node->data_read = out_pipe->data_read;
            if (ascii == '\n') {
                newline_entered = true;
            }
        } else {
            // Unhandled control char
            if (echo) {
                write_chr('\a');
            }
        }
    }
}
