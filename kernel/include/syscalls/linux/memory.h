#pragma once


#define MAP_SHARED       0x01  // Share changes
#define MAP_PRIVATE      0x02  // Changes are private (copy-on-write)
#define MAP_FIXED        0x10  // Force use of addr
#define MAP_ANONYMOUS    0x20  // Not backed by file (fd must be -1)

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
