#pragma once
#include <stdint.h>
#include <stddef.h>
#include <syscalls/syscalls.h>
#include <syscalls/files/fcntl.h>
#include <alloca.h>
#include <memory/heap.h>
#include <memory.h>
#include <cstr.h>
#include <math.h>

// don't even bother, its a mess... but it works
// and you know what they say!
inline void fix_path(const char* orig, char* buffer, size_t size, task_t* ctask){
    char original[0x1000];
    char *r = ctask->read_string((void*)orig, 0x1000);
    strcpy(original, r);
    free(r);

    char* pathname = original;

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->cwd->node);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        free(cwd);
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->cwd->node);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        free(cwd);

        // serialf("redirected path to %s\n\r", pathname);
    }

    int to_copy = min(size, strlen(pathname) + 1);
    memcpy(buffer, pathname, to_copy);

    buffer[to_copy - 1] = '\0';
    //serialf("redirected path from %s to %s\n\r", original, buffer);
}

inline void kfix_path(const char* original, char* buffer, size_t size, task_t* ctask){
    char* pathname = (char*)original;

    if (pathname[0] == '.' && (pathname[1] == '/' || pathname[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(ctask->cwd->node);

        // Remove leading './'
        char* rel = pathname + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        free(cwd);
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/'){
        char* cwd = vfs::get_full_path_name(ctask->cwd->node);

         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        free(cwd);

        // serialf("redirected path to %s\n\r", pathname);
    }

    int to_copy = min(size, strlen(pathname) + 1);
    memcpy(buffer, pathname, to_copy);

    buffer[to_copy - 1] = '\0';
    //serialf("redirected path from %s to %s\n\r", original, buffer);
}

inline void fix_path(int dirfd, const char* orig, char* buffer, size_t size, task_t* ctask){
    fd_t* dir = ctask->get_fd(dirfd);

    char original[0x1000];
    char *r = ctask->read_string((void*)orig, 0x1000);
    strcpy(original, r);
    free(r);

    char* pathname = original;

    if (original[0] == '.' && (original[1] == '/' || original[1] == '\0')) {
        char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->cwd->node : dir->node);


        // Remove leading './'
        char* rel = (char*)original + 1;
        if (*rel == '/') rel++;

        // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(rel);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, rel);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }else if (pathname[0] != '/' && (dir != nullptr || dirfd == AT_FDCWD)){
            char* cwd = vfs::get_full_path_name(dirfd == AT_FDCWD ? ctask->cwd->node : dir->node);


         // Calculate the size with space for '/' and null terminator
        size_t cwd_len = strlen(cwd);
        size_t rel_len = strlen(pathname);
        bool need_slash = (cwd[cwd_len - 1] != '/' && rel_len > 0);
        size_t total_len = cwd_len + (need_slash ? 1 : 0) + rel_len + 1;

        char* path = (char*)alloca(total_len);
        strcpy(path, cwd);
        if (need_slash) strcat(path, "/");
        strcat(path, pathname);

        // Optional cleanup: remove trailing slash unless root
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';

        pathname = path;
        // serialf("redirected path to %s\n\r", pathname);
    }

    int to_copy = min(size, strlen(pathname) + 1);
    memcpy(buffer, pathname, to_copy);

    buffer[to_copy - 1] = '\0';
    //serialf("redirected path from %s to %s\n\r", original, buffer);
}


inline void split_path(char* fullpath, char*& parent, char*& name) {
    size_t len = strlen(fullpath);
    int last_slash = -1;

    // Find the last '/'
    for (int i = len - 1; i >= 0; i--) {
        if (fullpath[i] == '/') {
            last_slash = i;
            break;
        }
    }

    if (last_slash <= 0) {
        // No slash or root only: everything is in name
        parent = (char*)malloc(2);
        strcpy(parent, "/");

        name = (char*)malloc(len + 1);
        strcpy(name, fullpath[0] == '/' ? fullpath + 1 : fullpath);
    } else {
        parent = (char*)malloc(last_slash + 1); // +1 for null
        memcpy(parent, fullpath, last_slash);
        parent[last_slash] = '\0';

        size_t name_len = len - last_slash - 1;
        name = (char*)malloc(name_len + 1);
        memcpy(name, fullpath + last_slash + 1, name_len);
        name[name_len] = '\0';
    }
}
