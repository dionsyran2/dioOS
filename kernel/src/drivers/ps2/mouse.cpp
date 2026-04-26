#include <drivers/ps2/mouse.h>
#include <drivers/ps2/ps2.h>
#include <kstdio.h>
#include <IO.h>
#include <drivers/timers/common.h>
#include <evcodes.h>
#include <cstr.h>
#include <kerrno.h>

namespace ps2_mouse{
    int identifier;
    bool has_wheel = false;
    bool has_multi_btn = false;
    uint8_t packet[4];
    int packet_ptr = 0;
    uint8_t last_buttons = 0;
    vnode_t *evt;

    
    int ps2_mouse_evdev_ioctl(int operation, char* argp, vnode_t* this_node) {
        uint32_t op = (uint32_t)operation;
        task_t *self = task_scheduler::get_current_task();
        uint32_t user_len = (op >> 16) & 0x3FFF; // Extract buffer size from ioctl code

        // We need to handle the "Base" operation because the size bits change
        uint32_t base_op = op & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);

        switch (op) {
            case EVIOCGVERSION: {
                uint32_t version = 0x010001;
                return self->write_memory(argp, &version, 4) ? 0 : -EFAULT;
            }

            case EVIOCGID: {
                input_id id = { .bustype = BUS_I8042, .vendor = 0x2, .product = 0x6, .version = 0x0001 };
                return self->write_memory(argp, &id, sizeof(id)) ? 0 : -EFAULT;
            }
            
            case EVIOCGRAB: {
                this_node->exclusive_flag = (argp != nullptr);
                return 0;
            }
        }

        if (base_op == (uint32_t)EVIOCGNAME(0)) {
            const char* name = "dioOS PS/2 Mouse";
            size_t copy_size = (strlen(name) + 1 < user_len) ? strlen(name) + 1 : user_len;
            return self->write_memory(argp, name, copy_size) ? copy_size : -EFAULT;
        }

        uint32_t gbit_base = (uint32_t)EVIOCGBIT(0, 0) & ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
        
        if (base_op >= gbit_base && base_op <= (gbit_base + 0x1F)) {
            uint32_t ev_type = base_op - gbit_base;
            uint64_t caps[12] = {0};
            size_t caps_size = 0;

            if (ev_type == 0) { // Supported Types
                caps[0] = (1 << EV_SYN) | (1 << EV_KEY) | (1 << EV_REL);
                caps_size = 8;
            } 
            else if (ev_type == EV_REL) { // Relative Axes (X, Y, Wheel)
                caps[0] = (1 << REL_X) | (1 << REL_Y);
                if (has_wheel) caps[0] |= (1 << REL_WHEEL);
                caps_size = 8;
            } 
            else if (ev_type == EV_KEY) { // Mouse Buttons
                caps[4] |= (1ULL << 16); // BTN_LEFT (0x110)
                caps[4] |= (1ULL << 17); // BTN_RIGHT (0x111)
                caps[4] |= (1ULL << 18); // BTN_MIDDLE (0x112)
                
                if (has_multi_btn) {
                    caps[4] |= (1ULL << 19); // BTN_SIDE
                    caps[4] |= (1ULL << 20); // BTN_EXTRA
                }
                caps_size = sizeof(caps);
            }

            size_t copy_size = (caps_size < user_len) ? caps_size : user_len;
            if (copy_size > 0) {
                return self->write_memory(argp, caps, copy_size) ? copy_size : -EFAULT;
            }
            
            return 0; 
        }

