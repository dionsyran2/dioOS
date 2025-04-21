#include "keyboard.h"

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
            case Enter:
                globalRenderer->print("\n");
                return;
            case Spacebar:
                globalRenderer->print(" ");
                return;
            case BackSpace:
               globalRenderer->ClearChar(0);
               return;
            case SCROLL_LOCK:
                ScrollLock = !ScrollLock;
                SetLEDS(ScrollLock, NumberLock, CapsLock);
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

        InEvents::EVENT_KB* event = new InEvents::EVENT_KB; 
        event->type = 1;
        event->nextEvent = nullptr;
        event->scancode = scancode;
        event->data = (CAPS_LOCK << 2) | (NUM_LOCK << 1) | (SCROLL_LOCK << 0);
        
        InEvents::SendEvent(event);


        //char ascii = QWERTYKeyboard::Translate(scancode, (CapsLock || (LeftShiftDown || RightShiftDown)) && !(CapsLock && (LeftShiftDown || RightShiftDown)), NumberLock);
        
        //globalRenderer->printf(charToConstCharPtr(ascii));
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
            globalRenderer->printf(0xFF0000, "[PS/2 Keyboard] ");
            globalRenderer->printf("No keyboard is connected at port 1 (byte0: %hh | byte1: %hh)", byte0, byte1);
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
            globalRenderer->printf(0xFF0000, "[PS/2 Keyboard] ");
            globalRenderer->printf("Could not set the keyboard scan code. Key presses may not be handled as expected!");
        }

        SetIDTGate((void*)KeyboardInt_Handler, 0x21, IDT_TA_InterruptGate, 0x08);
        SetIrq(1, 0x21, 0);
        PS2::EnablePort(1, true);



    }
}