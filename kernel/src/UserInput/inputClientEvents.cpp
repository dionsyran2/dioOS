#include <UserInput/inputClientEvents.h>
namespace InEvents{
    void** EVENT_BUFFER;

    void AddEvent(EVENT_DEFAULT* event, void* newEvent){
        if (event->nextEvent == nullptr){
            event->nextEvent = newEvent;
        }else{
            AddEvent((EVENT_DEFAULT*)event->nextEvent, newEvent);
        }
    }

    void SendEvent(void* event){
        ((EVENT_DEFAULT*)event)->nextEvent = nullptr;
        if (EVENT_BUFFER == nullptr) return;
        if (*EVENT_BUFFER == nullptr){
            *EVENT_BUFFER = event;
        }else{
            AddEvent((EVENT_DEFAULT*)*EVENT_BUFFER, event);
        }
    }
}