        return -EOPNOTSUPP;
    }

    int ps2_mouse_evdev_write(uint64_t offset, uint64_t size, const void* buf, vnode_t* node) {
        /*input_event* ev = (input_event*)buf;
        
        if (ev->type == EV_LED) {
            bool n = num_lock, c = caps_lock, s = scroll_lock;
            
            if (ev->code == LED_NUML) n = ev->value;
            if (ev->code == LED_CAPSL) c = ev->value;
            if (ev->code == LED_SCROLL) s = ev->value;
            
            ps2_kb::set_lights(c, s, n);
        }*/
        return size;
    }

    void push_event(event_node_t* node, uint16_t type, uint16_t code, int32_t val, bool wake = false) {
        input_event e;
        e.type = type; e.code = code; e.value = val;
        uint64_t total_ns = TSC::get_uptime_ns();
        
        e.time.tv_sec = total_ns / 1000000000ULL;
        e.time.tv_usec = (total_ns / 1000ULL) % 1000000ULL;
        node->write(&e);

        if (wake) {
            task_scheduler::wake_blocked_tasks(WAITING_ON_EVENT);
        }
    }

    void mouse_isr() {
        uint8_t data = inb(PS2_DATA);

        // Sync check: The first byte of a packet MUST have bit 3 set to 1.
        // If we think we are at the start of a packet but bit 3 is 0, we are out of sync.
        if (packet_ptr == 0 && !(data & 0x08)) return;

            packet[packet_ptr++] = data;

            // Determine expected packet size
            int packet_size = has_wheel ? 4 : 3;

            if (packet_ptr >= packet_size) {
            packet_ptr = 0;
            if (!evt) return;
            event_node_t* evnode = (event_node_t*)evt->fs_identifier;
            
            bool pushed_anything = false;

            // Movement (Only push if != 0) 
            int32_t x_move = (int32_t)packet[1];
            int32_t y_move = (int32_t)packet[2];
            if (packet[0] & 0x10) x_move |= 0xFFFFFF00; 
            if (packet[0] & 0x20) y_move |= 0xFFFFFF00; 
            y_move = -y_move;

            if (x_move != 0) { push_event(evnode, EV_REL, REL_X, x_move); pushed_anything = true; }
            if (y_move != 0) { push_event(evnode, EV_REL, REL_Y, y_move); pushed_anything = true; }

            if (has_wheel) {
                int8_t wheel_move = packet[3] & 0x0F;
                if (wheel_move & 0x08) wheel_move |= 0xF0;
                if (wheel_move != 0) { push_event(evnode, EV_REL, REL_WHEEL, -wheel_move); pushed_anything = true; }
            }

            // Buttons (Only push if changed) 
            uint8_t current_buttons = packet[0] & 0x07; // Mask bottom 3 bits
            if (current_buttons != last_buttons) {
                if ((current_buttons & 1) != (last_buttons & 1)) 
                    push_event(evnode, EV_KEY, BTN_LEFT, (current_buttons & 1));
                
                if ((current_buttons & 2) != (last_buttons & 2)) 
                    push_event(evnode, EV_KEY, BTN_RIGHT, (current_buttons & 2) >> 1);
                
                if ((current_buttons & 4) != (last_buttons & 4)) 
                    push_event(evnode, EV_KEY, BTN_MIDDLE, (current_buttons & 4) >> 2);
                    
                last_buttons = current_buttons;
                pushed_anything = true;
            }

            // 3. Final Sync (Only if we actually generated events)
            if (pushed_anything) {
                push_event(evnode, EV_SYN, SYN_REPORT, 0, true);
            }
        }
    }

    void secret_handshake() {
        // Enable Wheel (200, 100, 80)
        uint8_t wheel_rates[] = {200, 100, 80};
        for(int i = 0; i < 3; i++) {
            ps2::send_port_cmd(identifier, 0xF3); // Set Sample Rate
            ps2::read_res();
            ps2::send_port_cmd(identifier, wheel_rates[i]);
            ps2::read_res();
        }

        // Enable 5-Buttons (200, 200, 80)
        uint8_t btn_rates[] = {200, 200, 80};
        for(int i = 0; i < 3; i++) {
            ps2::send_port_cmd(identifier, 0xF3); 
            ps2::read_res();
            ps2::send_port_cmd(identifier, btn_rates[i]);
            ps2::read_res();
        }

        ps2::send_port_cmd(identifier, 0xF3); 
        ps2::read_res();
        ps2::send_port_cmd(identifier, 200);
        ps2::read_res();
    }

    bool init_driver(int handle){
        identifier = handle;
        evt = create_event_file();

        evt->close(); // there is already a reference open from the creation of the file
        if (!evt){
            kprintf("\e[0;31m[PS2 KB]\e[0m Initialization failed: Failed to create event file!");
            return false;
        }

        evt->file_operations.ioctl = ps2_mouse_evdev_ioctl;
        evt->file_operations.write = ps2_mouse_evdev_write;

        ps2::ps2_register_isr(handle, mouse_isr);
        uint32_t res;

        do {
            ps2::send_port_cmd(handle, PS2_DEV_ENABLE_SCANNING);
            res = ps2::read_res();
        } while ((res & 0xFF) == PS2_KB_RES_RESEND);

        if ((res & 0xFF) != 0xFA){
            kprintf("[PS/2] Enable Scanning command failed with code: %.2x\n", res);
            return false;
        }

        // Time for the magic handshake
        secret_handshake();

        // Now Identify again
        ps2::send_port_cmd(identifier, 0xF2);
        ps2::read_res();
        uint8_t new_id = ps2::read_res();
        kprintf("[PS2] Mouse re-identified as: %d\n", new_id);

        if (new_id == 0x03){
            has_wheel = true;
        }

        if (new_id == 0x04){
            has_wheel == true;
            has_multi_btn = true;
        }
        return true;
    }
}