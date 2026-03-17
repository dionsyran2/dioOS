/* A renderer for psf1 fonts */
#include <rendering/psf.h>
#include <memory/heap.h>
#include <memory.h>

#include <drivers/serial/serial.h>
psf1_font_t* psf_font = nullptr;
bool is_psf_renderer_initialized = false;

void init_psf_renderer(limine_file* file){
    // Allocate a buffer
    void* buffer = malloc(file->size);
    memcpy(buffer, file->address, file->size);

    psf1_header_t* psf_hdr = (psf1_header_t*)buffer;
    
    if (is_psf_renderer_initialized || psf_hdr->magic[0] != PSF1_MAGIC0 || psf_hdr->magic[1] != PSF1_MAGIC1) return;
    is_psf_renderer_initialized = true;
    psf_font = new psf1_font_t;
    psf_font->psf1_header = psf_hdr;
    psf_font->glyphBuffer = (char*)((uint64_t)buffer + sizeof(psf1_header_t));
    psf_font->size = file->size;
}

// PSF1 Fonts have a unicode table
uint16_t read_unicode_table(uint32_t codepoint) {
    if (((psf_font->psf1_header->mode & PSF1_MODEHASTAB) == 0) || codepoint < 0x7F)
        return codepoint < 0x7F ? (uint16_t)codepoint : '?';

    int glyph_count = (psf_font->psf1_header->mode & PSF1_MODE512) ? 512 : 256;
    uint16_t* unicode_table = (uint16_t*)((uint64_t)psf_font->glyphBuffer + (glyph_count * psf_font->psf1_header->charsize));

    uint64_t entries = (psf_font->size - sizeof(psf1_header_t) - (glyph_count * psf_font->psf1_header->charsize)) / sizeof(uint16_t);
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

void draw_char(unsigned int fg, unsigned int bg, wchar_t chr, unsigned int x_start, unsigned int y_start, bool bold, bool underline, drivers::GraphicsDriver* driver){
    if (!psf_font) return;

    chr = read_unicode_table(chr); // Ensure this returns a valid index
    
    // Pointer to the specific glyph in the font buffer
    uint8_t* glyph_ptr = (uint8_t*)psf_font->glyphBuffer + (chr * psf_font->psf1_header->charsize);

    // Font dimensions (Standard PSF1 is 8 pixels wide)
    const int font_width = 8;
    const int font_height = 16; 

    // Draw Background (Clear the cell)
    driver->DrawRectangle(x_start, y_start, font_width, font_height, bg);

    // Draw Glyph Loop
    for (unsigned long y = 0; y < font_height; y++){
        // Get the row byte from the font
        uint8_t row_byte = glyph_ptr[y]; 

        for(unsigned long x = 0; x < font_width; x++){
            // Check if bit is set (from MSB to LSB)
            if ((row_byte & (0b10000000 >> x)) > 0){
                
                // Draw the main pixel
                driver->PlotPixel(x_start + x, y_start + y, fg);

                // --- BOLD SUPPORT ---
                // If bold, draw the neighbor pixel to the right.
                // We verify (x + 1 < font_width) to prevent drawing outside the cell width
                if (bold && (x + 1 < font_width)) {
                    driver->PlotPixel(x_start + x + 1, y_start + y, fg);
                }
            }
        }
    }

    if (underline) {
        // Draw a line 2 pixels from the bottom
        // (y_start + 14) is standard for a 16px height font
        unsigned int underline_y = y_start + (font_height - 2);
        
        // Draw the line across the full width of the cell
        for (unsigned int x = 0; x < font_width; x++) {
             driver->PlotPixel(x_start + x, underline_y, fg);
        }
    }

    // Update Screen (Flush)
    driver->Update(x_start, y_start, font_width, font_height);
}