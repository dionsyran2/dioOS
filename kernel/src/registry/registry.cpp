#include <registry/registry.h>
#include <VFS/vfs.h>

size_t num_of_entries = 0;
size_t size_in_bytes = 0;

RegKey* reg_keys[1024];

size_t GetLineLength(char* line){
    size_t length = 0;
    while(*line != '\n' && *line != '\r' && *line != '\0'){
        length++;
        line++;
    }
    return length;
}

size_t GetNumOfLines(const char* buffer) {
    size_t current_line = 1;

    while (*buffer) {
        if (*buffer == '\r') {
            if (*(buffer + 1) == '\n') buffer++; // skip \n in \r\n
            current_line++;
        } else if (*buffer == '\n') {
            current_line++;
        }
        buffer++;
    }

    return current_line;
}

char* ReadLine(char* buffer, int line){
    int current_line = 0;
    while(*buffer != '\0'){
        if (current_line == line){
            size_t line_length = GetLineLength(buffer);
            char* newBuffer = new char[line_length];
            memcpy(newBuffer, buffer, line_length);
            newBuffer[line_length] = '\0';
            return newBuffer;
        }

        if (*buffer == '\n' || *buffer == '\r') {
            if (*buffer == '\r' && *(buffer + 1) == '\n' || *buffer == '\n' && *(buffer + 1) == '\r') buffer++;
            current_line++;
        }
        buffer++;
    }
    return nullptr;
}

char* GetPath(char* buffer, int start_line){
    for (int i = start_line; i > 0; i--){
        char* line = ReadLine(buffer, i);
        if (line[0] == '['){
            int length = strlen(line);
            memcpy(line, line + 1, length - 1);
            line[length - 2] = '\0';
            return line;
        }

        free(line);
    }
    return nullptr;
}


REG_KEY_TYPE GetType(char* str){
    if (strncmp(str, "dword", 5) == 0){
        return REG_KEY_TYPE::REG_DWORD;
    }else if (strncmp(str, "word", 4) == 0){
        return REG_KEY_TYPE::REG_WORD;
    }else if (strncmp(str, "byte", 4) == 0){
        return REG_KEY_TYPE::REG_BYTE;
    }else{
        return REG_KEY_TYPE::REG_STRING;
    }
}

/*
In theory this is pretty simple. Read the file (in lines) till you find the character '"' (at the start of the line). Thats the key's name.
Then go up, till you find a line that starts with '['. Inside those brackets is the path.
Extract any information like the type and value, save it and move on to the next entry.
*/


void CreateKeyFromFileEntry(char* path, char* entry){
    size_t entry_length = strlen(entry);

    RegKey* key = new RegKey;
    key->path = path;

    //Extract key info
    size_t name_length = 0;
    size_t type_length = 0;
    size_t value_length = 0;

    for (int i = 1; i < entry_length; i++){
        if (entry[i] == '"'){
            name_length = i - 1;
            break;
        }
    }

    for (int i = name_length + 2; i < entry_length; i++){
        if (entry[i] == ':'){
            type_length = i - name_length - 3;
            break;
        }
    }

    value_length = entry_length - name_length - type_length - 2;

    char* name = new char[name_length + 1];
    char* type = new char[type_length + 1];
    char* value = new char[value_length + 1];
    REG_KEY_TYPE reg_type;

    memcpy(name, entry + 1, name_length);
    memcpy(type, entry + name_length + 3, type_length);
    reg_type = GetType(type);

    if (reg_type == REG_KEY_TYPE::REG_STRING) value_length--;

    memcpy(value, entry + name_length + type_length + 2, value_length);

    name[name_length] = '\0';
    type[type_length] = '\0';
    value[value_length] = '\0';

    key->name = name;
    key->type = reg_type;
    
    switch (key->type){
        case REG_STRING:
            key->str_value = value;
            break;
        case REG_BYTE:
            if (value[0] == '0' && value[1] == 'x'){
                key->byte_value = string_to_hex(value);
            }else{
                key->byte_value = string_to_int(value);
            }
            break;
        case REG_WORD:
            if (value[0] == '0' && value[1] == 'x'){
                key->word_value = string_to_hex(value);
            }else{
                key->word_value = string_to_int(value);
            }
            break;
        case REG_DWORD:
            if (value[0] == '0' && value[1] == 'x'){
                key->dword_value = string_to_hex(value);
            }else{
                key->dword_value = string_to_int(value);
            }
            break;
        case REG_BOOL:
            if (strncmp(value, "true", 5) == 0){
                key->bool_value = true;
            }else{
                key->bool_value = false;
            }
            break;
    }

    reg_keys[num_of_entries] = key;
    num_of_entries++;
    size_in_bytes += sizeof(RegKey);
}

void LoadEntries(char* buffer){
    int current_line = 0;
    int num_of_lines = GetNumOfLines(buffer);

    for (; current_line < num_of_lines; current_line++){
        char* line = ReadLine(buffer, current_line);
        if (line == nullptr) continue;

        if (line[0] == '"'){
            char* path = GetPath(buffer, current_line); // Get the path of the key
            CreateKeyFromFileEntry(path, line);

        }

        free(line);
    }
}

int LoadRegistry(){
    vnode_t* file = vfs::resolve_path("/mnt/sda1/sys_registry.sys");
    if (!file) return -1;

    size_t cnt = 0;
    char* data = (char*)file->ops.load(&cnt, file);
    if (data == nullptr) return -2;

    LoadEntries(data);

    return 0;
}


void InitRegistry(){
    int error_code = LoadRegistry();

    kprintf(0x00FF00, "[Registry] ");
    if (error_code != 0) return kprintf("function 'LoadRegistry' exited with error code: %d\n", error_code);
    kprintf("Loaded %d entries (%d bytes)\n", num_of_entries, size_in_bytes);
}

void RegQueryValue(char* keyPath, char* valueName, void* data){
    for (int i = 0; i < num_of_entries; i++){
        RegKey* key = reg_keys[i];

        if (strcmp(keyPath, key->path) == 0 && strcmp(valueName, key->name) == 0){
            switch(key->type){
                case REG_KEY_TYPE::REG_BOOL:
                    *(bool*)data = key->bool_value;
                    break;
                case REG_KEY_TYPE::REG_BYTE:
                    *(uint8_t*)data = key->byte_value;
                    break;
                case REG_KEY_TYPE::REG_WORD:
                    *(uint16_t*)data = key->word_value;
                    break;
                case REG_KEY_TYPE::REG_DWORD:
                    *(uint32_t*)data = key->dword_value;
                    break;
                case REG_KEY_TYPE::REG_STRING:
                    char* d = new char[strlen(key->str_value) + 1];
                    strcpy(d, key->str_value);
                    *((char**)data) = d;
                    break; 
            }
        }
    }
}
