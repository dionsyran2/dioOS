#pragma once

#include <cstr.h>
inline static bool split_path(const char *path, char *parent_path, char *target_name) {
    if (!path || path[0] == '\0') return false;

    // Find the last slash
    const char *last_slash = strrchr(path, '/');

    if (!last_slash) {
        // Relative path without slashes (e.g., "foo")
        strcpy(parent_path, ".");
        strcpy(target_name, path);
    } else if (last_slash == path) {
        // Target lives in root directory (e.g., "/foo")
        strcpy(parent_path, "/");
        strcpy(target_name, last_slash + 1);
    } else {
        // Nested path (e.g., "/a/b/c")
        size_t parent_len = last_slash - path;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strcpy(target_name, last_slash + 1);
    }

    // Edge case: path ended with a trailing slash (e.g., "/a/b/")
    if (target_name[0] == '\0') return false;

    return true;
}