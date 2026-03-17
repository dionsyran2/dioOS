// SAFETY: Prevent GCC from including <mm_malloc.h>, which drags in the 
// system <stdlib.h> and conflicts with our kernel's malloc/free.
#ifndef _MM_MALLOC_H_INCLUDED
#define _MM_MALLOC_H_INCLUDED
#endif
#include <immintrin.h>