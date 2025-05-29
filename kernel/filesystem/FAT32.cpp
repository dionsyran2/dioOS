#include "FAT32.h"
FAT32* FS;

const char* charToConstCharPtr__(char c) {
    static char buffer[2];
    buffer[0] = c;
    buffer[1] = '\0';
    return buffer;
}


void checkFATValues(uint64_t entry) {
    if (entry == 0x00000000) {
        kprintf("Cluster is free/");
    } else if (entry >= 0x0FFFFFF8 && entry <= 0x0FFFFFFF) {
        kprintf("End of file chain/");
    } else if (entry == 0x0FFFFFF7) {
        kprintf("Bad cluster/");
    } else if (entry >= 0x0FFFFFF0 && entry <= 0x0FFFFFF6) {
        kprintf("Reserved cluster/");
    } else {
        kprintf("Next cluster in chain: ");
        kprintf(toString(entry));
        kprintf("//");
    }
}

uint32_t FAT32::calculateSector(uint32_t cluster){
    uint16_t reservedSectors = *((uint32_t*)(bpb + 0x0E));
    uint32_t sectorsPerFAT = *((uint32_t*)(bpb + 36));
    uint32_t numOfFats = *((uint32_t*)(bpb + 0x10));
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));

    uint8_t sectorsPerCluster = *((uint32_t*)(bpb + 0x0D));
    return (((cluster - 2) * sectorsPerCluster) + reservedSectors + (numOfFats * sectorsPerFAT)) + offset;
}

uint64_t FAT32::getNextCluster(uint64_t cluster){
    uint16_t reservedSectors = *((uint32_t*)(bpb + 0x0E));
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));
    uint8_t* FAT_table = (uint8_t*)GlobalAllocator.RequestPage();
    memset(FAT_table, 0, 0x1000);
    unsigned int fat_offset = cluster * 4;
    unsigned int fat_sector = reservedSectors + (fat_offset / bytesPerSector);
    unsigned int ent_offset = fat_offset % bytesPerSector;

    //at this point you need to read from sector "fat_sector" on the disk into "FAT_table".
    port->Read(fat_sector + offset, 1, FAT_table);
    //remember to ignore the high 4 bits.
    unsigned int table_value = *(unsigned int*)&FAT_table[ent_offset];
    table_value &= 0x0FFFFFFF;
    
    if (table_value == 0x00000000) {
        //Cluster Free
    } else if (table_value >= 0x0FFFFFF8 && table_value <= 0x0FFFFFFF) {
        return 0; //End of chain
    } else if (table_value == 0x0FFFFFF7) {
        return 0; //Bad Cluster
    } else if (table_value >= 0x0FFFFFF0 && table_value <= 0x0FFFFFF6) {
        return 0; //Reserved Cluster
    } else {
        return table_value; //Next Cluster
    }
}
char* FAT32::partName(){
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));
    uint8_t sectorsPerCluster = *((uint32_t*)(bpb + 0x0D));
    uint32_t clusterSize = sectorsPerCluster * bytesPerSector;
    char* name = (char*)malloc(sizeof(char) * 12);
    memcpy(name, "NO NAME", sizeof(char) * 7);
    for (uint32_t x = 0; x < clusterSize / sizeof(DirectoryEntry); x++) {
        DirectoryEntry* entry = (DirectoryEntry*)(rootDir + (x * sizeof(DirectoryEntry)));
        if (entry == NULL) break;
        if (entry->attributes == 0x08){
            memcpy(name, entry->name, sizeof(char) * 11);
            name[11] = '\0';
            break;
        }
    }
    return name;
}
DirEntries* FAT32::ListDir(uint8_t* dir){
    uint32_t rootCluster = *((uint32_t*)(bpb + 0x02C));
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));
    uint8_t sectorsPerCluster = *((uint32_t*)(bpb + 0x0D));
    uint32_t rootClusterSector = calculateSector(rootCluster);
    //kprintf("rootclustersector: %d\n", rootClusterSector);
    uint32_t clusterSize = sectorsPerCluster * bytesPerSector;

    if(dir == nullptr){
        dir = rootDir;
    }else{
        DirectoryEntry* directory = (DirectoryEntry*)dir;
        uint16_t cluster = directory->firstClusterLow;
        uint32_t dirSector = calculateSector(cluster);
        dir = (uint8_t*)GlobalAllocator.RequestPages((clusterSize / 0x1000) + 1); //memory leak to be fixed later
        port->Read(dirSector, sectorsPerCluster, dir);
    }
    DirEntries* dirEntries = (DirEntries*)GlobalAllocator.RequestPage();
    memset(dirEntries, 0, 0x1000);
    for (uint32_t x = 0; x < clusterSize / sizeof(DirectoryEntry); x++) {
        DirectoryEntry* entry = (DirectoryEntry*)(dir + (x * sizeof(DirectoryEntry)));
        if (entry == NULL) continue;
        if (entry->name[0] == 0x00) break;
        if ((uint8_t)entry->name[0] == 0xE5) continue; //Deleted entry
        /*if (entry->name[0] == '.'){
            if(entry->name[1] == '.' || entry->name[1] == ' '){
                if(entry->name[2] == ' '){
                    if(entry->name[3] == ' '){
                        continue;
                    }
                }
            }
        }*/
        //if (entry->attributes == 0x0f) continue; //Long name!
        dirEntries->entries[dirEntries->numOfEntries] = entry;
        dirEntries->numOfEntries++;
    }
    return dirEntries;
}


