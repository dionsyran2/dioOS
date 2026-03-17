#pragma once
#include <stdint.h>
#include <stddef.h>
#include <local.h>
#include <kerrno.h>
#include <syscalls/syscall_ids.h>

typedef long (*syscall_function)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

typedef struct syscall_definition{
    syscall_function entry;
    int number;
} syscall_definition_t;

#define SYSCALL_SECTION_NAME ".syscalls"

#define REGISTER_SYSCALL(id, function) \
    const syscall_definition_t __reg_entry_##id \
        __attribute__((section(SYSCALL_SECTION_NAME))) \
        __attribute__((aligned(8))) \
        __attribute__((used)) \
        = { (syscall_function)function, id }

extern syscall_definition_t __start_syscalls[];
extern syscall_definition_t __stop_syscalls[];

struct iovec{
    void *iov_base;	/* Pointer to data.  */
    size_t iov_len;	/* Length of data.  */
};
void setup_syscalls();