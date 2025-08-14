#pragma once
#include <stdint.h>
#include <stddef.h>

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define PSF1_MODE512 	0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODESEQ	0x04

struct PSF1_HEADER{
	unsigned char magic[2];
	unsigned char mode;
	unsigned char charsize;
} ;

struct PSF1_FONT{
	PSF1_HEADER* psf1_Header;
	void* glyphBuffer;
	size_t size;
} ;