uint8_t* FAT32::FindFile(const char* filename, uint8_t* dir){
    if(dir == nullptr){
        dir = rootDir;
    }else{
        DirectoryEntry* directory = (DirectoryEntry*)dir;
        uint16_t cluster = directory->firstClusterLow;
        uint32_t dirSector = calculateSector(cluster);
        dir = (uint8_t*)GlobalAllocator.RequestPage();
        port->Read(dirSector, 1, dir);
    }

    uint32_t rootCluster = *((uint32_t*)(bpb + 0x02C));
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));
    uint8_t sectorsPerCluster = *((uint32_t*)(bpb + 0x0D));
    uint32_t rootClusterSector = calculateSector(rootCluster);
    uint32_t clusterSize = sectorsPerCluster * bytesPerSector;

    for (uint32_t x = 0; x < clusterSize / sizeof(DirectoryEntry); x++) {
        DirectoryEntry* entry = (DirectoryEntry*)(dir + (x * sizeof(DirectoryEntry)));
        if (entry == NULL) break;
        if (entry->name[0] == 0x00) break;
        if (entry->name[0] == '.'){
            if(entry->name[1] == '.' || entry->name[1] == ' '){
                if(entry->name[2] == ' '){
                    if(entry->name[3] == ' '){
                        continue;
                    }
                }
            }
        }
        /*if (entry->attributes == 0x10){
            //Sub Directory
            uint16_t cluster = entry->firstClusterLow;
            uint32_t dirSector = calculateSector(cluster);
            uint8_t* dirBuffer = (uint8_t*)GlobalAllocator.RequestPage();
            port->Read(dirSector, 1, dirBuffer);
            uint8_t* val = FindFile(filename, dirBuffer);

            if (val != 0){
                return val;
            };
        }*///Probably shouldn't look in sub directories because there could be a file with the same name somewhere else.
        if (entry->attributes == 0x0f) continue; //Long name!
        bool ret = true;
        for (int i = 0; i < 11; i++){
            if (entry->name[i] != filename[i]){
                ret = false;
            }
        }
        if (ret) return (dir + (x * sizeof(DirectoryEntry)));
    }
    return 0;
}

