#include <session/session.h>
#include <ArrayList.h>
#include <other/ELFLoader.h>
#include <tty/tty.h>

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

        out->tty->write(message, strlen(message), 0);

        sessions->add(out);

        free(message);

        out->login();
        return out;
    }

    void session_t::login(){
        task_t* self = task_scheduler::get_current_task();
        while(true){
            do {
                /*tty::tty_t* t = (tty::tty_t*)tty->misc_data[0];
                t->term.c_cflag |= CS8 | CREAD | CLOCAL;
                t->term.c_lflag |= ICANON | ECHO | ECHOE;
                t->term.c_oflag |= ONLCR;*/
                
                tty->write("login: ", 7);
                char* username = new char[4096];
                char* password = new char[4096];

                int64_t u_size = tty->read(username, 4096);
                if (u_size > 0) username[u_size - 1] = '\0';

                tty->write("password: ", 10);
                int64_t p_size = tty->read(password, 4096);
                if (p_size > 0) password[p_size - 1] = '\0';


                user = validate_user(username, password);
                if (user == nullptr){
                    tty->write("Login incorrect!\n", 17);
                }
                free(username);
                free(password);
            } while(user == nullptr);
            tty->write("\f", 1);
            vnode_t* shell = vfs::resolve_path(user->shell);
            if (shell == nullptr){
                int pid;
                do{
                    tty->write("Enter executable path: ", 23);
                    char exec_name[4096];
                    uint64_t e_size = tty->read(exec_name, 4096);
                    if (e_size != 0) exec_name[e_size - 1] = '\0'; // replace \n with \0

                    vnode_t* exec = vfs::resolve_path(exec_name);
                    if (exec == nullptr || e_size == 0){
                        tty->write("Executable not found!\n", 22);
                        continue;
                    } 

                    pid = ELFLoader::Load(exec, 0, user, tty, this, self);
                    break;
                }while (true);
                if (pid < 0) continue;
                task_scheduler::block_task(self, WAITING_ON_CHILD, pid);
            }else{
                int pid = ELFLoader::Load(shell, 0, user, tty, this, self);
                if (pid < 0) continue;
                task_scheduler::block_task(task_scheduler::get_current_task(), WAITING_ON_CHILD, pid);
            }
            tty->write("\f", 1);
        }
    }
}