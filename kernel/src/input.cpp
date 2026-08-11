#include <input.h>
#include <rendering/multiplexer.h>

void input_core_report_event(evdev_t *source_device, input_event *event) {
    
    source_device->push_event(event);

    if (event->type == EV_KEY) {
        
        if (source_device->supported_events_mask & (1 << EV_KEY)) {
            virtual_terminal *tty = global_multiplexer->get_active();

            if (!tty || !tty->get_state()) return;
            if (tty->input_handler_evdev) tty->input_handler_evdev(source_device, event, tty->input_handler_ctx);
        }
    }
}