uint8_t* FAT32::LoadFile(uint8_t* DirEntry){
    
    DirectoryEntry* entry = (DirectoryEntry*)DirEntry;

    uint32_t cluster = (entry->firstCluster << 16) | entry->firstClusterLow;
    uint8_t sectorsPerCluster = *((uint8_t*)(bpb + 0x0D));
    //kprintf("SECTORS PER CLUSTERS %d\n", sectorsPerCluster);
    uint16_t bytesPerSector = *((uint16_t*)(bpb + 0x0B));
    
    uint8_t* fileBuffer = (uint8_t*)GlobalAllocator.RequestPages(((entry->size + 0x1000 - 1) / 0x1000) + 1);
    uint32_t bytesRead = 0;
    uint32_t totalBytesToRead = entry->size;


    do {
        uint32_t sector = calculateSector(cluster);

        uint32_t bytesToRead = sectorsPerCluster * bytesPerSector;
        if (bytesRead + bytesToRead > totalBytesToRead) {
            bytesToRead = totalBytesToRead - bytesRead;
        }


        port->Read(sector, sectorsPerCluster, fileBuffer + bytesRead);
        bytesRead += bytesToRead;

        cluster = getNextCluster(cluster);

    } while (cluster != 0 && cluster < 0xFFFFFF8);
    /*uint8_t* buff = (uint8_t*)malloc(entry->size);
    memcpy(buff, fileBuffer, entry->size); // transfer it to the heap
    GlobalAllocator.FreePages(fileBuffer, ((entry->size + 0x1000 - 1) / 0x1000) + 1);*/
    
    return fileBuffer;
}

void FAT32::Init(DISK_DRIVE* port_, uint64_t off) {
    //BPB And INIT!
    port = port_;
    offset = off;
    bpb = (uint8_t*)GlobalAllocator.RequestPage();
    memset(bpb, 0, 0x1000);
    port->Read(offset, 4, bpb);
    
    char identifier[8];
    memcpy(&identifier, bpb + 0x052, 8);
    identifier[8] = '\0';
    bool isfat = true;
    for (int i=0; i < 5; i++){
        if (identifier[i] != "FAT32"[i]){
            isfat = false;
            return;
        }
    }


    uint16_t reservedSectors = *((uint32_t*)(bpb + 0x0E));
    uint32_t sectorsPerFAT = *((uint32_t*)(bpb + 36));
    uint32_t numOfFats = *((uint32_t*)(bpb + 0x10));
    uint16_t bytesPerSector = *((uint32_t*)(bpb + 0x0B));
    //kprintf("Reserved Sectors: %d, Sectors Per FAT: %d, Number Of Fats: %d, bps: %d\n", reservedSectors, sectorsPerFAT, numOfFats, bytesPerSector);

    //Reading FAT
    const uint32_t FAT_ENTRY_SIZE = 4;
    uint16_t fatStartSector = reservedSectors;
    fatBuffer = GlobalAllocator.RequestPages(((sectorsPerFAT * bytesPerSector)/4096) + 1);
    memset(fatBuffer, 0, (((sectorsPerFAT * bytesPerSector)/4096) + 1) * 0x1000);
    //kprintf(0xFF0000, "1");
    port->Read(offset + fatStartSector, sectorsPerFAT, fatBuffer);
    //kprintf(0xFF0000, "2");
    uint32_t rootCluster = *((uint32_t*)(bpb + 0x02C));
    uint8_t sectorsPerCluster = *((uint32_t*)(bpb + 0x0D));

    uint32_t rootClusterSector = reservedSectors + (numOfFats * sectorsPerFAT) + ((rootCluster - 2) * sectorsPerCluster);
    uint32_t clusterSize = sectorsPerCluster * bytesPerSector;

    rootDir = (uint8_t*)GlobalAllocator.RequestPages((clusterSize / 0x1000) + 1);
    memset(rootDir, 0, 0x1000);

    //kprintf(0xFFFF00, "%d - %d - %d - %d - %d - %d", rootClusterSector + offset, numOfFats, sectorsPerFAT, rootCluster, sectorsPerCluster, offset);
    port->Read(rootClusterSector + offset, sectorsPerCluster, rootDir);
    //kprintf(0xFF0000, "4");

    if(FindFile("KERNEL  ELF", rootDir)){
        isBoot = true;
    };
    //CLEANUP!!!
}
