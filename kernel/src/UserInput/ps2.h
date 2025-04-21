#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../IO.h"
#include "../BasicRenderer.h"
#include "../scheduling/apic/apic.h"
/***********************/
/*      REGISTERS      */
/***********************/

#define PS2_DATA_REG 0x60
#define PS2_STATUS_REG 0x64
#define PS2_CMD_REG 0x64

/***********************/
/*     STATUS BITS     */
/***********************/

#define PS2_OUT_BUFF_STS 1
#define PS2_IN_BUFF_STS (1 << 1)
#define PS2_SYS_FLAG (1 << 2)
#define PS2_COMMAND_DATA (1 << 3)
#define PS2_TIMEOUT_ERROR (1 << 6)
#define PS2_PARITY_ERROR (1 << 7)

/***********************/
/*      COMMANDS       */
/***********************/

#define PS2_SELF_TEST 0xAA

#define PS2_READ_INTERN_RAM_BYTE_0 0x20
#define PS2_WRITE_INTERN_RAM_BYTE_0 0x60

#define PS2_ENABLE_PRIMARY_PORT     0xAE
#define PS2_DISABLE_PRIMARY_PORT     0xAD
#define PS2_ENABLE_SECONDARY_PORT   0xA8
#define PS2_DISABLE_SECONDARY_PORT  0xA7

#define PS2_TEST_PRIMARY_PORT   0xAB
#define PS2_TEST_SECONDARY_PORT 0xA9

#define PS2_READ_CONTROLLER_OUTPUT_PORT 0xD0
#define PS2_WRITE_NXT_BYTE_TO_OUTPUT_PORT 0xD1

#define PS2_WRITE_NXT_BYTE_TO_SEC_PORT  0xD4

/***********************/
/*    DEV COMMANDS     */
/***********************/

#define PS2_DEV_RESET 0xFF
#define PS2_DEV_ENABLE_SCANNING 0xF4
#define PS2_DEV_DISABLE_SCANNING 0xF5
#define PS2_DEV_IDENTIFY 0xF2


/***********************/
/*       REPLIES       */
/***********************/
#define PS2_SELF_TEST_PASS 0x55
#define PS2_SELF_TEST_FAIL 0xFC

#define PS2_PORT_TEST_PASS 0x00

#define PS2_RES_AKN        0xFA


/***********************/
/*        MISC         */
/***********************/

#define PS2_CONF_BYTE_PRI_PORT_INT (1 << 0)
#define PS2_CONF_BYTE_SEC_PORT_INT (1 << 1)
#define PS2_CONF_BYTE_SYS_FLAG (1 << 2)
#define PS2_CONF_BYTE_PRI_PORT_CLK (1 << 4) // ACTIVE LOW
#define PS2_CONF_BYTE_SEC_PORT_CLK (1 << 5) // ACTIVE LOW
#define PS2_CONF_BYTE_PRI_PORT_TRANSLATION (1 << 6)

#define PS2_OUTPUT_RESET_BIT (1 << 0)
#define PS2_OUTPUT_A20_GATE (1 << 1)
#define PS2_OUTPUT_SEC_PORT_CLK (1 << 2)
#define PS2_OUTPUT_SEC_PORT_DATA (1 << 3)
#define PS2_OUTPUT_PRI_PORT_BUFF_FULL (1 << 4)
#define PS2_OUTPUT_SEC_PORT_BUFF_FULL (1 << 5)
#define PS2_OUTPUT_PRI_PORT_CLK (1 << 6)
#define PS2_OUTPUT_PRI_PORT_DATA (1 << 7)

namespace PS2{
    extern bool dualPort;
    uint8_t ReadData();
    uint8_t ReadStatus();
    void WriteData(uint8_t data);
    void WriteCommand(uint8_t data);
    void SendCommand(uint8_t command, uint8_t data);
    int EnablePort(uint8_t port, bool interrupts);
    void SendCommandToDevice(uint8_t dev, uint8_t command);
    void SendCommandToDevice(uint8_t dev, uint8_t command, uint8_t data);
    int Initialize();
}