#include <UserInput/mouse.h>
#include <pipe.h>

namespace PS2Mouse
{

    void MouseWait()
    {
        uint64_t timeout = 100000;
        while (timeout--)
        {
            if ((PS2::ReadStatus() & 0b10) == 0)
            {
                return;
            }
        }
    }

    void MouseWaitInput()
    {
        uint64_t timeout = 100000;
        while (timeout--)
        {
            if (PS2::ReadStatus() & 0b1)
            {
                return;
            }
        }
    }

    void MouseWrite(uint8_t value)
    {
        PS2::SendCommandToDevice(2, value);
    }

    uint8_t MouseRead()
    {
        MouseWaitInput();
        return PS2::ReadData();
    }

    uint8_t MouseCycle = 0;
    uint8_t MousePacket[4];
    bool MousePacketReady = false;
    Point MousePosition;
    Point MousePositionOld;

    void ProcessMousePacket()
    {
        if (!MousePacketReady)
            return;

        bool xNegative, yNegative, xOverflow, yOverflow;

        if (MousePacket[0] & PS2XSign)
        {
            xNegative = true;
        }
        else
            xNegative = false;

        if (MousePacket[0] & PS2YSign)
        {
            yNegative = true;
        }
        else
            yNegative = false;

        if (MousePacket[0] & PS2XOverflow)
        {
            xOverflow = true;
        }
        else
            xOverflow = false;

        if (MousePacket[0] & PS2YOverflow)
        {
            yOverflow = true;
        }
        else
            yOverflow = false;

        if (!xNegative)
        {
            MousePosition.X += MousePacket[1];
            if (xOverflow)
            {
                MousePosition.X += 255;
            }
        }
        else
        {
            MousePacket[1] = 256 - MousePacket[1];
            MousePosition.X -= MousePacket[1];
            if (xOverflow)
            {
                MousePosition.X -= 255;
            }
        }

        if (!yNegative)
        {
            MousePosition.Y -= MousePacket[2];
            if (yOverflow)
            {
                MousePosition.Y -= 255;
            }
        }
        else
        {
            MousePacket[2] = 256 - MousePacket[2];
            MousePosition.Y += MousePacket[2];
            if (yOverflow)
            {
                MousePosition.Y += 255;
            }
        }

        if (MousePosition.X < 0)
            MousePosition.X = 0;
        if (MousePosition.X > globalRenderer->targetFramebuffer->common.framebuffer_width - 1)
            MousePosition.X = globalRenderer->targetFramebuffer->common.framebuffer_width - 1;

        if (MousePosition.Y < 0)
            MousePosition.Y = 0;
        if (MousePosition.Y > globalRenderer->targetFramebuffer->common.framebuffer_height - 1)
            MousePosition.Y = globalRenderer->targetFramebuffer->common.framebuffer_height - 1;

        uint32_t bdata = 0;

        if (MousePacket[0] & PS2Leftbutton)
        {
            bdata |= (1 << 0);
        }
        if (MousePacket[0] & PS2Middlebutton)
        {
            bdata |= (1 << 1);
        }
        if (MousePacket[0] & PS2Rightbutton)
        {
            bdata |= (1 << 2);
        }

        MousePacketReady = false;
        MousePositionOld = MousePosition;

        //InEvents::SendEvent(event);
        char cdata[sizeof(uint64_t) * 4];
        uint64_t* data = (uint64_t*)cdata; 
        data[0] = MousePosition.X;
        data[1] = MousePosition.Y;
        data[2] = bdata;
        data[3] = 0;
    }

    void HandlePS2Mouse()
    {
        uint8_t data = PS2::ReadData();
        ProcessMousePacket();
        static bool skip = true;
        if (skip)
        {
            skip = false;
            return;
        }

        switch (MouseCycle)
        {
        case 0:

            if ((data & 0b00001000) == 0)
                break;
            MousePacket[0] = data;
            MouseCycle++;
            break;
        case 1:

            MousePacket[1] = data;
            MouseCycle++;
            break;
        case 2:

            MousePacket[2] = data;
            MousePacketReady = true;
            MouseCycle = 0;
            break;
        }
    }

    
    int Initialize()
    {

        // reset
        PS2::EnablePort(2, false);
        PS2::SendCommandToDevice(2, PS2_DEV_RESET);

        Sleep(10);

        PS2::ReadData();

        Sleep(10);
        PS2::ReadData();

        Sleep(10);
        PS2::ReadData();

        Sleep(10);

        PS2::SendCommandToDevice(1, PS2_DEV_IDENTIFY);

        Sleep(20);

        uint8_t res = PS2::ReadData();

        if (res == 0)
        {
            kprintf(0xFF0000, "[PS/2 Mouse] ");
            kprintf("No mouse connected!\n");
            return -1;
        }

        PS2::WriteCommand(PS2_READ_INTERN_RAM_BYTE_0);
        Sleep(10);
        uint8_t status = PS2::ReadData();
        status |= 0b10;
        MouseWait();
        Sleep(10);
        PS2::WriteCommand(PS2_WRITE_INTERN_RAM_BYTE_0);
        Sleep(10);
        MouseWait();
        Sleep(10);
        PS2::WriteData(status); // setting the correct bit is the "compaq" status byte

        MouseWrite(0xF6);
        MouseRead();

        MouseWrite(0xF4);
        MouseRead();
        return 0;
    }
}