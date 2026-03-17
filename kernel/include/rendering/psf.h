/* A renderer for psf1 fonts */
#pragma once
#include <stdint.h>
#include <stddef.h>

#include <limine.h>
#include <drivers/graphics/common.h>


#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define PSF1_MODE512 	0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODESEQ	0x04

#define PSF_EXTENSION ".psf"


// The psf1 header
struct psf1_header_t{
	unsigned char magic[2];
	unsigned char mode;
	unsigned char charsize;
} __attribute__((packed));

// A font structure
struct psf1_font_t{
	psf1_header_t* psf1_header;
	char* glyphBuffer;
	size_t size;
} __attribute__((packed));

extern bool is_psf_renderer_initialized;

void init_psf_renderer(limine_file* file);
void draw_char(unsigned int fg, unsigned int bg, wchar_t chr, unsigned int x_start, unsigned int y_start, bool bold, bool underline, drivers::GraphicsDriver* driver);