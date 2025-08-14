#include <users.h>
#include <cryptography/sha256.h>

void* usr_data = nullptr;

bool load_passwd(){
    uint8_t* usr_cnt = (uint8_t*)usr_data;

    vnode_t* file = vfs::resolve_path("/etc/passwd");
    if (file == nullptr) return false;
    file->open();
    char* data = new char[file->size];
    size_t cnt = file->read(data, file->size, 0);
    cnt = strlen(data);


    for (int i = 0; i < cnt;){
        int ln_start = i;
        size_t line_length = 0;

        // find the end of the line
        while (i + line_length < cnt) {
            char c = data[i + line_length];
            if (c == '\n' || c == '\r') {
                // Handle possible \r\n or \n\r sequences (Windows line endings)
                if ((i + line_length + 1 < cnt) &&
                    ((data[i + line_length + 1] == '\n' || data[i + line_length + 1] == '\r') &&
                    data[i + line_length + 1] != c)) {
                    line_length++; // skip the paired newline char
                }
                line_length++; // include newline char(s)
                break;
            }
            line_length++;
        }

        if (i + line_length > cnt) line_length = cnt - i;

        i += line_length;

        // Each entry contains 7 values seperated by ':'.
        // So its safe to assume that if the length is <= 6 its invalid
        // + ignore the line if it starts with '#' (comment)

        if (line_length <= 6 || data[ln_start] == '#') continue;

        char* tmp = new char[line_length + 1];
        memcpy(tmp, data + ln_start, line_length);
        tmp[line_length] = '\0';

        // Get rid of the windows line endings
        for (int j = line_length - 1; j >= 0; j--) {
            if (tmp[j] == '\n' || tmp[j] == '\r') tmp[j] = '\0';
            else break;
        }

        int len = 0;

        char array[20][200];

        // break the line into each value
        int off = 0;
        for (int x = 0; x < (line_length + 1); x++){
            if (tmp[x] == ':' || x == line_length || tmp[x] == '\0'){
                memcpy(array[len], tmp + off, x - off);
                array[len][x - off] = '\0';
                off = x + 1;
                len++;
            }
            if (tmp[x] == '\0') break;
        }

        if (len != 7) {
            delete[] tmp; // invalid entry
            continue;
        }

        char* username = array[0];
        char* passwd = array[1];
        char* UID = array[2];
        char* GID = array[3];
        char* comment = array[4];
        char* home_dir = array[5];
        char* usr_shell = array[6];

        user_t* usr = (user_t*)((uint64_t)usr_data + sizeof(uint8_t) + (sizeof(user_t) * *usr_cnt));
        strcpy(usr->username, username);
        strcpy(usr->comment, comment);
        strcpy(usr->home, home_dir);
        strcpy(usr->shell, usr_shell);
        usr->UID = kstrtol(UID, nullptr, 10);
        usr->GID = kstrtol(GID, nullptr, 10);

        if (usr->UID == 0 || usr->GID == 0) usr->superuser = true;
        
        (*usr_cnt)++;

        delete[] tmp;
    }
    delete[] data;
    return true;
}


bool load_shadow(){
    uint8_t* usr_cnt = (uint8_t*)usr_data;

    vnode_t* file = vfs::resolve_path("/etc/shadow");
    if (file == nullptr) return false;
    file->open();
    char* data = new char[file->size];
    size_t cnt = file->read(data, file->size, 0);
    
    cnt = strlen(data);


    for (int i = 0; i < cnt;){
        int ln_start = i;
        size_t line_length = 0;

        // find the end of the line
        while (i + line_length < cnt) {
            char c = data[i + line_length];
            if (c == '\n' || c == '\r') {
                // Handle possible \r\n or \n\r sequences (Windows line endings)
                if ((i + line_length + 1 < cnt) &&
                    ((data[i + line_length + 1] == '\n' || data[i + line_length + 1] == '\r') &&
                    data[i + line_length + 1] != c)) {
                    line_length++; // skip the paired newline char
                }
                line_length++; // include newline char(s)
                break;
            }
            line_length++;
        }

        if (i + line_length > cnt) line_length = cnt - i;

        i += line_length;

        if (line_length <= 2 || data[ln_start] == '#') continue;

        char* tmp = new char[line_length + 1];
        memcpy(tmp, data + ln_start, line_length);
        tmp[line_length] = '\0';

        // Get rid of the windows line endings
        for (int j = line_length - 1; j >= 0; j--) {
            if (tmp[j] == '\n' || tmp[j] == '\r') tmp[j] = '\0';
            else break;
        }

        int len = 0;

        char array[20][200];

        // break the line into each value
        int off = 0;
        for (int x = 0; x < (line_length + 1); x++){
            if (tmp[x] == ':' || x == line_length || tmp[x] == '\0'){
                memcpy(array[len], tmp + off, x - off);
                array[len][x - off] = '\0';
                off = x + 1;
                len++;
            }
            if (tmp[x] == '\0') break;
        }

        if (len != 2) {
            delete[] tmp; // invalid entry
            continue;
        }

        char* username = array[0];
        char* passwd = array[1];
        

        
        for (int i = 0; i < *usr_cnt; i++){
            user_t* usr = (user_t*)((uint64_t)usr_data + sizeof(uint8_t) + (sizeof(user_t) * i));
            if (strcmp(usr->username, username) == 0){
                strcpy(usr->pwd_hash, passwd);
            }
        }

        delete[] tmp;
    }
    return true;
}

void hash_to_string(BYTE hash[32], char* buffer){
    for (int i = 0; i < 32; i ++){
        const char* str = toHString(hash[i]);
        buffer[i * 2] = to_lower(str[0]);
        buffer[(i * 2) + 1] = to_lower(str[1]);
    }
}

bool load_users(){
    if (usr_data == nullptr) usr_data = GlobalAllocator.RequestPage(); // Allocate 1 page for all of the data
    memset(usr_data, 0, 0x1000);
    uint8_t* usr_cnt = (uint8_t*)usr_data;
    *usr_cnt = 0;

    if (!load_passwd()) return false;
    if (!load_shadow()) return false;

    // Set up page protection (you dont want user applications to access this)
    globalPTM.SetPageFlag(usr_data, PT_Flag::NX, true); // Set the no execute bit
    globalPTM.SetPageFlag(usr_data, PT_Flag::UserSuper, false); // Can only be accessed in CPL 0
    globalPTM.SetPageFlag(usr_data, PT_Flag::ReadWrite, false); // Make the page read only
    return true;
}

user_t* validate_user(char* user, char* passwd){
    uint8_t usr_cnt = *(uint8_t*)usr_data;

    for (int i = 0; i < usr_cnt; i++){
        user_t* usr = (user_t*)((uint64_t)usr_data + sizeof(uint8_t) + (sizeof(user_t) * i));
        if (strcmp(usr->username, user) == 0){

            if (strlen(usr->pwd_hash) == 0 && strlen(passwd) == 0) return usr;
            SHA256_CTX ctx;
            BYTE hash[32];

            sha256_init(&ctx);
            sha256_update(&ctx, (BYTE*)passwd, strlen(passwd));
            sha256_final(&ctx, hash);

            
            char hashed_passwd[64];
            hash_to_string(hash, hashed_passwd);

            if (memcmp(usr->pwd_hash, hashed_passwd, 64) == 0) return usr;
        }
    }
    return nullptr;
}