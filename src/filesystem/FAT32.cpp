#include "FAT32.h"
#include "../BasicRenderer.h"
#include "../cstr.h"
#include "../paging/PageFrameAllocator.h"

const char* charToConstCharPtr__(char c) {
    static char buffer[2]; // 1 char + 1 for null terminator
    buffer[0] = c;         // Set the character
    buffer[1] = '\0';      // Null terminate the string
    return buffer;         // Return the pointer to the buffer
}

int strncmp(const char* str1, const char* str2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (str1[i] != str2[i]) {
            return 1;  // Not equal
        }
        // If we reach a null-terminator before `n`, stop comparison
        if (str1[i] == '\0' || str2[i] == '\0') {
            break;
        }
    }
    return 0;  // Strings are equal
}
int strcmpB(const char* str1, const char* str2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (str1[i] != str2[i]) {
            return 1;  // Not equal
        }
        // If we reach a null-terminator before `n`, stop comparison
        if (str1[i] == '\0' || str2[i] == '\0') {
            break;
        }
    }
    return 0;  // Strings are equal
}
FAT32_DirectoryEntry* FAT32::FindFile(const char* filename, FAT32_DirectoryEntry* directory) {
    uint32_t directorySector = GetSectorForCluster(directory->firstClusterLow);
    uint32_t sectorsPerCluster = bpb->sectorsPerCluster;
    
    FAT32_DirectoryEntry* entries = (FAT32_DirectoryEntry*)GlobalAllocator.RequestPages(sectorsPerCluster); 
    
    // Read the directory from disk (assume one cluster)
    port->Read(directorySector, sectorsPerCluster, entries);
    
    // Loop through the directory entries
    for (int i = 0; i < 16 * sectorsPerCluster; i++) { // 512 bytes per sector, 32 bytes per entry
        if (strncmp(entries[i].name, filename, 11) == 0) {
            return &entries[i]; // Return a pointer to the directory entry
        }
    }
    
    return nullptr; // File not found
}

void FAT32::LoadFileData(FAT32_DirectoryEntry* file, void* buffer) {
    uint32_t currentCluster = file->firstClusterLow;
    uint32_t bytesPerSector = bpb->bytesPerSector;
    uint32_t sectorsPerCluster = bpb->sectorsPerCluster;
    
    uint32_t bufferOffset = 0;
    
    while (currentCluster < 0x0FFFFFF8) { // 0x0FFFFFF8 means end of file in FAT32
        uint32_t sector = GetSectorForCluster(currentCluster);
        
        // Read the current cluster into the buffer
        port->Read(sector, sectorsPerCluster, (uint8_t*)buffer + bufferOffset);
        bufferOffset += bytesPerSector * sectorsPerCluster;
        
        // Move to the next cluster
        currentCluster = ReadFATEntry(currentCluster);
    }
}

uint32_t FAT32::GetSectorForCluster(uint32_t cluster) {
    uint32_t firstDataSector = bpb->reservedSectors + (bpb->numberOfFATs * bpb->sectorsPerFAT32);
    return firstDataSector + (cluster - 2) * bpb->sectorsPerCluster;
}

uint32_t FAT32::ReadFATEntry(uint32_t cluster) {
    uint32_t fatSector = bpb->reservedSectors + (cluster * 4) / bpb->bytesPerSector;
    uint32_t entryOffset = (cluster * 4) % bpb->bytesPerSector;
    
    uint32_t* fat = (uint32_t*)GlobalAllocator.RequestPages(1);
    port->Read(fatSector, 1, fat);
    
    return fat[entryOffset / 4] & 0x0FFFFFFF; // Mask out high bits as they're reserved in FAT32
}


bool FAT32::LoadDirectoryEntries(uint32_t cluster, FAT32_DirectoryEntry* entries, uint32_t maxEntries) {
    uint32_t sector = (cluster - 2) * bpb->sectorsPerCluster + dataStartSector; // Calculate the starting sector of the cluster
    uint32_t bytesPerCluster = bpb->bytesPerSector * bpb->sectorsPerCluster;

    for (uint32_t i = 0; i < bpb->sectorsPerCluster; i++) {
        port->Read(sector + i, 1, (void*)((uintptr_t)entries + i * bytesPerCluster));
    }
    return true;
}

FAT32_DirectoryEntry* FAT32::LoadDirectoryEntries(const char* filename) {
    FAT32_DirectoryEntry entries[16]; // Change this size based on your needs
    uint32_t cluster = bpb->rootCluster;

    while (cluster != 0xFFFFFFFF) {
        LoadDirectoryEntries(cluster, entries, 16);

        for (uint32_t i = 0; i < 16; i++) { // Change this based on the max entries you read
            FAT32_DirectoryEntry& entry = entries[i];

            if (entry.name[0] == 0x00) {
                // End of directory entries
                return nullptr;
            }

            if ((entry.attr & 0x0F) == 0) { // Not a long file name entry
                if (strncmp(entry.name, filename, 11) == 0) {
                    return &entry;
                }
            }
        }

        // Get the next cluster
        cluster = GetNextCluster(cluster);
    }
    return nullptr; // File not found
}

