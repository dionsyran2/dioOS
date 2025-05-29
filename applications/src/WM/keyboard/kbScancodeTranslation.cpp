#include "kbScancodeTranslation.h"

namespace QWERTYKeyboard {

    const wchar_t ASCIITable[] = {
         0 ,  0 , L'1', L'2',
        L'3', L'4', L'5', L'6',
        L'7', L'8', L'9', '0',
        L'-', L'=',  0 ,  0 ,
        L'q', L'w', L'e', L'r',
        L't', L'y', L'u', L'i',
        L'o', L'p', L'[', L']',
         0 ,  0 , L'a', L's',
        L'd', L'f', L'g', L'h',
        L'j', L'k', L'l', L';',
        L'\'',L'`',  0 , L'\\',
        L'z', L'x', 'c', L'v',
        L'b', L'n', L'm', L',',
        L'.', L'/',  0 , L'*',
         0 , ' '
    };
    const wchar_t ASCIITableUpper[] = {
        '.',  0 , '!', '@',
        '#', '$', '%', '^',
        '&', '*', '(', ')',
        '_', '+',  0 ,  0 ,
        'Q', 'W', 'E', 'R',
        'T', 'Y', 'U', 'I',
        'O', 'P', '{', '}',
         0 ,  0 , 'A', 'S',
        'D', 'F', 'G', 'H',
        'J', 'K', 'L', ':',
        '"','~',  0 , '|',
        'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<',
        '>', '?',  0 , '*',
         0 , ' '
    };

    //0x47
    const wchar_t NumPad[] = {
       '7', '8', '9', '-',
       '4', '5', '6', '+',
       '1', '2', '3', '0',
       '.',  0
    };
    const wchar_t NumPad_Sec[] = {
         0 ,  0 ,  0 , '-',
         0 ,  0 ,  0 , '+',
         0 ,  0 ,  0 ,  0 ,
        '.',  0
    };

    wchar_t Translate(uint8_t scancode, bool uppercase, bool numlock){
    // Special cases
        switch (scancode){
            case Enter:     return L'\n';
        }

        // NumPad range
        if (scancode >= 0x47 && scancode <= 0x53){
            return numlock ? NumPad[scancode - 0x47] : NumPad_Sec[scancode - 0x47];
        }

        // Printable ASCII
        if (scancode < sizeof(ASCIITable)){
            return uppercase ? ASCIITableUpper[scancode] : ASCIITable[scancode];
        }

        // Unknown key
        return 0;
    }

}