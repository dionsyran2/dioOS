#include <drivers/serial.h>
#include <cstr.h>

void InitSerial(uint16_t port){
    outb(port + 1, 0x00);
    
    outb(port + 3, 0x80);
    outb(port + 0, 1);
    outb(port + 1, 0);

    outb(port + 3, 0x03);

    outb(port + 2, 0xC7);

    outb(port + 1, 0x01);
}

void serialWrite(uint16_t port, char c) {
    while ((inb(port + 5) & 0x20) == 0);
    outb(port, c);
}
void serialPrint(uint16_t port, const char* str) {
    while (*str) {
        serialWrite(port, *str++);
    }
}

void serialfva(const char* str, va_list args){

    char* chr = (char*)str;

    while (*chr != 0){
        if (*chr == '%'){
            chr++;
            switch (*chr){
                case 's':{ //%s string
                    const char* str = va_arg(args, char*);
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'd':{
                    const char* str = toString((uint32_t)va_arg(args, int));
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'f':{
                    const char* str = toString((double)va_arg(args, double));
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'u':{// %u unsigned int
                    const char* str = toString((uint32_t)va_arg(args, int));
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'x':{// %x unsigned hex | TODO: Add support for lowercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'X':{// %X unsigned hex uppercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    serialPrint(COM1, str);
                    chr++;
                    break;
                }
                case 'h':{// %h unsigned hex (16 bit)
                    chr++;
                    if (*chr == 'h'){ //%hh unsigned hex (8 bit)
                        const char* str = toHString((uint8_t)va_arg(args, int));
                        serialPrint(COM1, str);
                        chr++;
                        break;
                    }else{ //16 bit
                        const char* str = toHString((uint16_t)va_arg(args, int));
                        serialPrint(COM1, str);
                        break;
                    }
                }
                case 'l':{// %l long unsigned decimal (32 bit)
                    chr++;
                    if (*chr == 'l'){
                        chr++;
                        if (*chr == 'x' || *chr == 'X'){ // %llx Unsigned hex (64 bit)
                            const char* str = toHString((uint64_t)va_arg(args, uint64_t));
                            serialPrint(COM1, str);
                            chr++;
                            break;
                        }else{ // %ll long unsigned decimal (64 bit)
                            const char* str = toString((uint64_t)va_arg(args, uint64_t));
                            serialPrint(COM1, str);
                            break;
                        }
                    }else{ //16 bit
                        const char* str = toString((uint32_t)va_arg(args, uint32_t));
                        serialPrint(COM1, str);
                        break;
                    }
                }
                case 'c':{
                    chr++;
                    char arg = va_arg(args, int);
                    serialWrite(COM1, arg);
                    break;
                }
            }
        continue;
        }

        serialWrite(COM1, *chr);
        chr++;
    }
}

void serialf(const char* str, ...){
    va_list args;
    va_start(args, str);
    serialfva(str, args);
    va_end(args);
}