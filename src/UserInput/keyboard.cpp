#include "keyboard.h"

const char* charToConstCharPtr(char c) {
    static char buffer[2]; // 1 char + 1 for null terminator
    buffer[0] = c;         // Set the character
    buffer[1] = '\0';      // Null terminate the string
    return buffer;         // Return the pointer to the buffer
}

bool isLeftShiftPressed;
bool isRightShiftPressed;

void HandleKeyboard(uint8_t scancode){

    switch (scancode){
        case LeftShift:
            isLeftShiftPressed = true;
            return;
        case LeftShift + 0x80:
            isLeftShiftPressed = false;
            return;
        case RightShift:
            isRightShiftPressed = true;
            return;
        case RightShift + 0x80:
            isRightShiftPressed = false;
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
    }

    char ascii = QWERTYKeyboard::Translate(scancode, isLeftShiftPressed | isRightShiftPressed);

    if (ascii != 0){
        globalRenderer->print(charToConstCharPtr(ascii));
    }

}