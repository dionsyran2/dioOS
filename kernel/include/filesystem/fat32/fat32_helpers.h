#pragma once
#include <stdint.h>
#include <stddef.h>
#include <filesystem/fat32/fat32_structures.h>


// Helper to copy UCS-2 chars to ASCII buffer
static void _lfn_copy_name(uint16_t* ucs2, char* buffer, int count, int offset);

// @brief Parses a chain of LFN entries and reconstructs the filename.
fat_directory_entry_t* fat32_helper_parse_lfn(fat_directory_entry_t *current, char *name_buffer, uint8_t *out_checksum);

uint8_t lfn_checksum(const uint8_t* pFcbName);
void fat32_parse_sfn(fat_directory_entry_t* entry, char* out);

bool _is_valid_8_3(const char* name);
uint8_t _format_8_3(const char* input, char* output);

void _generate_sfn(const char* long_name, char* out_sfn);