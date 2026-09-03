#include <drivers/filesystems/common.h>
#include <structures/lists/linked_list.h>

kstd::linked_list_t<filesystem_registration_t*> filesystem_list;

void register_filesystem(filesystem_registration_t *fs){
    filesystem_list.lock();

    filesystem_list.add(fs);

    filesystem_list.unlock();
}

filesystem_t::filesystem_t(vnode_t *disk){

};

filesystem_t *find_filesystem(vnode_t *disk){
    filesystem_list.lock();

    for (int i = 0; i < filesystem_list.size(); i++){
        filesystem_registration_t *reg = filesystem_list.get(i);

        if (reg->has_valid_fs(disk)){
            filesystem_list.unlock();

            return reg->create_fs(disk);
        }
    }

    filesystem_list.unlock();
    return nullptr;
}