uint32_t FAT32::GetNextCluster(uint32_t cluster) {
    uint32_t fatStartSector = bpb->reservedSectors; // Start of FAT
    uint32_t fatOffset = cluster * 4; // Each entry is 4 bytes
    uint32_t fatSector = fatStartSector + (fatOffset / bpb->bytesPerSector);
    uint32_t fatEntryOffset = fatOffset % bpb->bytesPerSector;

    uint32_t fatEntry;
    port->Read(fatSector, 1, &fatEntry); // Read the sector

    return (fatEntry & 0x0FFFFFFF); // Return only the lower 28 bits (FAT32)
}

void FAT32::LoadFile(const char* filename, FAT32_DirectoryEntry* directory, uint8_t* fileBuffer) {
    FAT32_DirectoryEntry* entry = FindFile(filename, directory);
    if (entry) {
        if (fileBuffer == nullptr) {
            return;
        }
        fileBuffer = (uint8_t*)malloc(entry->fileSize);
        
        // Successfully found the file, now load the data
        LoadFileData(entry, fileBuffer);
    } else {
        // Handle file not found case
        fileBuffer = NULL;
        globalRenderer->print("File not found\n");
    }
}

void FAT32::LoadPartition(MBR::PartitionEntry partition) {
    uint32_t partitionStart = partition.lbaFirst;
    port->Read(partitionStart, 1, bpb);

    globalRenderer->print("Partition Start: ");
    globalRenderer->print(toString((uint64_t)partitionStart));
    globalRenderer->print("\n");

    // Set up additional FAT32 parameters if needed
}
const char* FormatFilename(const char* original) {
    static char formatted[12]; // 11 characters + 1 for null terminator
    size_t i = 0;

    // Copy the first part (up to 8 characters)
    while (i < 8 && original[i] != '\0' && original[i] != '.') {
        formatted[i] = original[i];
        i++;
    }

    // If there are less than 8 characters, fill with spaces
    while (i < 8) {
        formatted[i++] = ' ';
    }

    // If there's an extension, copy it (up to 3 characters)
    if (original[i] == '.') {
        i++;
        size_t extIndex = 0;
        while (extIndex < 3 && original[i] != '\0') {
            formatted[8 + extIndex] = original[i];
            i++;
            extIndex++;
        }
    }

    // Fill the rest with spaces
    while (i < 11) {
        formatted[i++] = ' ';
    }

    formatted[11] = '\0'; // Null terminate the string
    return formatted;
}

void FAT32::InitDisk(AHCI::Port* port_){
    port = port_;
    bpb = (FAT32_BPB*) GlobalAllocator.RequestPages(1); //can fit 8 sectors, we are reading only one
    port->Read(0, 1, bpb);
    globalRenderer->print("FILE SYSTEM: ");
    globalRenderer->print(bpb->fileSystemType);
    globalRenderer->print("\n");
    for (int i=0;i<5;i++){
        if (bpb->fileSystemType[i] != "FAT32"[i]){
            fat32 = false;
            globalRenderer->print("Not FAT32!\n");
            return;
        }
    }
    fat32 = true;

    uint32_t bytesPerSector = bpb->bytesPerSector;
    uint32_t sectorsPerCluster = bpb->sectorsPerCluster;
    uint32_t reservedSectors = bpb->reservedSectors;
    uint32_t sectorsPerFAT = bpb->sectorsPerFAT32;
    uint32_t rootCluster = bpb->rootCluster;

    uint32_t fatStartSector = reservedSectors;
    // First sector of the data region (after FATs)
    dataStartSector = fatStartSector + (bpb->numberOfFATs * sectorsPerFAT);
    globalRenderer->print("Bytes Per Sector: ");
    globalRenderer->print(toString((uint64_t)bytesPerSector));
    globalRenderer->print("\n");
    globalRenderer->print("OEMName: ");
    globalRenderer->print((const char*)bpb->OEMName);
    globalRenderer->print("\n");

    MBR* mbr = (MBR*)GlobalAllocator.RequestPages(1);
    port->Read(0, 1, mbr);
    globalRenderer->print("Signature: ");
    globalRenderer->print(toHString(bpb->bootSignature));
    globalRenderer->print("\n");

    if (bpb->bootSignature != 0xAA55) {
        globalRenderer->print("Invalid MBR signature!\n");
        return;
    }

    // Iterate through the partition entries
    for (int i = 0; i < MAX_PARTITIONS; i++) {
        if (mbr->partitions[i].status == 0x80) {  // Bootable partition
            globalRenderer->print("Found bootable partition: ");
            globalRenderer->print(toString((uint64_t)i));
            globalRenderer->print("\n");
            uint8_t* fileBuffer;
            LoadFile(FormatFilename("background.bmp"), nullptr, fileBuffer);
            if (fileBuffer != NULL){
                globalRenderer->print("LOADED BACKGROUND.BMP!");
            }
        }
    }
}