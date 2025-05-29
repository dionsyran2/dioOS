#include "volume.h"

namespace VolumeSpace
{
    Volume volumes[255];
    size_t numOfVolumes = 0;

    Filesystem FindFS(DISK_DRIVE *drv, uint64_t lbaOffset)
    {
        uint8_t *bpb = (uint8_t *)GlobalAllocator.RequestPage();
        memset(bpb, 0, 0x1000);
        drv->Read(lbaOffset, 1, bpb);

        char identifier[8];
        memcpy(&identifier, bpb + 0x052, 8);
        identifier[8] = '\0';
        for (int i = 0; i < 5; i++)
        {
            if (identifier[i] != "FAT32"[i])
            {
                return UNKNOWN;
            }
        }
        return FS_FAT32;
    }

    bool CreateVolume(DISK_DRIVE *drv, uint64_t lbaOffset)
    {
        if (FindFS(drv, lbaOffset) == FS_FAT32)
        {
            FAT32 *fs = new FAT32;
            fs->Init(drv, lbaOffset);

            volumes[numOfVolumes].assignedLetter = 'C'; // make an algorithm for these stuff
            volumes[numOfVolumes].drive = drv;
            volumes[numOfVolumes].fs = (void *)fs;
            volumes[numOfVolumes].FSType = FS_FAT32;
            volumes[numOfVolumes].lbaStart = lbaOffset;
            volumes[numOfVolumes].volumeID = numOfVolumes;

            char *volName = fs->partName();
            memcpy(volumes[numOfVolumes].volumeName, volName, 12);
            numOfVolumes++;

            free(volName);
            kprintf(0xFF4D00, "[Volume] ");
            kprintf("FAT32 volume initialized\n");
        }
    }

    Volume* getVolume(char letter){
        for (int i = 0; i < numOfVolumes; i++){
            if (volumes[i].assignedLetter == letter){
                return &volumes[i];
            }
        }
    }

    DirEntry* OpenFile(char* path){
        int cnt = 0;
        char** ext = split(path, '\\', cnt);
        char drive = ext[0][0];
        Volume* volume = getVolume(drive);
        if (volume == nullptr) return nullptr;

        DirEntry* last = nullptr;

        for (int i = 1; i < cnt; i++){
            DirEntry* prev = last; 
            last = volume->OpenFile(ext[i], prev);

            if (prev != nullptr) volume->CloseFile(prev);
        }

        return last;
    }

    void CloseFile(DirEntry* entry){
        entry->volume->CloseFile(entry);
    }
}

DirectoryList *Volume::listDir(DirEntry *dir)
{
    if (FSType == VolumeSpace::FS_FAT32)
    {
        FAT32 *fat = (FAT32 *)fs;
        uint8_t* fsdir = nullptr;
        if (dir){
            fsdir = (uint8_t*)dir->fs_entry;
        }
        DirEntries *entries = fat->ListDir(fsdir); // GET DIRECTORIES!!!
        DirectoryList *list = (DirectoryList *)malloc(sizeof(DirectoryList));
        list->numOfEntries = 0;
        for (int i = 0; i < entries->numOfEntries; i++)
        {
            if (entries->entries[i]->attributes == 0x0F) continue;
            list->entries[list->numOfEntries].attributes = entries->entries[i]->attributes;
            list->entries[list->numOfEntries].creationDate = entries->entries[i]->creationDate;
            list->entries[list->numOfEntries].creationTime = entries->entries[i]->creationTime;
            list->entries[list->numOfEntries].LastAccessed = entries->entries[i]->LastAccessed;
            list->entries[list->numOfEntries].lastModificationTime = entries->entries[i]->lastModificationTime;
            list->entries[list->numOfEntries].lastModificationDate = entries->entries[i]->lastModificationDate;
            list->entries[list->numOfEntries].size = entries->entries[i]->size;
            list->entries[list->numOfEntries].volume = this;
            memcpy(list->entries[list->numOfEntries].name, entries->entries[i]->name, 11);
            list->entries[list->numOfEntries].name[11] = '\0';


            list->entries[list->numOfEntries].fs_entry = malloc(sizeof(DirectoryEntry));
            memcpy(list->entries[list->numOfEntries].fs_entry, entries->entries[i], sizeof(DirectoryEntry));
            uint32_t chk = 0;
            size_t off = 0;
            if (i - 1 >= 0)
            {
                if (entries->entries[i - 1]->attributes == 0x0F)
                {
                    LNEntry *lne = (LNEntry *)entries->entries[i - 1];
                    chk = lne->checksum;
                    int lnOff = 1;
                    while (true)
                    {
                        int x = 0;
                        LNEntry *ln = (LNEntry *)entries->entries[i - lnOff];
                        if (ln->checksum != chk)
                            break;
                        while (x < 10)
                        {
                            list->entries[list->numOfEntries].name[off] = (uint8_t)ln->first5Char[x];
                            x += 2;
                            off++;
                        }
                        x = 0;
                        while (x < 12)
                        {
                            list->entries[list->numOfEntries].name[off] = (uint8_t)ln->next6Char[x];
                            x += 2;
                            off++;
                        }
                        x = 0;
                        while (x < 4)
                        {
                            list->entries[list->numOfEntries].name[off] = (uint8_t)ln->last2Char[x];
                            x += 2;
                            off++;
                        }
                        lnOff++;
                    }
                    list->entries[list->numOfEntries].name[off] = '\0';
                }
            }

            list->numOfEntries++;
        }
        return list;
    }
}

DirEntry *Volume::OpenFile(char* filename, DirEntry* dir){
    if (FSType == VolumeSpace::FS_FAT32)
    {
        DirectoryList* list = listDir(dir);
        DirEntry* entry = (DirEntry*)malloc(sizeof(DirEntry));
        memset(entry, 0, sizeof(DirEntry));
        for (int i = 0; i < list->numOfEntries; i++){
            if (list->entries[i].attributes != 0x20 && list->entries[i].attributes != 0x10) continue;
            if (strcmp(list->entries[i].name, filename) == 0){
                memcpy((void*)entry, &list->entries[i], sizeof(DirEntry));
            }else{
                free(list->entries[i].fs_entry);
            }
        }
        free(list);
        if ((uint8_t)entry->name[0] == 0){
            free(entry);
            return nullptr;
        }
        return entry;
    }
}

void *Volume::LoadFile(DirEntry* file){
    if (FSType == VolumeSpace::FS_FAT32)
    {
        FAT32 *fat = (FAT32*)fs;
        return (void*)fat->LoadFile((uint8_t*)file->fs_entry);
    }
}

void Volume::CloseFile(DirEntry* file){
    free(file->fs_entry);
    free(file);
}