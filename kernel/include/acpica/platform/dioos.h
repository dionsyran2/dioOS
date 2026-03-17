#pragma once
#include <stddef.h>
#include <acpica/acoutput.h>
#include <CONFIG.h>

#define ACPI_MACHINE_WIDTH 64
#define ACPI_USE_NATIVE_MATH64
#define ACPI_USE_NATIVE_DIVIDE

#define ACPI_USE_SYSTEM_CLIBRARY 0 

#undef ACPI_DEBUG_DEFAULT

#ifdef ACPICA_FULL_LOGGING
#define ACPI_DEBUG_DEFAULT          ACPI_NORMAL_DEFAULT | ACPI_DEBUG_ALL
#else
#define ACPI_DEBUG_DEFAULT          ACPI_NORMAL_DEFAULT
#endif

#define ACPI_DEBUG_OUTPUT


#define ACPI_USE_LOCAL_CACHE
#define ACPI_MUTEX_TYPE ACPI_OSL_MUTEX





#define BOOLEAN UINT8

#define ACPI_OFFSET(d, f)   offsetof(d, f)

#undef isupper
#undef islower
#undef isdigit
#undef isxdigit
#undef isspace
#undef isprint
#undef isalpha
#undef isalnum
#undef toupper
#undef tolower

#define isupper(c)  ((c) >= 'A' && (c) <= 'Z')
#define islower(c)  ((c) >= 'a' && (c) <= 'z')
#define isdigit(c)  ((c) >= '0' && (c) <= '9')
#define isspace(c)  ((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')
#define isprint(c)  ((c) >= ' ' && (c) <= '~')
#define isxdigit(c) (((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F') || isdigit(c))
#define isalpha(c)  (isupper(c) || islower(c))
#define isalnum(c)  (isalpha(c) || isdigit(c))
#define toupper(c)  ((islower(c)) ? ((c) - 32) : (c))
#define tolower(c)  ((isupper(c)) ? ((c) + 32) : (c))