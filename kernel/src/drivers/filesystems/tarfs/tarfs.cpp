#include <drivers/filesystems/tarfs/tarfs.h>
#include <drivers/filesystems/ramfs/ramfs.h>
#include <vfs/vnode_attributes.h>
#include <string.h>
#include <memory.h>
#include <kstdio.h>

static uint64_t parse_octal(const char *str, size_t size) {
    uint64_t n = 0;
    for (size_t i = 0; i < size && str[i] >= '0' && str[i] <= '7'; i++) {
        n = (n << 3) | (str[i] - '0'); // Multiply by 8 and add digit
    }
    return n;
}

namespace tarfs {
    void init_tarfs(void* file, size_t file_size){
        dentry_t *root = ramfs::create_fs();
        vfs::mount(vfs::get_root(), root);
        
        size_t offset = 0;

        while (offset < file_size){
            ustar_header_t *header = (ustar_header_t *)((uint64_t)file + offset);

            if (header->name[0] == '\0') {
                break;
            }

            void *data = (void *) ((uint64_t)file + offset + 512);
            size_t payload_size = parse_octal(header->size, 12);
            size_t mode = parse_octal(header->mode, 8);
            
            size_t uid = parse_octal(header->uid, 8);
            size_t gid = parse_octal(header->gid, 8);
            size_t mtime = parse_octal(header->mtime, 12);

            // Set the permissions
            vnode_attributes_t attrs = {
                .valid = VNODE_ATTR_MODE | VNODE_ATTR_UID | VNODE_ATTR_GID | VNODE_ATTR_MTIME,
                .mode = mode,
                .uid = uid,
                .gid = gid,
                .mtime = mtime,
            };

            char full_path[512];
            full_path[0] = '/';
            full_path[1] = '\0';

            if (header->prefix[0] != '\0') {
                strncat(full_path, header->prefix, sizeof(header->prefix));
                strcat(full_path, "/");
            }
            strncat(full_path, header->name, sizeof(header->name));

            //serialf("initfs: Creating %s\n\r", full_path);

            if (header->typeflag == '\0' || header->typeflag == '0'){
                /* Its a file */
                int r = vfs::mkfile(full_path, mode);
                vnode_t *node = vfs::resolve_path(full_path);

                if (node){
                    node->write(data, payload_size, 0);
                    node->set_attributes(&attrs);

                    node->close();
                }
            } else if (header->typeflag == '5') {
                /* Its a directory */
                int len = strlen(full_path);
                if (full_path[len - 1] == '/'){
                    full_path[len - 1] = '\0';
                }

                int r = vfs::mkdir(full_path, mode);
                vnode_t *node = vfs::resolve_path(full_path);

                if (node){
                    node->set_attributes(&attrs);
                    node->close();
                }
            } else if (header->typeflag == '2') {
                int len = strlen(full_path);
                if (full_path[len - 1] == '/'){
                    full_path[len - 1] = '\0';
                }

                char linkname[101];
                memcpy(linkname, header->linkname, 100);
                linkname[100] = '\0';

                int r = vfs::mklink(full_path, linkname);
            }

            offset += 512 + ROUND_UP(payload_size, 512);
        }
    }
}