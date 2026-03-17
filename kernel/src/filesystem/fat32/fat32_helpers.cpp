#include <filesystem/fat32/fat32_helpers.h>
#include <memory.h>
#include <cstr.h>

// Helper to copy UCS-2 chars to ASCII buffer
static void _lfn_copy_name(uint16_t* ucs2, char* buffer, int count, int offset) {
    for (int i = 0; i < count; i++) {
        uint16_t c = ucs2[i];
        if (c == 0x0000 || c == 0xFFFF) {
            // Null terminator logic could go here, but usually we rely on memset 0
            continue; 
        }
        // Simple downcast to ASCII (only works for English/Basic Latin)
        buffer[offset + i] = (char)(c & 0xFF);
    }
}
// @brief Parses a chain of LFN entries and reconstructs the filename.
fat_directory_entry_t* fat32_helper_parse_lfn(fat_directory_entry_t *current, char *name_buffer, uint8_t *out_checksum) {
    fat_long_file_name* lfn = (fat_long_file_name*)current;
    
    // Clear buffer initially
    // We loop as long as the entry looks like an LFN (Attribute 0x0F)
    while (lfn->attributes == FAT_LFN) {
        
        // Get the Sequence Number
        // Bit 6 (0x40) is the "Last Logical Entry" flag. Mask it off.
        uint8_t seq = lfn->sequence & 0x1F;
        
        // If seq is deleted (0xE5) or invalid (0), stop.
        if (lfn->sequence == 0xE5 || seq == 0) break;

        // Capture Checksum (Only need to do this once, they are all the same)
        if (out_checksum) *out_checksum = lfn->checksum;

        // Calculate Buffer Offset
        // Each entry holds 13 characters.
        // Seq 1 = Offset 0, Seq 2 = Offset 13, etc.
        int offset = (seq - 1) * 13;

        // Extract Characters (13 chars total per entry)
        _lfn_copy_name(lfn->name1, name_buffer, 5, offset);
        _lfn_copy_name(lfn->name2, name_buffer, 6, offset + 5);
        _lfn_copy_name(lfn->name3, name_buffer, 2, offset + 11);

        lfn++;
    }

    return (fat_directory_entry_t*)lfn;
}

uint8_t lfn_checksum(const uint8_t* pFcbName) {
    int i;
    uint8_t sum = 0;
    for (i = 11; i; i--, pFcbName++)
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *pFcbName;
    return sum;
}

void fat32_parse_sfn(fat_directory_entry_t* entry, char* out){
    int pos = 0;
    char* sfn = entry->name;
    uint8_t nt_flags = entry->winnt_flags;

    // Base name: 8 chars max
    for (int i = 0; i < 8 && sfn[i] != ' '; ++i) {
        char c = sfn[i];
        if (nt_flags & (1 << 3)) c = tolower(c);
        out[pos++] = c;
    }

    // Extension: only add if not all spaces
    bool has_ext = false;
    for (int i = 0; i < 3; ++i) {
        if (sfn[8 + i] != ' ') {
            has_ext = true;
            break;
        }
    }

    if (has_ext) {
        out[pos++] = '.';
        for (int i = 0; i < 3 && sfn[8 + i] != ' '; ++i) {
            char c = sfn[8 + i];
            if (nt_flags & (1 << 4)) c = tolower(c);
            out[pos++] = c;
        }
    }

    out[pos] = '\0';
}


// Helper to check if we can skip LFN
bool _is_valid_8_3(const char* name) {
    int len = strlen(name);
    if (len > 12) return false; 

    bool base_has_upper = false;
    bool base_has_lower = false;
    bool ext_has_upper = false;
    bool ext_has_lower = false;

    int dot_pos = -1;
    int i = 0;

    for (; i < len; i++) {
        char c = name[i];
        if (c == '.') {
            dot_pos = i;
            break; 
        }

        // Validate Character
        bool is_upper = (c >= 'A' && c <= 'Z');
        bool is_lower = (c >= 'a' && c <= 'z');
        bool is_digit = (c >= '0' && c <= '9');

        bool is_special = (strchr("$%'-_@~`!(){}^#&", c) != nullptr);

        if (!is_upper && !is_lower && !is_digit && !is_special) return false;

        if (is_upper) base_has_upper = true;
        if (is_lower) base_has_lower = true;
    }

    int base_len = (dot_pos == -1) ? len : dot_pos;
    if (base_len == 0 || base_len > 8) return false;

    if (base_has_upper && base_has_lower) return false;


    if (dot_pos != -1) {
        // Skip the dot
        i++; 
        int ext_len = 0;

        for (; i < len; i++) {
            char c = name[i];
            
            // Validate Character
            bool is_upper = (c >= 'A' && c <= 'Z');
            bool is_lower = (c >= 'a' && c <= 'z');
            bool is_digit = (c >= '0' && c <= '9');
            bool is_special = (strchr("$%'-_@~`!(){}^#&", c) != nullptr);

            if (!is_upper && !is_lower && !is_digit && !is_special) return false;

            if (is_upper) ext_has_upper = true;
            if (is_lower) ext_has_lower = true;
            
            ext_len++;
        }

        // Check Extension Length (Must be 0-3 chars)
        if (ext_len > 3) return false;

        // Check Extension Consistency
        if (ext_has_upper && ext_has_lower) return false;
    }

    // If we passed all checks, it's valid 8.3 (possibly with NT flags)
    return true;
}

// @brief Formats name into 8.3 buffer and returns the NT Reserved byte
uint8_t _format_8_3(const char* input, char* output) {
    memset(output, ' ', 11);
    uint8_t nt_resv = 0;
    
    int i = 0, j = 0;
    while(input[i] != 0 && input[i] != '.' && j < 8) {
        char c = input[i];
        
        if (c >= 'a' && c <= 'z') {
            nt_resv |= FAT_LC_BASENAME;
            c -= 32;
        }
        
        output[j++] = c;
        i++;
    }
    
    // --- Find Extension ---
    const char* dot = strchr(input, '.');
    if (dot) {
        dot++; // Skip dot
        int k = 8;
        while(*dot != 0 && k < 11) {
            char c = *dot;

            
            if (c >= 'a' && c <= 'z') {
                nt_resv |= FAT_LC_EXTENSION;
                c -= 32;
            }

            output[k++] = c;
            dot++;
        }
    }

    return nt_resv;
}

void _generate_sfn(const char* long_name, char* out_sfn) {
    memset(out_sfn, ' ', 11);
    int j = 0;
    for (int i = 0; long_name[i] != 0 && j < 6; i++) {
        char c = long_name[i];
        if (c == '.') continue;
        if (c >= 'a' && c <= 'z') c -= 32; // Always uppercase for SFN
        out_sfn[j++] = c;
    }
    out_sfn[6] = '~';
    out_sfn[7] = '1'; // TODO: Handle collisions loop here eventually
    
    const char* dot = strrchr(long_name, '.');
    if (dot) {
        dot++;
        int k = 8;
        for (int i = 0; dot[i] != 0 && k < 11; i++) {
            char c = dot[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out_sfn[k++] = c;
        }
    }
}