#include <drivers/ps2/keyboard.h>
#include <drivers/ps2/ps2.h>
#include <kstdio.h>
#include <IO.h>
#include <drivers/timers/common.h>
#include <cstr.h>
#include <input.h>

#define EXTENDED_BYTE   0xE0

namespace ps2_kb{
    bool extended = false;
    int identifier = 0;
    evdev_t *ps2_kb_evdev = nullptr;

    bool caps = false;
    bool scroll = false;
    bool num = false;
    
    void kb_isr(){
        uint8_t scancode = inb(PS2_DATA);

        if (scancode == PS2_KB_RES_ACK || scancode == PS2_KB_RES_RESEND) return;
        
        if (scancode == EXTENDED_BYTE){
            extended = true;
            return;
        }

        // Determine Press/Release
        bool is_released = (scancode & 0x80) != 0;
        uint8_t index = scancode & 0x7F; // Strip the high bit

        // Translate
        uint16_t linux_key_code = 0;

        if (extended) {
            linux_key_code = translate_e0(index); // Use the helper function
            extended = false;
        } else {
            // Bounds check for the dense array
            if (index < sizeof(set1_map) / sizeof(uint16_t)) {
                linux_key_code = set1_map[index];
            }
        }
        
        if (linux_key_code){

            input_event e;
            e.type = EV_KEY;
            e.code = linux_key_code;
            e.value = !is_released;
            e.time.tv_sec = current_time;
            e.time.tv_usec = (TSC::get_ticks() / TSC::get_ticks_per_ns()) / 1000;

            input_core_report_event(ps2_kb_evdev, &e);
        }
    }

    void set_lights(bool caps, bool scroll, bool num){
        ps2_kb_led led;
        led.bits.caps_lock = caps;
        led.bits.scroll_lock = scroll;
        led.bits.number_lock = num;

        
        ps2::send_port_cmd(identifier, PS2_KB_CMD_SET_LED);
        ps2::send_port_cmd(identifier, led.raw);
    }

    void ps2_kb_event_callback(void *context, input_event *event) {
        if (event->type == EV_LED) {
            caps = false;
            scroll = false;
            num = false;
            // Update our internal mask
            if (event->code == LED_CAPSL) {
                caps = event->value != 0;
            }
            else if (event->code == LED_NUML) {
                num = event->value != 0;
            }
            else if (event->code == LED_SCROLL) {
                caps = event->value != 0;
            }

            set_lights(caps, scroll, num);
        }
    }

    void init_kb(uint8_t ident){
        identifier = ident;
        ps2_kb_evdev = new evdev_t(ps2_kb_event_callback, nullptr);
        ps2_kb_evdev->supported_events_mask = (1 << EV_KEY) | (1 << EV_LED) | (1 << EV_SYN);
        strcpy(ps2_kb_evdev->name, "PS/2 Standard Keyboard");

        uint16_t res = 0;

        // Reset the keyboard
        do {
            ps2::send_port_cmd(ident, PS2_KB_CMD_RESET);
            res = ps2::read_res();
        } while ((res & 0xFF) == PS2_KB_RES_RESEND);

        if ((res & 0xFF) != PS2_KB_RES_ACK){
            kprintf("[PS/2 KB] Self test failed with code: %.2x (Byte 0)\n", res);
            return;
        }

        int timeout = 2;
        while (timeout--) {
            Sleep(1000);
            res = ps2::read_res();
            if ((res & 0xFF) == PS2_KB_RES_ST_PASS) break;
            if ((res & 0xFF) == 0xFC) { kprintf("[KB] Device Self Test Failed\n"); return; }
        }

        if ((res & 0xFF) != PS2_KB_RES_ST_PASS) {
            kprintf("[PS/2 KB] Timeout waiting for BAT (0xAA)\n");
            return;
        }

        ps2::ps2_register_isr(ident, kb_isr);

        do {
            ps2::send_port_cmd(ident, PS2_DEV_ENABLE_SCANNING);
            res = ps2::read_res();
        } while ((res & 0xFF) == PS2_KB_RES_RESEND);

        if ((res & 0xFF) != 0xFA){
            kprintf("[PS/2 KB] Enable Scanning command failed with code: %.2x\n", res);
            return;
        }
    }
}