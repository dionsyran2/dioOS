#include <UserInput/kbScancodeTranslation.h>

namespace QWERTYKeyboard {

    const char ASCIITable[] = {
         0 ,  0 , '1', '2',
        '3', '4', '5', '6',
        '7', '8', '9', '0',
        '-', '=',  0 ,  0 ,
        'q', 'w', 'e', 'r',
        't', 'y', 'u', 'i',
        'o', 'p', '[', ']',
         0 ,  0 , 'a', 's',
        'd', 'f', 'g', 'h',
        'j', 'k', 'l', ';',
        '\'','`',  0 , '\\',
        'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',',
        '.', '/',  0 , '*',
         0 , ' '
    };
    const char ASCIITableUpper[] = {
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
    const char NumPad[] = {
       '7', '8', '9', '-',
       '4', '5', '6', '+',
       '1', '2', '3', '0',
       '.',  0
    };
    const char NumPad_Sec[] = {
         0 ,  0 ,  0 , '-',
         0 ,  0 ,  0 , '+',
         0 ,  0 ,  0 ,  0 ,
        '.',  0
    };

    char Translate(uint8_t scancode, bool uppercase, bool numlock){
        if (scancode > 0x3A){
            if (scancode < 0x54 && scancode > 0x46){
                if (numlock){
                    return NumPad[scancode - 0x47];
                }else{
                    return NumPad_Sec[scancode - 0x47];
                }
            }
            return 0;
        }

        if (uppercase){
            return ASCIITableUpper[scancode];
        }
        else return ASCIITable[scancode];
    }

}