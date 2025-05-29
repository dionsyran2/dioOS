#include "wclass.h"

void RegisterClassINT(WNDCLASS* wclass){
    wndclasses->add(wclass);
}

void UnRegisterClassINT(WNDCLASS* wclass){
    wndclasses->remove(wclass);
}

WNDCLASS* FindClassINT(wchar_t* name, thread_t* thread){
    for (int i = 0; i < wndclasses->length(); i++){
        WNDCLASS* wclass = wndclasses->get(i);
        if ((strcmp(wclass->ClassName, name) == 0) && wclass->thread->pid == thread->pid){
            return wclass;
        }
    }
}

void SendMessageINT(MSG* msg, WNDCLASS* wclass){
    if (wclass->num_of_messages > 31) return;
    wclass->messages[wclass->num_of_messages] = msg;
    wclass->num_of_messages++;
}