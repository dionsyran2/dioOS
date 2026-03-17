#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cpu.h>

#define PS2_DATA    0x60
#define PS2_STATUS  0x64 // When READ
#define PS2_COMMAND 0x64 // When WRITTEN


// PS2 Commands
#define PS2_READ_CONFIG_BYTE        0x20
#define PS2_WRITE_CONFIG_BYTE       0x60
#define PS2_SELF_TEST               0xAA
#define PS2_READ_OUTPUT_PORT        0xD1
#define PS2_WRITE_OUTPUT_PORT       0xD1


// First Port
#define PS2_TEST_FIRST_PORT         0xAB
#define PS2_ENABLE_FIRST_PORT       0xAE
#define PS2_DISABLE_FIRST_PORT      0xAD


// Secondary Port
// Only if 2 ports are supported
#define PS2_ENABLE_SECONDARY_PORT   0xA8
#define PS2_DISABLE_SECONDARY_PORT  0xA7
#define PS2_TEST_SECONDARY_PORT     0xA9
#define PS2_WRITE_BYTE_TO_SECONDARY 0xD4

// IRQ
#define PS2_FIRST_PORT_IRQ          1
#define PS2_SECONDARY_PORT_IRQ      12

// Device Commands
#define PS2_DEV_ENABLE_SCANNING     0xF4
#define PS2_DEV_DISABLE_SCANNING    0xF5
#define PS2_DEV_IDENTIFY            0xF2

enum PS2_PORT_TEST_RESULT {
    PORT_SUCCESS                 = 0,
    PORT_CLOCK_LINE_STUCK_LOW    = 0x01,
    PORT_CLOCK_LINE_STUCK_HIGH   = 0x02,
    PORT_DATA_LINE_STUCK_LOW     = 0x03,
    PORT_DATA_LINE_STUCK_HIGH    = 0x04
};

enum PS2_SELF_TEST_RESULT {
    CONTROLLER_SUCCESS                 = 0x55,
    CONTROLLER_FAILURE                 = 0xFC
};

struct ps2_status_t
{
    union{
        uint8_t raw;

        struct {
            uint8_t output_buffer_status: 1; // Output buffer status (0 = empty, 1 = full). Indicates whether there is data to read
            uint8_t input_buffer_status: 1; // Must be clear before attempting to write to any ports
            uint8_t system_flag: 1;
            uint8_t command_data: 1;
            uint8_t chipset_specific: 1; // Probably a keyboard lock
            uint8_t chipset_specific_2: 1;
            uint8_t timeout: 1;
            uint8_t parity_error: 1;
        } bits;
    };
};

namespace ps2{
    void initialize_ps2();
    void ps2_register_isr(uint8_t handle, function cb);
    void send_port_cmd(uint8_t ident, uint8_t data);
    uint16_t read_res();
}