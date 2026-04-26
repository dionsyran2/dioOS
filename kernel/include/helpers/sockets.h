#pragma once
#include <filesystem/vfs/vfs.h>
#include <syscalls/sockets.h>
#include <scheduling/spinlock/spinlock.h>


int connect_socket_structs(unix_socket_t *s1, unix_socket_t *s2, socket_transfer *t1, socket_transfer* t2);