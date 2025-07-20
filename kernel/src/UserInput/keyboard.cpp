#include <UserInput/keyboard.h>
#include <pipe.h>
#include <kernel.h>

namespace PS2KB{
    bool ScrollLock = false;
    bool NumberLock = false; 
    bool CapsLock = false;
    bool LeftShiftDown = false;
    bool RightShiftDown = false;
    bool IS_CTRL_DOWN = false;
    bool waiting_on_second_int = false; // scancode 0xE0

    void SetLEDS(bool ScrollLock, bool NumberLock, bool CapsLock){
        uint8_t payload = 0;
        payload |= ScrollLock << 0;
        payload |= NumberLock << 1;
        payload |= CapsLock << 2;
        PS2::SendCommandToDevice(1, PS2_KB_CMD_LED, payload);
        Sleep(100);
        PS2::ReadData();
    }

    int sc_time = 0;
    int sc_am = 0;

    void HandleKBInt(){
        uint8_t scancode = PS2::ReadData();

        if (scancode == 0xFA){
            scancode = PS2::ReadData();
        }
        asm ("sti");

        switch (scancode){
            case PS2_KB_EXTENDED_BYTE:
                waiting_on_second_int = true;
                return;
            case KEY_DOWN::LEFT_SHIFT:
                LeftShiftDown = true;
                return;
            case KEY_DOWN::LEFT_SHIFT + KEY_UP_OFFSET:
                LeftShiftDown = false;
                return;
            case KEY_DOWN::RIGHT_SHIFT:
                RightShiftDown = true;
                return;
            case KEY_DOWN::RIGHT_SHIFT + KEY_UP_OFFSET:
                RightShiftDown = false;
                return;
            case KEY_DOWN::SCROLL_LOCK:
                ScrollLock = !ScrollLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);

                if ((GetAPICTick() - sc_time) < 1000){
                    sc_am++;
                } else{
                    sc_time = GetAPICTick();
                    sc_am = 1;
                }
                if (sc_am >= 3) Shutdown();

                return;
            case KEY_DOWN::NUMLOCK:
                NumberLock = !NumberLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);
                return;
            case KEY_DOWN::CAPS_LOCK:
                CapsLock = !CapsLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);
                return;
            case KEY_DOWN::LEFT_CTRL:
                IS_CTRL_DOWN = true;
                return;
            case KEY_DOWN::LEFT_CTRL + KEY_UP_OFFSET:
                IS_CTRL_DOWN = false;
                return;
        }
        char ascii = 0;
        bool is_esc = false;


        if (waiting_on_second_int){
            waiting_on_second_int = false;
            switch (scancode){
                case KEY_DOWN::CURSOR_UP:
                    ascii = 'A';
                    is_esc = true;
                    break;
                case KEY_DOWN::CURSOR_DOWN:
                    ascii = 'B';
                    is_esc = true;
                    break;
                case KEY_DOWN::CURSOR_RIGHT:
                    ascii = 'C';
                    is_esc = true;
                    break;
                case KEY_DOWN::CURSOR_LEFT:
                    ascii = 'D';
                    is_esc = true;
                    break;
                case KEY_DOWN::INSERT:
                    ascii = 'I';
                    is_esc = true;
                    break;
                case KEY_DOWN::DELETE:
                    ascii = '\b';
                    is_esc = true;
                    break;
                case KEY_DOWN::HOME:
                    ascii = 'H';
                    is_esc = true;
                    break;
                case KEY_DOWN::END:
                    ascii = 'F';
                    is_esc = true;
                    break;
                case KEY_DOWN::PAGE_UP:
                    ascii = 'U';
                    is_esc = true;
                    break;
                case KEY_DOWN::PAGE_DOWN:
                    ascii = 'P';
                    is_esc = true;
                    break;
                case KEY_DOWN::NUMPAD_ENTER:
                    ascii = '\n';
                    break;
            }
        }else{
            ascii = QWERTYKeyboard::Translate(scancode, (CapsLock || (LeftShiftDown || RightShiftDown)) && !(CapsLock && (LeftShiftDown || RightShiftDown)), NumberLock);
            
            switch (scancode){
                case KEY_DOWN::BACKSPACE:
                    ascii = '\b';
                    break;
                case KEY_DOWN::ENTER:
                    ascii = '\n';
                    break;
                case KEY_DOWN::ESCAPE:
                    ascii = 'E';
                    is_esc = true;
                    break;
                case KEY_DOWN::F1:
                    ascii = 11;
                    is_esc = true;
                    break;
                case KEY_DOWN::F2:
                    ascii = 12;
                    is_esc = true;
                    break;
                case KEY_DOWN::F3:
                    ascii = 13;
                    is_esc = true;
                    break;
                case KEY_DOWN::F4:
                    ascii = 14;
                    is_esc = true;
                    break;
                case KEY_DOWN::F5:
                    ascii = 15;
                    is_esc = true;
                    break;
                case KEY_DOWN::F6:
                    ascii = 17;
                    is_esc = true;
                    break;
                case KEY_DOWN::F7:
                    ascii = 18;
                    is_esc = true;
                    break;
                case KEY_DOWN::F8:
                    ascii = 19;
                    is_esc = true;
                    break;
                case KEY_DOWN::F9:
                    ascii = 20;
                    is_esc = true;
                    break;
                case KEY_DOWN::F10:
                    ascii = 21;
                    is_esc = true;
                    break;
                case KEY_DOWN::F11:
                    ascii = 23;
                    is_esc = true;
                    break;
                case KEY_DOWN::F12:
                    ascii = 24;
                    is_esc = true;
                    break;
            }
        }
        if ((ascii == 'c' || ascii == 'C') && IS_CTRL_DOWN){
            ascii = '\x03'; // Ctrl + C escape code
            is_esc = true;
        }
        if ((ascii == 'd' || ascii == 'D') && IS_CTRL_DOWN){
            ascii = 'd'; // Ctrl + D escape code
            is_esc = true;
        }
        if ((ascii == 'q' || ascii == 'Q') && IS_CTRL_DOWN){
            ascii = 'q'; // Ctrl + D escape code
            is_esc = true;
        }
        if (ascii == 0) return;

        if (tty::focused_tty != nullptr){
            tty::focused_tty->handle_key(ascii, is_esc);
        }
        //kprintf(charToConstCharPtr(ascii));
    }

    int Initialize(){
        //reset
        PS2::EnablePort(1, false);
        PS2::SendCommandToDevice(1, PS2_DEV_RESET);

        Sleep(10);

        PS2::ReadData();

        Sleep(10);
        PS2::ReadData();

        Sleep(10);
        PS2::ReadData();



        // get type
        PS2::SendCommandToDevice(1, PS2_DEV_DISABLE_SCANNING);


        Sleep(10);


        PS2::SendCommandToDevice(1, PS2_DEV_IDENTIFY);


        Sleep(20);

        uint8_t res = PS2::ReadData();


        Sleep(10);

        uint8_t byte0 = PS2::ReadData();
        Sleep(10);
        uint8_t byte1 = PS2::ReadData();
        /*if (!(byte0 == 0xAB && byte1 == 0x41)){
            kprintf(0xFF0000, "[PS/2 Keyboard] ");
            kprintf("No keyboard is connected at port 1 (byte0: %hh | byte1: %hh)", byte0, byte1);
            return -1;
        }*/

        PS2::SendCommandToDevice(1, PS2_DEV_ENABLE_SCANNING);
        Sleep(10);
        //Keyboard is connected and responding!

        PS2::SendCommandToDevice(1, PS2_KB_SCAN_CODE_SET, 2); //Select scan code set 2
        Sleep(10);
        res = PS2::ReadData();
        Sleep(10);
        if (res != PS2_RES_AKN){
            kprintf(0xFF0000, "[PS/2 Keyboard] ");
            kprintf("Could not set the keyboard scan code. Key presses may not be handled as expected!\n");
        }

        SetIDTGate((void*)KeyboardInt_Handler, 0x21, IDT_TA_InterruptGate, 0x08, &idtr);
        SetIrq(IRQtoGSI(1), 0x21, 0);
        PS2::EnablePort(1, true);

        kprintf(0x00FF00, "[PS/2] ");
        kprintf("Keyboard initialized!\n");
        return 0;
    }
}