#include <UserInput/keyboard.h>
#include <pipe.h>

namespace PS2KB{
    bool ScrollLock = false;
    bool NumberLock = false; 
    bool CapsLock = false;
    bool LeftShiftDown = false;
    bool RightShiftDown = false;

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
            case LeftShift:
                LeftShiftDown = true;
                return;
            case LeftShift + 0x80:
                LeftShiftDown = false;
                return;
            case RightShift:
                RightShiftDown = true;
                return;
            case RightShift + 0x80:
                RightShiftDown = false;
                return;
            case SCROLL_LOCK:
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
            case NUM_LOCK:
                NumberLock = !NumberLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);
                return;
            case CAPS_LOCK:
                CapsLock = !CapsLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);
                return;
        }

        char ascii = QWERTYKeyboard::Translate(scancode, (CapsLock || (LeftShiftDown || RightShiftDown)) && !(CapsLock && (LeftShiftDown || RightShiftDown)), NumberLock);
        
        if (scancode == BackSpace) ascii = '\b';
        if (scancode == Enter) ascii = '\n';

        if (ascii == 0) return;

        if (tty::focused_tty != nullptr){
            tty::focused_tty->handle_key(ascii);
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
    }
}