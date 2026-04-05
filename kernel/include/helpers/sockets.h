#pragma once
#include <filesystem/vfs/vfs.h>
#include <scheduling/spinlock/spinlock.h>

int connect_socket_to_pipes(vnode_t *socket, vnode_t *inbound_pipe, vnode_t *outbound_pipe);