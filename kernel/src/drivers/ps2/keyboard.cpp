#include <drivers/ps2/keyboard.h>
#include <drivers/ps2/ps2.h>
#include <kstdio.h>
#include <IO.h>
#include <drivers/timers/common.h>
#include <evcodes.h>
#include <cstr.h>
#include <kerrno.h>

#define EXTENDED_BYTE   0xE0

int ps2_kb_evdev_ioctl(int operation, char* argp, vnode_t* this_node){
    uint32_t op = (uint32_t)operation;
    task_t *self = task_scheduler::get_current_task();
    uint32_t user_len = (op >> 16) & 0x3FFF;

    switch (op){
        case EVIOCGVERSION: {
            uint32_t version = 0x010001; 
            return self->write_memory(argp, &version, 4) ? 0 : -EFAULT;
        }
        case EVIOCGID: {
            input_id id = { .bustype = 0x11, .vendor = 0x1, .product = 0x1, .version = 0xab41 };
            return self->write_memory(argp, &id, sizeof(id)) ? 0 : -EFAULT;
        }

        case EVIOCGRAB: {
            this_node->exclusive_flag = (argp != nullptr);
            return 0;
        }
    }

    uint32_t base_op = op & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);

    // --- EVIOCGNAME ---
    if (base_op == (uint32_t)EVIOCGNAME(0)) {
        const char* name = "dioOS PS/2 Keyboard";
        size_t name_len = strlen(name) + 1;
        size_t copy_size = (name_len < user_len) ? name_len : user_len;
        return self->write_memory(argp, name, copy_size) ? copy_size : -EFAULT;
    }

    uint32_t gbit_base = (uint32_t)EVIOCGBIT(0, 0) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
    if (base_op >= gbit_base && base_op <= (gbit_base + 0x1F)) {
        uint32_t ev_type = base_op - gbit_base;
        uint64_t caps[12] = {0};
        size_t caps_size = 0;

        if (ev_type == 0) { // Supported Types
            caps[0] = (1 << EV_SYN) | (1 << EV_KEY) | (1 << EV_LED) | (1 << EV_REP);
            caps_size = 8;
        } 
        else if (ev_type == EV_KEY) { // Supported Keys
            for (int i = 0; i < 248; i++) {
                caps[i / 64] |= (1ULL << (i % 64));
            }
            caps_size = sizeof(caps);
        }
        else if (ev_type == EV_LED) { // LEDs
            caps[0] = (1 << LED_NUML) | (1 << LED_CAPSL) | (1 << LED_SCROLL);
            caps_size = 8;
        }

        size_t copy_size = (caps_size < user_len) ? caps_size : user_len;
        if (copy_size > 0) {
            return self->write_memory(argp, caps, copy_size) ? copy_size : -EFAULT;
        }
        return 0;
    }

    return -EOPNOTSUPP;
}

namespace ps2_kb{
    bool extended = false;
    vnode_t* evt = nullptr;
    int identifier = 0;

    bool caps_lock = false;
    bool scroll_lock = false;
    bool num_lock = false;

    bool kbd_ready = false;

    int ps2_kb_evdev_write(uint64_t offset, uint64_t size, const void* buf, vnode_t* node) {
        input_event* ev = (input_event*)buf;
        
        if (ev->type == EV_LED) {
            bool n = num_lock, c = caps_lock, s = scroll_lock;
            
            if (ev->code == LED_NUML) n = ev->value;
            if (ev->code == LED_CAPSL) c = ev->value;
            if (ev->code == LED_SCROLL) s = ev->value;
            
            ps2_kb::set_lights(c, s, n);
        }
        return size;
    }

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
            if (!evt) return;

            event_node_t *evnode = (event_node_t *)evt->fs_identifier;

            input_event e;
            e.type = EV_KEY;
            e.code = linux_key_code;
            e.value = !is_released;
            uint64_t total_ns = TSC::get_uptime_ns();
        
            e.time.tv_sec = total_ns / 1000000000ULL;
            e.time.tv_usec = (total_ns / 1000ULL) % 1000000ULL;
            
            evnode->write(&e);

            input_event sync;
            sync.type = EV_SYN;
            sync.code = SYN_REPORT;
            sync.value = 0;
            sync.time = e.time;
            evnode->write(&sync);
        }
    }

    void set_lights(bool caps, bool scroll, bool num){
        caps_lock = caps;
        scroll_lock = scroll;
        num_lock = num;
        
        ps2_kb_led led;
        led.bits.caps_lock = caps;
        led.bits.scroll_lock = scroll;
        led.bits.number_lock = num;

        
        ps2::send_port_cmd(identifier, PS2_KB_CMD_SET_LED);
        ps2::send_port_cmd(identifier, led.raw);
    }

    void init_kb(uint8_t ident){
        identifier = ident;
        evt = create_event_file();

        kbd_ready = true;

        evt->close(); // there is already a reference open from the creation of the file
        if (!evt){
            kprintf("\e[0;31m[PS2 KB]\e[0m Initialization failed: Failed to create event file!");
            return;
        }

        evt->file_operations.ioctl = ps2_kb_evdev_ioctl;
        evt->file_operations.write = ps2_kb_evdev_write;

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