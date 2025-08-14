#include <BasicRenderer.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/lock/spinlock.h>


BasicRenderer* globalRenderer;
//spinlock_t renderer_spinlock;

void BasicRenderer::Set(bool val){
    status = val;
}

BasicRenderer::BasicRenderer(display* screen, void* font)
{
    status = true;
    this->font_height = FONT_HEIGHT;
    this->screen = screen;
    this->screen->get_screen_size(&width, &height, nullptr);
    this->cursorPos.X = 0;
    this->cursorPos.Y = 0;
    this->font = (PSF1_FONT*)font;
}



void BasicRenderer::draw_char(unsigned int color, wchar_t chr, unsigned int xOff, unsigned int yOff){
    chr = read_unicode_table(chr);
    char* PSF1_FontPtr = (char*)font->glyphBuffer + (chr * font->psf1_Header->charsize);
    for (unsigned long y=yOff; y<yOff + 16; y++){
        for(unsigned long x = xOff; x<xOff + 8; x++){
            if((*PSF1_FontPtr & (0b10000000 >> (x - xOff))) > 0){
               screen->set_pixel(x, y, color);
            }
        }
        PSF1_FontPtr++;
    }
}

void BasicRenderer::HandleScrolling() {
    screen->scroll(font_height);

    for (int y = 0; y < font_height; y++){
        for (int x = 0; x < width; x++) {
            screen->set_pixel(x, height - (y + 1), 0);
        }
    }

    screen->update_screen();
    
    cursorPos.Y -= font_height;
}

void BasicRenderer::print(unsigned int color, const wchar_t* str){
    if (!status) return;

    wchar_t* chr = (wchar_t*)str;
    while (*chr != 0){
        if (*chr != '\n') serialWrite(COM1, *chr);
        
        size_t char_width = 8;
        if (cursorPos.Y + font_height >= height) {
            HandleScrolling();
        }


        // Draw character if within bounds
        if (*chr != '\n') {
            draw_char(color, *chr, cursorPos.X, cursorPos.Y);
        }

        cursorPos.X += char_width; // Move cursor to the right

        if (cursorPos.X > width || (*chr == '\n')) {
            cursorPos.X = 0; // Move back to start of the line
            cursorPos.Y += font_height; // Move down to the next line

            if (*chr == '\n'){
                serialPrint(COM1, "\n\r");
            }
        }
        chr++;
    }
}

