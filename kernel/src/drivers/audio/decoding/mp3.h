#pragma once
#include "minimp3.h"
#include "minimp3_ex.h"
#include <stdint.h>
#include <stddef.h>
#include "../../../filesystem/FAT32.h"
#include "../../storage/volume/volume.h"
#include "../HDA.h"
#include "../../../paging/PageFrameAllocator.h"

void playMP3(DirEntry* fileD);
void playMP3(uint8_t*, size_t);