#include <drivers/ps2/ps2.h>
#include <acpi.h>
#include <IO.h>
#include <cpu.h>
#include <drivers/timers/common.h>
#include <interrupts/interrupts.h>
#include <kstdio.h>
#include <scheduling/apic/ioapic.h>
#include <drivers/ps2/keyboard.h>
#include <drivers/ps2/mouse.h>


const char* ps2_port_test_codes[] = {
    "Success",
    "Clock line stuck low",
    "Clock line stuck high",
    "Data line stuck low",
    "Data line stuck high"
};

namespace ps2{
    function primary_cb = nullptr;
    function secondary_cb = nullptr;

    void ps2_primary_isr(void*){
        if (primary_cb) primary_cb();
    }

    void ps2_secondary_isr(void*){
        if (secondary_cb) secondary_cb();
    }

    void ps2_register_isr(uint8_t handle, function cb){
        if (handle == 0){
            primary_cb = cb;
        } else {
            secondary_cb = cb;
        }
    }

    bool does_ps2_controller_exist(){
        if (AcpiGbl_FADT.BootFlags & ACPI_FADT_8042) {
            return true; // Controller exists
        }

        return false;
    }

    void write_value(uint8_t value){
        // Wait until its available
        uint32_t timeout = 500000;
        while (timeout != 0){
            ps2_status_t res;
            res.raw = inb(PS2_STATUS);

            if (!res.bits.input_buffer_status) break;

            nanosleep(100);
            timeout--;
        }

        if (timeout == 0){
            kprintf("[PS2] Input buffer did not clear in time!\n");
            return;
        }
        
        // Write the command
        io_wait();
        outb(PS2_DATA, value);
    }

    void send_cmd(uint8_t cmd){
        // Wait until its available
        uint32_t timeout = 500000;
        while (timeout != 0){
            ps2_status_t res;
            res.raw = inb(PS2_STATUS);

            if (!res.bits.input_buffer_status) break;

            nanosleep(100);
            timeout--;
        }

        if (timeout == 0){
            kprintf("[PS2] Input buffer did not clear in time!\n");
            return;
        }
        
        // Write the command
        io_wait();
        outb(PS2_COMMAND, cmd);
    }

    uint16_t read_res(){
        // Wait until its available
        uint32_t timeout = 500000;
        while (timeout != 0){
            ps2_status_t res;
            res.raw = inb(PS2_STATUS);

            if (res.bits.output_buffer_status) break;

            nanosleep(100);
            timeout--;
        }

        if (timeout == 0){
            //kprintf("[PS2] Output buffer did not clear in time!\n");
            return 0x100;
        }
        
        // Write the command
        io_wait();
        uint8_t ret = inb(PS2_DATA);

        return ret;
    }

    void send_data_to_pri(uint8_t data){
        write_value(data);
    }

    void send_data_to_sec(uint8_t data){
        send_cmd(PS2_WRITE_BYTE_TO_SECONDARY);
        write_value(data);
    }

    void send_port_cmd(uint8_t ident, uint8_t data){
        if (ident == 0){
            send_data_to_pri(data);
        } else if (ident == 1){
            send_data_to_sec(data);
        }
    }

    
    void flush(){
        int limit = 10000;
        while ((inb(PS2_STATUS) & 1) && limit--) {
            inb(PS2_DATA); // Discard
            nanosleep(10);
        }
    }

    void start_ps2_driver(int ident) {
        flush(); // Clear any junk

        // Disable Scanning
        send_port_cmd(ident, PS2_DEV_DISABLE_SCANNING); 
        if (read_res() != 0xFA) return;

        // 2. Identify
        send_port_cmd(ident, PS2_DEV_IDENTIFY);
        if (read_res() != 0xFA) return;

        // 3. Read ID (Wait up to 10ms for first byte)
        uint16_t b1 = read_res();
        if (b1 == 0x100) return; // Timeout

        if (b1 == 0x00 || b1 == 0x03 || b1 == 0x04) {
            kprintf("[PS2] Mouse detected on port %d (ID: %x)\n", ident, b1);
            ps2_mouse::init_driver(ident);
            return;
        }

        if (b1 == 0xAB) {
            uint16_t b2 = read_res();
            kprintf("[PS2] Keyboard detected on port %d (ID: AB %x)\n", ident, b2);
            ps2_kb::init_kb(ident);
            return;
        }
        
        kprintf("[PS2] Unknown device on port %d: %x\n", ident, b1);
    }

    void initialize_ps2(){
        if (!does_ps2_controller_exist()) return; // There is no ps2 controller

        // Disable the ports
        send_cmd(PS2_DISABLE_FIRST_PORT);
        send_cmd(PS2_DISABLE_SECONDARY_PORT);

        flush();

        // Disable ports 1 and 2
        send_cmd(PS2_READ_CONFIG_BYTE);
        uint8_t config = read_res();
        config &= ~0b01111011;
        config |= (1 << 6);
        
        // Write back the config
        send_cmd(PS2_WRITE_CONFIG_BYTE);
        write_value(config);

        // Perform the self test
        send_cmd(PS2_SELF_TEST);
        uint8_t res = read_res();

        if (res != PS2_SELF_TEST_RESULT::CONTROLLER_SUCCESS){
            kprintf("[PS2] Self test failed with code: %.2x\n", res);
            return;
        }

        // Perform port tests
        send_cmd(PS2_TEST_FIRST_PORT);
        res = read_res();

        bool p1 = false;
        bool p2 = false;

        if (res != PS2_PORT_TEST_RESULT::PORT_SUCCESS){
            const char* str = res > 4 ? "Unknown error" : ps2_port_test_codes[res];
            kprintf("[PS2] Primary port test failed: %s.\n", str);
        } else {
            flush();
            // Configure port 1

            // Install ISR
            _add_dynamic_isr(PS2_PRI_PORT_VECTOR, ps2_primary_isr, nullptr);
            set_apic_irq(PS2_FIRST_PORT_IRQ, PS2_PRI_PORT_VECTOR, false);

            // Enable the port
            send_cmd(PS2_READ_CONFIG_BYTE);
            uint8_t config = read_res();
            config &= ~0b00010000; // Enable the clock

            // Write back the config
            send_cmd(PS2_WRITE_CONFIG_BYTE);
            write_value(config);

            start_ps2_driver(0);
            p1 = true;
        }

        send_cmd(PS2_TEST_SECONDARY_PORT);
        res = read_res();

        if (res != PS2_PORT_TEST_RESULT::PORT_SUCCESS){
            const char* str = res > 4 ? "Unknown error" : ps2_port_test_codes[res];
            kprintf("[PS2] Secondary port test failed: %s.\n", str);
        } else {
            // Flush
            flush();
            // Configure port 2

            // Install ISR
            _add_dynamic_isr(PS2_SEC_PORT_VECTOR, ps2_secondary_isr, nullptr);
            set_apic_irq(PS2_SECONDARY_PORT_IRQ, PS2_SEC_PORT_VECTOR, false);

            // Enable the port
            send_cmd(PS2_READ_CONFIG_BYTE);
            uint8_t config = read_res();
            config &= ~0b00100000;

            // Write back the config
            send_cmd(PS2_WRITE_CONFIG_BYTE);
            write_value(config);

            start_ps2_driver(1);
            p2 = true;
        }

        // enable interrupts
        send_cmd(PS2_READ_CONFIG_BYTE);
        config = read_res();
        
        if (p1) config |= 1;
        if (p2) config |= (1 << 1);

        // Write back the config
        send_cmd(PS2_WRITE_CONFIG_BYTE);
        write_value(config);

        flush();
    }
}