void BasicRenderer::print(unsigned int color, const char* str){
    wchar_t* wchar = char_to_wchar_string((char*)str);
    print(color, wchar);
    delete wchar;
}
void BasicRenderer::printfva(uint32_t color, const wchar_t* str, va_list args){
    if (!status) return;


    wchar_t* chr = (wchar_t*)str;

    while (*chr != 0){
        if (*chr == '%'){
            chr++;
            switch (*chr){
                case 's':{ //%s string
                    const char* str = va_arg(args, char*);
                    print(color, str);
                    chr++;
                    break;
                }
                case 'd':{
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'f':{
                    const char* str = toString((double)va_arg(args, double));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'u':{// %u unsigned int
                    const char* str = toString((uint32_t)va_arg(args, int));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'x':{// %x unsigned hex | TODO: Add support for lowercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'X':{// %X unsigned hex uppercase (32 bit)
                    const char* str = toHString((uint32_t)va_arg(args, uint32_t));
                    print(color, str);
                    chr++;
                    break;
                }
                case 'h':{// %h unsigned hex (16 bit)
                    chr++;
                    if (*chr == 'h'){ //%hh unsigned hex (8 bit)
                        const char* str = toHString((uint8_t)va_arg(args, int));
                        print(color, str);
                        chr++;
                        break;
                    }else{ //16 bit
                        const char* str = toHString((uint16_t)va_arg(args, int));
                        print(color, str);
                        break;
                    }
                }
                case 'l':{// %l long unsigned decimal (32 bit)
                    chr++;
                    if (*chr == 'l'){
                        chr++;
                        if (*chr == 'x' || *chr == 'X'){ // %llx Unsigned hex (64 bit)
                            const char* str = toHString((uint64_t)va_arg(args, uint64_t));
                            print(color, str);
                            chr++;
                            break;
                        }else{ // %ll long unsigned decimal (64 bit)
                            const char* str = toString((uint64_t)va_arg(args, uint64_t));
                            print(color, str);
                            break;
                        }
                    }else{ //16 bit
                        const char* str = toString((uint32_t)va_arg(args, uint32_t));
                        print(color, str);
                        break;
                    }
                }
                case 'c':{
                    chr++;
                    char arg = va_arg(args, int);
                    char st[2] = { arg, '\0'};
                    print(color, st);
                    break;
                }
            }
        continue;
        }

        wchar_t c[2] = {*chr, L'\0'};
        print(color, c);
        chr++;
    }
    screen->update_screen();
}


void BasicRenderer::printfva(uint32_t color, const char* str, va_list args){
    wchar_t* wchar = char_to_wchar_string((char*)str);
    printfva(color, wchar, args);
    delete wchar;
}
void BasicRenderer::draw_char_tty(uint32_t color, uint32_t bg, wchar_t chr, unsigned int xOff, unsigned int yOff, bool underline){
    for (int y = 0; y < font_height; y++){
        for (int x = 0; x < 8; x++){
            screen->set_pixel(x + xOff, y + yOff, bg);
        }
    }
    
    draw_char(color, chr, xOff, yOff);

    if (!underline) {screen->update_screen(xOff, yOff, 8, font_height); return;}
    // UNDERLINE

    for (int y = font_height - 2; y < font_height; y++){
        for (int x = 0; x < 8; x++){
            screen->set_pixel(x + xOff, y + yOff, color);
        }
    }
    screen->update_screen(xOff, yOff, 8, font_height);
}

void BasicRenderer::draw_cursor(uint32_t xOff, uint32_t yOff, uint32_t clr){
    for (int y = font_height - 3; y < font_height; y++){
        for (int x = 0; x < 8; x++){
            screen->set_pixel(x + xOff, y + yOff, clr);
        }
    }
    screen->update_screen(xOff, yOff, 8, font_height);
}
void BasicRenderer::clear_cursor(uint32_t xOff, uint32_t yOff){
    for (int y = font_height - 3; y < font_height; y++){
        for (int x = 0; x < 8; x++){
            screen->set_pixel(x + xOff, y + yOff, 0);
        }
    }
    screen->update_screen(xOff, yOff, 8, font_height);
}


void BasicRenderer::Clear(unsigned int color){
    if (!status) return;
    screen->draw_rectangle(0, 0, width, height, color);
    screen->update_screen();
}

void BasicRenderer::DrawBMPScaled(int dstX, int dstY, int targetW, int targetH, uint8_t* bmpData) {
    BMPFileHeader* fileHeader = (BMPFileHeader*)bmpData;
    BMPInfoHeader* infoHeader = (BMPInfoHeader*)(bmpData + sizeof(BMPFileHeader));

    if (fileHeader->bfType != 0x4D42 || infoHeader->biBitCount != 24 || infoHeader->biCompression != 0) {
        kprintf("INVALID BMP");
        return;
    }

    int srcW = infoHeader->biWidth;
    int srcH = infoHeader->biHeight;
    uint8_t* pixelData = bmpData + fileHeader->bfOffBits;
    int rowSize = ((srcW * 3 + 3) / 4) * 4;

    for (int y = 0; y < targetH; y++) {
        int destY = dstY + y;

        for (int x = 0; x < targetW; x++) {
            int destX = dstX + x;

            int srcX = (x * srcW) / targetW;
            int srcY = (y * srcH) / targetH;

            uint8_t* pixel = pixelData + (srcH - 1 - srcY) * rowSize + srcX * 3;

            uint8_t blue  = pixel[0];
            uint8_t green = pixel[1];
            uint8_t red   = pixel[2];

            uint32_t color = 0xFF000000 | (red << 16) | (green << 8) | blue;
            screen->set_pixel(destX, destY, color);
        }
    }
}


void kprintf(unsigned int color, const char* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(color, str, args);
    va_end(args);
}

void kprintf(const char* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(0xFFFFFF, str, args);
    va_end(args);
}


void kprintf(unsigned int color, const wchar_t* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(color, str, args);
    va_end(args);
}

void kprintf(const wchar_t* str, ...){
    va_list args;
    va_start(args, str);
    globalRenderer->printfva(0xFFFFFF, str, args);
    va_end(args);
}

uint16_t BasicRenderer::read_unicode_table(uint32_t codepoint) {
    if (((font->psf1_Header->mode & PSF1_MODEHASTAB) == 0) || codepoint < 0x7F)
        return codepoint < 0x7F ? (uint16_t)codepoint : '?';

    int glyph_count = (font->psf1_Header->mode & PSF1_MODE512) ? 512 : 256;
    uint16_t* unicode_table = (uint16_t*)((uint64_t)font->glyphBuffer + (glyph_count * font->psf1_Header->charsize));

    uint64_t entries = (font->size - sizeof(PSF1_HEADER) - (glyph_count * font->psf1_Header->charsize)) / sizeof(uint16_t);
    int glyph = 0;
    for (uint64_t i = 0; i < entries;) {
        bool matched = false;

        // One or more sequences per glyph
        while (i < entries) {
            uint16_t cp = unicode_table[i++];
            if (cp == 0xFFFE) {
                continue; // Start of new sequence
            } else if (cp == 0xFFFF) {
                break; // End of current glyph entry
            } else if (cp == codepoint) {
                matched = true;
                break;
            }
        }

        if (matched) return glyph;

        glyph++;
        if (glyph >= glyph_count) break; // Avoid overflow
    }

    return read_unicode_table(0xFFFD); // replacement char
}