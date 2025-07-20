#include <UserInput/ps2.h>

namespace PS2{
    bool dualPort = false;

    uint8_t ReadData(){
        return inb(PS2_DATA_REG);
    }

    uint8_t ReadStatus(){
        return inb(PS2_STATUS_REG);
    }

    void WriteData(uint8_t data){
        outb(PS2_DATA_REG, data);
    }

    void WriteCommand(uint8_t data){
        while(ReadStatus() & 1){
            ReadData();
            Sleep(10);
        }
        outb(PS2_CMD_REG, data);
    }

    void SendCommand(uint8_t command, uint8_t data){
        outb(PS2_CMD_REG, command);
        Sleep(10);
        outb(PS2_DATA_REG, data);
    }

    void SendCommandToDevice(uint8_t dev, uint8_t command){
        if (dev == 1){
            WriteData(command);
            Sleep(10);
        }else if (dev == 2 && dualPort){
            WriteCommand(PS2_WRITE_NXT_BYTE_TO_SEC_PORT);
            Sleep(10);
            WriteData(command);
            Sleep(10);
        }
    }


    void SendCommandToDevice(uint8_t dev, uint8_t command, uint8_t data){
        SendCommandToDevice(dev, command);
        SendCommandToDevice(dev, data);
    }

    int EnablePort(uint8_t port, bool interrupts){
        while(ReadStatus() & 0x02){
            ReadData();
        }
        if (port == 1){
            WriteCommand(PS2_ENABLE_PRIMARY_PORT);
            Sleep(10);

            WriteCommand(PS2_READ_INTERN_RAM_BYTE_0);
            Sleep(10);

            uint8_t config = ReadData();
            config &= ~PS2_CONF_BYTE_PRI_PORT_INT;
            if (interrupts){
                config |= PS2_CONF_BYTE_PRI_PORT_INT;
            }
            SendCommand(PS2_WRITE_INTERN_RAM_BYTE_0, config);
        }else if (port == 2 && dualPort){
            WriteCommand(PS2_ENABLE_SECONDARY_PORT);
            Sleep(10);

            WriteCommand(PS2_READ_INTERN_RAM_BYTE_0);
            Sleep(10);

            uint8_t config = ReadData();
            config &= ~PS2_CONF_BYTE_SEC_PORT_INT;
            if (interrupts){
                config |= PS2_CONF_BYTE_SEC_PORT_INT;
            }
            SendCommand(PS2_WRITE_INTERN_RAM_BYTE_0, config);
        }else{
            return -1;
        }
        return 0;
    }

    int Initialize(){
        WriteCommand(PS2_DISABLE_PRIMARY_PORT);
        WriteCommand(PS2_DISABLE_SECONDARY_PORT);

        ReadData(); // This will flush the output buffer
        Sleep(10);

        WriteCommand(PS2_READ_INTERN_RAM_BYTE_0); // Read Controller Configuration Byte
        Sleep(10);

        // Disable the interrupts and the clocks
        uint8_t config = ReadData();
        config &= ~PS2_CONF_BYTE_PRI_PORT_INT;
        config &= ~PS2_CONF_BYTE_SEC_PORT_INT;
        config |= PS2_CONF_BYTE_PRI_PORT_CLK;
        config |= PS2_CONF_BYTE_SEC_PORT_CLK;

        SendCommand(PS2_WRITE_INTERN_RAM_BYTE_0, config); // Write the controller configuration byte

        Sleep(10);

        WriteCommand(PS2_SELF_TEST); // Perform self test

        Sleep(10);


        /* Check if it supports 2 ports */
        uint8_t res = ReadData();
        if (res != PS2_SELF_TEST_PASS){
            kprintf(0xFF0000, "[PS2] ");
            kprintf("Self test failed (res = %hh)\n", res);
            return -1;
        }

        WriteCommand(PS2_ENABLE_SECONDARY_PORT);
        Sleep(10);
        WriteCommand(PS2_READ_CONTROLLER_OUTPUT_PORT);
        Sleep(10);
        config = ReadData();

        if ((config & PS2_CONF_BYTE_SEC_PORT_CLK) == 0){
            dualPort = true;
        }
        Sleep(10);

        WriteCommand(PS2_DISABLE_SECONDARY_PORT);
        /*********************************/

        WriteCommand(PS2_TEST_PRIMARY_PORT);
        Sleep(10);
        res = ReadData();

        if (res != PS2_PORT_TEST_PASS){
            kprintf(0xFF0000, "[PS2] ");
            kprintf("Primary port test failed (res = %hh)\n", res);
            return -1;
        }

        Sleep(10);

        if (dualPort){
            WriteCommand(PS2_TEST_PRIMARY_PORT);
            Sleep(10);
            res = ReadData();
            Sleep(10);
            if (res != PS2_PORT_TEST_PASS){
                kprintf(0xFF0000, "[PS2] ");
                kprintf("Secondary port test failed (res = %hh)\n", res);
                return -1;
            }
        }
        
        kprintf(0x00FF00, "[PS/2] ");
        kprintf("Mouse initialized!\n");
        return 0;
    }
}