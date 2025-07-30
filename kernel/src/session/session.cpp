#include <session/session.h>
#include <ArrayList.h>
#include <other/ELFLoader.h>

namespace session{
    uint16_t id = 1;
    ArrayList<session_t*>* sessions = nullptr;

    session_t* CreateSession(){
        if (sessions == nullptr) sessions = new ArrayList<session_t*>();
        session_t* out = new session_t;
        out->session_id = id;
        id++;
        
        char* terminal_name = new char[20];
        memset(terminal_name, 0, 20);
        strcat(terminal_name, "tty");
        strcat(terminal_name, (char*)toString(out->session_id));
        int res = tty::CreateTerminal(terminal_name);

        char* full_path = new char[128];
        memset(full_path, 0, 128);
        strcat(full_path, "/dev/");
        strcat(full_path, terminal_name);

        out->tty = vfs::resolve_path(full_path);
        
        if (out->tty == nullptr) panic("Could not create session!", "Could not create tty (path could not be resolved)");

        free(full_path);
        free(terminal_name);

        
        char* message = new char[1024];
        memset(message, 0, 1024);
        strcat(message, "dioOS at ");
        strcat(message, out->tty->name);
        strcat(message, "\n");

        out->tty->write(message, strlen(message));

        sessions->add(out);

        free(message);

        out->login();
        return out;
    }

    void session_t::login(){
        do {
            tty->ops.write("login: ", 7, tty);
            size_t u_size = 0;
            char* username = (char*)tty->ops.read(&u_size, tty);
            if (u_size > 0) username[u_size - 1] = '\0';

            tty->ops.write("password: ", 10, tty);
            size_t p_size = 0;
            char* password = (char*)tty->ops.read(&p_size, tty);
            if (p_size > 0) password[p_size - 1] = '\0';


            user = validate_user(username, password);
            if (user == nullptr){
                tty->ops.write("Login incorrect!\n", 17, tty);
            }
            free(username);
            free(password);
        } while(user == nullptr);
        tty->write("\f", 1);
        vnode_t* shell = vfs::resolve_path(user->shell);
        if (shell == nullptr){
            do{
                tty->ops.write("Enter executable path: ", 23, tty);
                size_t e_size = 0;
                char* exec_name = (char*)tty->ops.read(&e_size, tty);
                if (e_size != 0) exec_name[e_size - 1] = '\0'; // replace \n with \0

                vnode_t* exec = vfs::resolve_path(exec_name);
                if (exec == nullptr || e_size == 0){
                    tty->ops.write("Executable not found!\n", 22, tty);
                    continue;
                } 

                ELFLoader::Load(exec, 0, user, tty, this);
                break;
            }while (true);
        }else{
            ELFLoader::Load(shell, 0, user, tty, this);
        }
    }
}