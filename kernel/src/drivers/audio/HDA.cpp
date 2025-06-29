#include <drivers/audio/HDA.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/apic/apic.h>
void *TestSong = nullptr;
uint8_t numOfHDAInstances = 0;
HDA *hdaInstances[10];

uint64_t *CORB;
uint64_t *RIRB;
uint8_t NumOfCodecs = 0;
uint8_t DAC[20];
uint8_t numOfDACs = 0;
uint8_t PINS[20];
uint8_t numOfpins = 0;
PCI::PCIHeader0 *hdr;
void HDA::Reset()
{ // Perform a cold reset
    volatile uint32_t *control = (volatile uint32_t *)(bar0 + 0x08);
    *control &= ~0x01;
    while (*control & 0x01)
        ;
    Sleep(1);
    *control |= 0x01;
    while ((*control & 0x01) == 0)
        ;
    Sleep(1);
}

uint16_t HDA::GetCORBSize()
{
    uint8_t *size = (uint8_t *)bar0 + 0x4E;

    if ((*size & 0b11) == 0x02)
    {
        return 256;
    }
    else if ((*size & 0b11) == 0x01)
    {
        return 16;
    }
    else
    {
        return 2;
    }
}

void HDA::ResetCORB()
{
    volatile uint8_t *ctrl = (volatile uint8_t *)(bar0 + 0x4C);
    *ctrl &= ~0b10;
    while (*ctrl & 0b10)
        ;
    for (int i = 0; i < 10000; i++)
        ;

    uint8_t *size = (uint8_t *)(bar0 + 0x4E);
    uint8_t entries2 = (*size >> 4) & 0b0001;
    uint8_t entries16 = (*size >> 4) & 0b0010;
    uint8_t entries256 = (*size >> 4) & 0b0100;

    //globalRenderer->printf("[HDA DRIVER] CORB SUPPORTED SIZE: [256]: 0x%hh| [16]: 0x%hh | [2]:0x%hh | [RAW]: 0x%hh\n", entries256, entries16, entries2, *size & 0xFF);

    if (entries256 == 1 && (entries16 == 1 || entries2 == 1))
    { // We need to check if there is at least one more supported. If only one is then the register is read only

       //globalRenderer->printf("[HDA DRIVER] Setting CORB Size to 0b10 (256 Entries)\n");

        *size &= ~0b11;
        *size |= 0b10;
    }
    else if (entries16 == 1 && entries2 == 1)
    { // We need to check if there is at least one more supported. If only one is then the register is read only

       //globalRenderer->printf("[HDA DRIVER] Setting CORB Size to 0b01 (16 Entries)\n");

        *size &= ~0b11;
        *size |= 0b01;
    } // If none of those are true, then there is only one size supported, the register is Read Only and the value is set to 0 (2 entries)

    uint64_t mem = (uint64_t)GlobalAllocator.RequestPage(); // Allocate memory for the corb commands
    memset((void *)mem, 0, 0x2000);
    CORB = (uint64_t *)mem;
    volatile uint32_t *memLow = (volatile uint32_t *)(bar0 + 0x40);
    volatile uint32_t *memHigh = (volatile uint32_t *)(bar0 + 0x44);
    *memLow = (uint32_t)(mem); // Set the low part of the memory address
    uint32_t *bar0 = (uint32_t *)(bar0);
    uint32_t Cap = *bar0;
    if (Cap & 0x1)
    {
        *memHigh = (uint32_t)((mem >> 32)); // Set the high part of the memory address
    }
    //globalRenderer->printf("[HDA DRIVER] CORB Low: [%x]/[%x] | CORB High: [%x]/[%x]\n", (uint32_t)(mem), *memLow, (uint32_t)((mem >> 32)), *memHigh);

    uint16_t *CORBRP = (uint16_t *)(bar0 + 0x4A);
    *CORBRP |= (1 << 15);
    while(!(*CORBRP & (1 << 15)));
    Sleep(10);
    *CORBRP &= ~(1 << 15);

    volatile uint16_t *writeptr = (volatile uint16_t *)(bar0 + 0x48);
    *writeptr = 0; // Reset the write pointer

    *ctrl |= 0x2; // Enable the CORB

    while ((*ctrl & 0b10) == 0);
}

void HDA::ResetRIRB()
{
    volatile uint8_t *ctrl = (volatile uint8_t *)(bar0 + 0x5C);
    *ctrl &= ~0b10;
    while (*ctrl & 0b10)
        ;

    uint8_t *size = (uint8_t *)(bar0 + 0x5E);
    uint8_t entries2 = (*size >> 4) & 0b0001;
    uint8_t entries16 = (*size >> 4) & 0b0010;
    uint8_t entries256 = (*size >> 4) & 0b0100;
    //globalRenderer->printf("[HDA DRIVER] RIRB SUPPORTED SIZE: [256]: 0x%hh| [16]: 0x%hh | [2]: 0x%hh | [RAW]: 0x%hh\n", entries256, entries16, entries2, *size);
    if (entries256 == 1 && (entries16 == 1 || entries2 == 1))
    { // We need to check if there is at least one more supported. If only one is then the register is read only
       //globalRenderer->printf("[HDA DRIVER] Setting RIRB Size to 0b10 (256 Entries)\n");
        *size &= ~0b11;
        *size |= 0b10;
    }
    else if (entries16 == 1 && entries2 == 1)
    { // We need to check if there is at least one more supported. If only one is then the register is read only
       //globalRenderer->printf("[HDA DRIVER] Setting RIRB Size to 0b01 (16 Entries)\n");
        *size &= ~0b11;
        *size |= 0b01;
    } // If none of those are true, then there is only one size supported, the register is Read Only and the value is set to 0 (2 entries)

    uint64_t mem = (uint64_t)GlobalAllocator.RequestPages((uint32_t)2); // Allocate memory for the RIRB
    memset((void *)mem, 0, 0x2000);
    if ((mem & 0xFF) != 0)
    {
       //globalRenderer->printf("[HDA DRIVER] ERROR: CORB buffer is not 256-byte aligned!\n");
    }
    volatile uint32_t *memLow = (volatile uint32_t *)(bar0 + 0x50);
    volatile uint32_t *memHigh = (volatile uint32_t *)(bar0 + 0x54);
    *memLow = (uint32_t)(mem); // Set the low part of the memory address
    uint32_t Cap = *(uint32_t*)bar0;
    if (Cap & 0x1)
    {
        *memHigh = (uint32_t)((mem >> 32)); // Set the high part of the memory address
    }
    RIRB = (uint64_t *)mem;
    

    volatile uint16_t *writeptr = (volatile uint16_t *)(bar0 + 0x58);
    *writeptr |= (1 << 15);
    Sleep(100);
    uint8_t *rintcnt = (uint8_t *)(bar0 + 0x5A);
    *rintcnt = 250;
    Sleep(100);

    *ctrl |= 0x3; // Enable the RIRB
    while (*ctrl & 0b10 == 0)
        ;
}

void HDA::SendCommand(uint8_t codec_address, uint8_t node_index, uint16_t command_code, uint8_t data)
{
    uint32_t command = (codec_address << 28) | (node_index << 20) | (command_code << 8) | data;
    uint16_t *CORBWP = (uint16_t *)(bar0 + 0x48);

    *(((uint32_t*)CORB) + (*CORBWP + 1)) = command;
    if (*CORBWP >= GetCORBSize())
    {
        *CORBWP = 0;
    }
    else
    {
        *CORBWP += 1;
    }
}

void HDA::SendCommand16(uint8_t codec_address, uint8_t node_index, uint8_t command_code, uint16_t data)
{
    uint32_t command = (codec_address << 28) | (node_index << 20) | (command_code << 16) | data;
    uint16_t *CORBWP = (uint16_t *)(bar0 + 0x48);

    *(((uint32_t*)CORB) + (*CORBWP + 1)) = command;
    if (*CORBWP >= GetCORBSize())
    {
        *CORBWP = 0;
    }
    else
    {
        *CORBWP += 1;
    }
}

uint16_t PrevRIRBWP = 0;
uint64_t HDA::ReadResponse()
{
    uint16_t *RIRBWP = (uint16_t *)(bar0 + 0x58);

    
    uint64_t cnt = 0;
    while (PrevRIRBWP == *RIRBWP && cnt < 1000)
    {
        cnt++;
        Sleep(1);
    }
    if (*RIRBWP == PrevRIRBWP)
        return 0;
    PrevRIRBWP = *RIRBWP;

    uint64_t response = *(uint64_t *)((uint8_t *)RIRB + ((*RIRBWP) * 8));
    return response;
}

void HDA::WaitForCommand()
{ // wait till the command is processed
    uint16_t *CORBRP = (uint16_t *)(bar0 + 0x4A);
    uint16_t *CORBWP = (uint16_t *)(bar0 + 0x48);
    volatile uint8_t *ctrl = (volatile uint8_t *)(bar0 + 0x5C);
    volatile uint8_t *corbctrl = (volatile uint8_t *)(bar0 + 0x4C);

    while (((*CORBWP) & 0xFF) > ((*CORBRP) & 0xFF))
    {
        __asm__ volatile("nop");
    }
}
uint64_t HDA::SRCMD(uint8_t codec_address, uint8_t node_index, uint16_t command_code, uint8_t data)
{ // Shortcut to send and read
    SendCommand(codec_address, node_index, command_code, data);
    WaitForCommand();
    return ReadResponse();
}

//16 bit payload
uint64_t HDA::SRCMD16(uint8_t codec_address, uint8_t node_index, uint8_t command_code, uint16_t data)
{ // Shortcut to send and read
    SendCommand16(codec_address, node_index, command_code, data);
    WaitForCommand();
    return ReadResponse();
}
connList *HDA::GetConnectedNodes(int codec, int widget)
{
    uint32_t res = SRCMD(codec, widget, GetParameters, ConnectionListLength);
    uint8_t lf = (res >> 7) & 0x1;
    uint8_t Entries = res & 0b01111111;
    uint8_t connections[50];
    size_t entries = 0;
    for (int i = 0; i < Entries; i++)
    {
        uint32_t entry = SRCMD(codec, widget, GetConnectionListEntry, 0);
        if (lf)
        { // 4 entries
            i += 3;
            connections[i + 0] = (uint8_t)entry;
            connections[i + 1] = (uint8_t)(entry >> 8);
            connections[i + 2] = (uint8_t)(entry >> 16);
            connections[i + 3] = (uint8_t)(entry >> 24);
            entries += 4;
        }
        else
        { // 2 entries
            connections[i + 0] = (uint8_t)entry;
            connections[i + 1] = (uint8_t)(entry >> 8);
            i++;
            entries += 2;
        }
        // globalRenderer->printf(0xffde21, "[HDA DRIVER] (%d) CONNECTION [%d]: %d\n", widget, lf ? i - 3 : i - 1, connections[lf ? i - 3 : i - 1]);
    }

    connList *list = new connList;
    memcpy(list->connections, connections, entries * sizeof(uint8_t));
    list->size = entries;
}

void HDA::EnumerateWidgets(int codec, int function)
{
    uint32_t res = SRCMD(codec, function, GetParameters, NodeCount);
    uint8_t NodesCount = res;
    uint8_t StartingNode = res >> 16;

    for (uint8_t widget = StartingNode; widget < StartingNode + NodesCount; widget++)
    {
        uint8_t type_of_node = ((SRCMD(codec, widget, 0xF00, 0x09) >> 20) & 0xF);
        //process node
        if(type_of_node == 0x0) {
            //log("Audio Output");
        
            //disable every audio output by connecting it to stream 0
            SRCMD(codec, widget, 0x706, 0x00);
        }
        else if(type_of_node == 0x1) {
            //log("Audio Input");
        }
        else if(type_of_node == 0x2) {
            //log("Audio Mixer");
        }
        else if(type_of_node==0x3) {
            //log("Audio Selector");
        }
        else if(type_of_node == 0x4) {
            //log("Pin Complex ");

            type_of_node = ((SRCMD(codec, widget, 0xF1C, 0x00) >> 20) & 0xF);
            if (type_of_node == 0x00) // Line out
            {
                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
            else if (type_of_node == 0x1)
            {
                // Speaker
                //  first speaker node is default speaker
                if (pin_speaker_default_node_number == 0)
                {
                    pin_speaker_default_node_number = widget;
                }

                // find if there is device connected to this PIN
                if ((SRCMD(codec, widget, 0xF00, 0x09) & 0x4) == 0x4)
                {
                    // find if it is jack or fixed device
                    if ((SRCMD(codec, widget, 0xF1C, 0x00) >> 30) != 0x1)
                    {
                        // find if is device output capable
                        if ((SRCMD(codec, widget, 0xF00, 0x0C) & 0x10) == 0x10)
                        {
                            // there is connected device
                            // connected output device

                            // we will use first pin with connected device, so save node number only for first PIN
                            if (pin_speaker_node_number == 0)
                            {
                                pin_speaker_node_number = widget;
                            }
                        }
                        else
                        {
                            // log("not output capable");
                        }
                    }
                    else
                    {
                        // log("not jack or fixed device");
                    }
                }
                else
                {
                    // log("no output device");
                }
            }
            else if (type_of_node == 0x2) // headphone pin out
            {
                // log("Headphone Out");

                // save node number
                // TODO: handle if there are multiple HP nodes
                pin_headphone_node_number = widget;
            }
            else if (type_of_node == 0x3)
            {
                // log("CD");

                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
            else if (type_of_node == 0x4)
            {
                // log("SPDIF Out");

                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
            else if (type_of_node == 0x5)
            {
                // log("Digital Other Out");

                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
            else if (type_of_node == 0x6)
            {
                // log("Modem Line Side");

                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
            else if (type_of_node == 0x7)
            {
                // log("Modem Handset Side");

                // save this node, this variable contain number of last alternative output
                pin_alternative_output_node_number = widget;
            }
        }
    }
}

void HDA::EnumerateFunctionGroups(int codec)
{
    uint32_t res = SRCMD(codec, 0, GetParameters, NodeCount); // Use the 4h parameter to get Node Count and Starting Node
    uint8_t NodesCount = res & 0xFF;
    uint8_t StartingNode = (res >> 16) & 0xFF;
    for (uint8_t function = StartingNode; function < StartingNode + NodesCount; function++)
    {
        uint64_t typeRes = SRCMD(codec, function, GetParameters, FunctionGroupType);
        uint8_t type = typeRes & 0xFF;
        if (type == 0x01)
        { // AFG
            //reset AFG
            SRCMD(codec, function, 0x7FF, 0x00);

            //enable power for AFG
            SRCMD(codec, function, 0x705, 0x00);

            //disable unsolicited responses
            SRCMD(codec, function, 0x708, 0x00);
            uint32_t audio_mixer_amp_capabilities = SRCMD(codec, function, 0xF00, 0x12);
            hda_set_node_gain(codec, function, 0x1, audio_mixer_amp_capabilities, 100);
            hda_set_node_gain(codec, function, 0x2, audio_mixer_amp_capabilities, 100);
            // globalRenderer->printf(0xFF0000, "[HDA DRIVER] AFG Function Group Located at %hh\n", function);
            EnumerateWidgets(codec, function);
        }
    }
}

void HDA::EnumerateCodecs()
{
    for (uint8_t codec = 0; codec < 16; codec++)
    {
        // if (codec == 0 || codec == 3) continue;
        uint64_t res = SRCMD(codec, 0, GetParameters, VendorIDP); // NID = 0 (ROOT NODE)
        if (res != 0)
        {
           //globalRenderer->printf("[HDA DRIVER] CODEC LOCATED (%d)[%x]\n", codec, res);
            EnumerateFunctionGroups(codec);
            break;
        }
    }
}

void HDA::SetVolume(uint8_t codec, uint8_t node, uint8_t volume)
{
    SRCMD(codec, node, 0x70C, 1 << 1);
    uint16_t val = 1 << 15 | 1 << 13 | 1 << 12 | volume;
    SRCMD(codec, node, AMPGain, val);
}

void HDA::SetFormat(uint8_t codec, uint8_t node, uint8_t type, uint8_t base, uint8_t mult, uint8_t div, uint8_t bps, uint8_t channels)
{
    uint16_t val = type << 15 | base << 14 | mult << 11 | div << 8 | bps << 4 | channels;
    SRCMD(codec, node, SetCONVFormat, val);
}

void HDA::SetConverterCTRL(uint8_t codec, uint8_t node, uint8_t stream, uint8_t channel)
{
    uint8_t val = stream << 4 | channel;
    SRCMD(codec, node, SetCONVCTRL, val);
}

void HDA::SetPin(uint8_t codec, uint8_t pin, uint8_t hphn, uint8_t out, uint8_t in, uint8_t vref, uint8_t ept)
{
    uint8_t val = hphn << 7 | out << 6 | in << 5 | vref << 2 | ept;
    SRCMD(codec, pin, SetPinCTRL, val);
}

uint8_t HDA::FindPin()
{
    for (int i = 0; i < numOfpins; i++)
    {
        uint8_t codec = 0;
        uint8_t pin = PINS[i];
        uint32_t def = SRCMD(codec, pin, GetPinWidgetConfigDefault, 0);

        if ((def >> 30) & 0b11 == 0b10)
        { // Integrated Speaker
            return pin;
        }
    }
}
/*
uint32_t cap = SRCMD(codec, widget, GetParameters, AudioWidgetCapabilities);
        WidgetCapabilities *wCap = (WidgetCapabilities *)&cap;

        if (wCap->type == 0x0)
*/

uint8_t HDA::GetPinSense(uint8_t codec, uint8_t node)
{
    uint32_t res = SRCMD(codec, node, HDAGetPinSense, 0);
    return res >> 31;
}
uint16_t HDA::GetFormat(uint32_t frequency, uint32_t bps, uint32_t channels){
    uint16_t payload = 0;
    switch (frequency){
        //Base 0 (48kHz)
        case 48000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b000 << 8; //Div
            break;
        case 96000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b001 << 11; //Mult (x2)
            payload |= 0b000 << 8; //Div
            break;
        case 144000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b010 << 11; //Mult (x3)
            payload |= 0b000 << 8; //Div
            break;
        case 192000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b011 << 11; //Mult (x4)
            payload |= 0b000 << 8; //Div
            break;
        case 24000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b001 << 8; //Div (/2)
            break;
        case 16000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b010 << 8; //Div (/3)
            break;
        case 9600:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b100 << 8; //Div (/5)
            break;
        case 8000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b101 << 8; //Div (/6)
            break;
        case 6000:
            payload |= 0b000 << 14; //Base (48 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b111 << 8; //Div (/6)
            break;

        //Base 1 (44.1kHz)
        case 44100:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b00 << 0; //Div
            break;
        case 88200:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b001 << 11; //Mult (x2)
            payload |= 0b00 << 0; //Div
            break;
        case 176000:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b011 << 11; //Mult (x4)
            payload |= 0b00 << 0; //Div
            break;
        case 22050:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b001 << 0; //Div (/2)
            break;
        case 32000:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b010 << 0; //Div (/3)
            break;
        case 11250:
            payload |= 0b001 << 14; //Base (44.1 kHz)
            payload |= 0b000 << 11; //Mult
            payload |= 0b011 << 0; //Div (/4)
            break;
    }

    switch(bps){
        case 8:
            payload |= 0b000 << 4;
            break;
        case 16:
            payload |= 0b001 << 4;
            break;
        case 20:
            payload |= 0b010 << 4;
            break;
        case 24:
            payload |= 0b011 << 4;
            break;
        case 32:
            payload |= 0b100 << 4;
            break;
    }

    payload |= (channels - 1);
    return payload;
}
uint8_t *BDL;
uint8_t *buff;
uint64_t offset = 0;
size_t sizeOfBuffer = 0;

uint16_t nextchunk = 0;

int HDA::RefillBuffer()
{
    uint8_t *Stream = (uint8_t *)(bar0 + 0x80 + (0x20 * InStreams));
    if (offset > sizeOfBuffer) {
        globalRenderer->printf("OFF: %d SZ: %d\n", offset, sizeOfBuffer);

        *Stream &= ~0b10; // Stop the stream
        return -1;
    }

    if ((*Stream & 0b10) != 0b10){
       //globalRenderer->printf("Stream not running\n");
        while(1);
    }

    for (int i = nextchunk; i < nextchunk + 32; i++)
    {
        BufferDescriptor *bd = (BufferDescriptor *)((uint8_t *)BDL + (i * 0x10));
        bd->IOC = ((i + 1) % 32 == 0) ? 1 : 0;
        if (offset > sizeOfBuffer) {
            memset((void*)bd->address, 0, 128);
            continue;
        }
        memcpy_simd((void *)bd->address, (void *)((uint64_t)buff + offset), 128);
        offset += 128;
    }
    asm("wbinvd");
    nextchunk += 32;
    if (nextchunk >= 255)
    {
        nextchunk = 0;
    }
    *(Stream + 0x03) |= 1 << 2;
    while ((*(Stream + 0x03) & (1 << 2)))

    asm volatile("mfence" ::: "memory"); // Ensure memory writes are completed
    return 1;
}



void HDA::Play(uint8_t *data, size_t sz, uint32_t frequency, uint32_t bps, uint32_t channels)
{

    buff = data;
    sizeOfBuffer = sz;
    offset = 0;
    //*((uint32_t *)(bar0 + 0x20)) |= (1 << 31) | 0b11111111;
    uint8_t *Stream = (uint8_t *)(bar0 + 0x80 + (0x20 * InStreams));
    *Stream &= ~0b10; // Stop the stream
    *Stream |= 1;     // Reset the stream
    Sleep(1);
    *Stream &= ~1;
    while (*Stream & 0b1);
    while (*Stream & 0b10);
   //globalRenderer->printf("Stream Reset!\n");
    void *bdbuff = GlobalAllocator.RequestPages((uint16_t)(((1024 * 128) / 0x1000) + 1));
    for (int i = 0; i < 256; i++)
    {
        BufferDescriptor *bd = (BufferDescriptor *)((uint8_t *)BDL + (i * 0x10));
        bd->address = (uint64_t)bdbuff + offset;
        if (bd->address & 0x7F) {
           //globalRenderer->printf("Not aligned to 128 bytes!\n");
        }
        memset(bdbuff, 0, (((128 * 256) / 0x1000) + 1) * 0x1000);
        memcpy((void *)((uint64_t)bd->address), (void *)((uint64_t)data + offset), 128);
        bd->size = 128;
        bd->IOC = ((i + 1) % 32 == 0) ? 1 : 0;
        offset += 128;
    }

    asm("wbinvd");


    *(uint32_t *)(Stream + 0x18) = (uint32_t)(uint64_t)BDL;
    *(uint32_t *)(Stream + 0x1C) = (uint32_t)((uint64_t)BDL >> 32);

    *(uint32_t *)(Stream + 0x08) = 128 * 256;
    *(uint16_t *)(Stream + 0x0C) = 255;

    uint16_t StreamFormat = GetFormat(frequency, bps, channels);
    *(uint16_t *)(Stream + 0x12) = StreamFormat;

    *(Stream + 0x02) &= 0b00001111;
    *(Stream + 0x02) |= 4 << 4;
    *(Stream + 0x03) = 0x1C;

    SRCMD16(0, hda_output_amp_node_number, 0x2, StreamFormat);
    SRCMD(0, hda_output_amp_node_number, 0x706, 0x40);

    Sleep(100);

    *Stream |= 1 << 2;
    *Stream |= 0b10;
    while ((*Stream & 0b10) != 0b10);

   //globalRenderer->printf("DAC CTRL: %x, DAC FORMAT: %x, PIN CONTROL: %x\n", SRCMD(0, hda_output_amp_node_number, 0xF06, 0), SRCMD(0, hda_output_amp_node_number, 0xA00, 0), SRCMD(0, 33, 0xF07, 0));

    while (true)
    {
        if ((*(Stream + 0x03) & 0xF) != 0)
        {
            if(RefillBuffer() == -1){
                break;
            } 
        }
    };

   //globalRenderer->printf(0x0FF000, "CAP: %hh\n", *(Stream + 0x03));

   //globalRenderer->printf(0xFF0000, "STREAM CONFIGURATION: \n  Stream Format: %h | Stream Control 2: %hh", *(uint16_t *)(Stream + 0x12), *(Stream + 0x02));
}

uint32_t HDA::GetNodeType(int codec, int node)
{
    uint32_t vcap = SRCMD(codec, node, GetParameters, AudioWidgetCapabilities);
    WidgetCapabilities *wCap = (WidgetCapabilities *)&vcap;
    return wCap->type;
}

void HDA::hda_set_node_gain(uint8_t codec, uint8_t node, uint8_t node_type, uint32_t capabilities, uint8_t gain) {
    uint16_t payload = 0x00;
    if (node_type == 0x1){
        payload |= 1 << 15;
        payload |= 0b11 << 12;
    }else{
        payload |= 1 << 14;
        payload |= 0b11 << 12;
    }
    
    //set number of gain
    if(gain==0 && (capabilities & 0x80000000)==0x80000000) {
     payload |= 0x80; //mute
    }
    else {
     payload |= (((capabilities>>8) & 0x7F)*(gain/100)); //recalculate range 0-100 to range of node steps
    }
    
    payload &= ~(1 << 7);
    //change gain
    SRCMD16(codec, node, 0x3, payload);
}

void HDA::InitializeDAC(uint8_t codec, uint8_t node)
{
   globalRenderer->printf("Initializing DAC %d\n", node);

    // turn on power for Audio Output
    SRCMD(codec, node, 0x705, 0x00);

    // disable unsolicited responses
    SRCMD(codec, node, 0x708, 0x00);

    // disable any processing
    SRCMD(codec, node, 0x703, 0x00);

    // connect Audio Output to stream 1 channel 0
    SRCMD(codec, node, 0x706, 0x10);

    uint32_t audio_output_amp_capabilities = SRCMD(codec, node, 0xF00, 0x12);
    hda_set_node_gain(codec, node, 0x1, audio_output_amp_capabilities, 100);
    if(audio_output_amp_capabilities!=0) {
        //we will control volume by Audio Output node
        hda_output_amp_node_number = node;
        hda_output_amp_node_capabilities = audio_output_amp_capabilities;
    }

     //because we are at end of node path, log all gathered info
    globalRenderer->printf("Sample Capabilites: %x\n", hda_audio_output_node_sample_capabilities);
    globalRenderer->printf("Stream Format Capabilites: %x\n", hda_audio_output_node_stream_format_capabilities);
    globalRenderer->printf("Volume node: %hh\n", hda_output_amp_node_number);
    globalRenderer->printf("Volume capabilities: %x\n", hda_output_amp_node_capabilities);

}

void HDA::InitializeAudioSelector(uint8_t codec, uint8_t node)
{
    globalRenderer->printf("Initializing Selector %d\n", node);

    // turn on power for Audio Selector
    SRCMD(codec, node, 0x705, 0x00);

    // disable unsolicited responses
    SRCMD(codec, node, 0x708, 0x00);

    // disable any processing
    SRCMD(codec, node, 0x703, 0x00);

    uint32_t audio_selector_amp_capabilities = SRCMD(codec, node, 0xF00, 0x12);
    hda_set_node_gain(codec, node, 0x1, audio_selector_amp_capabilities, 100);
    if(audio_selector_amp_capabilities!=0) {
        //we will control volume by Audio Selector node
        hda_output_amp_node_number = node;
        hda_output_amp_node_capabilities = audio_selector_amp_capabilities;
    }

    uint32_t first_connected_node_number = GetConnectedNodes(codec, node)->connections[0]; // get first node number
    uint32_t type_of_first_connected_node = GetNodeType(codec, first_connected_node_number);                      // get type of first node
    if (type_of_first_connected_node == 0x00)
    {
        InitializeDAC(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x02)
    {
        InitializeMixer(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x03)
    {
        InitializeAudioSelector(codec, first_connected_node_number);
    }
}

void HDA::InitializeMixer(uint8_t codec, uint8_t node)
{
    globalRenderer->printf("Initializing Mixer %d\n", node);

    // turn on power for Audio Mixer
    SRCMD(codec, node, 0x705, 0x00);

    // disable unsolicited responses
    SRCMD(codec, node, 0x708, 0x00);

    uint32_t first_connected_node_number = GetConnectedNodes(codec, node)->connections[0]; // get first node number
    uint32_t type_of_first_connected_node = GetNodeType(codec, first_connected_node_number);                      // get type of first node
    uint32_t audio_mixer_amp_capabilities = SRCMD(codec, node, 0xF00, 0x12);
    hda_set_node_gain(codec, node, 0x1, audio_mixer_amp_capabilities, 100);
    if(audio_mixer_amp_capabilities!=0) {
        //we will control volume by Audio Mixer node
        hda_output_amp_node_number = node;
        hda_output_amp_node_capabilities = audio_mixer_amp_capabilities;
    }
   
    if (type_of_first_connected_node == 0x00)
    {
        InitializeDAC(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x02)
    {
        InitializeMixer(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x03)
    {
        InitializeAudioSelector(codec, first_connected_node_number);
    }
}

void HDA::InitializeOutPin(uint8_t codec, uint8_t node)
{
    hda_audio_output_node_number = 0;
    hda_audio_output_node_sample_capabilities = 0;
    hda_audio_output_node_stream_format_capabilities = 0;
    hda_output_amp_node_number = 0;
    hda_output_amp_node_capabilities = 0;


    globalRenderer->printf("Initializing PIN %d\n", node);
    // turn on power for PIN
    SRCMD(codec, node, 0x705, 0x00);

    // disable unsolicited responses
    SRCMD(codec, node, 0x708, 0x00);

    // disable any processing
    SRCMD(codec, node, 0x703, 0x00);

    // enable pin
    SRCMD(codec, node, 0x707, ((SRCMD(codec, node, 0xF07, 0x00) | 0x80 | 0x40)));


    // enable EAPD + L-R swap
    SRCMD(codec, node, 0x70C, 0x6);


    //set maximal volume for PIN
    uint32_t pin_output_amp_capabilities = SRCMD(codec, node, 0xF00, 0x12);
    hda_set_node_gain(codec, node, 0x1, pin_output_amp_capabilities, 100);
    if(pin_output_amp_capabilities!=0) {
        //we will control volume by PIN node
        hda_output_amp_node_number = node;
        hda_output_amp_node_capabilities = pin_output_amp_capabilities;
    }

    // start enabling path of nodes
    SRCMD(codec, node, 0x701, 0x00);

    uint32_t first_connected_node_number = GetConnectedNodes(codec, node)->connections[0]; // get first node number
    uint32_t type_of_first_connected_node = GetNodeType(codec, first_connected_node_number);                      // get type of first node

    if (type_of_first_connected_node == 0x00)
    {
        InitializeDAC(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x02)
    {
        InitializeMixer(codec, first_connected_node_number);
    }
    else if (type_of_first_connected_node == 0x03)
    {
        InitializeAudioSelector(codec, first_connected_node_number);
    }
}
HDA::HDA(PCI::PCIDeviceHeader *pciHdr)
{
    hdaInstances[numOfHDAInstances] = this;
    numOfHDAInstances++;

    pciHdr->Command |= 1 << 1 | 1 << 2;
   //globalRenderer->printf("cmd:: %x\n", pciHdr->Command);
    PCI::PCIHeader0 *hdr0 = (PCI::PCIHeader0 *)pciHdr;
    hdr = hdr0;
    bar0 = (hdr->BAR0 & 0xFFFFFFF0);
    globalPTM.MapMemory((void*)bar0, (void*)bar0);
    if ((hdr->BAR0 & 0b110) == 0b100){
       //globalRenderer->printf("64-bit address\n");
        bar0 += ((uint64_t)hdr->BAR1) << 32;
    }
    globalPTM.MapMemory((void*)bar0, (void*)bar0);
   //globalRenderer->printf("bar0: %x bar1: %x\n", hdr->BAR0, hdr->BAR1);
   //globalRenderer->printf("%llx\n", (uint64_t)bar0);
   //globalRenderer->printf("4-byte read at the ba: 0x%x\n", *(uint32_t*)bar0);
    uint32_t Cap = *(uint32_t*)bar0;
    OutStreams = (Cap >> 8) & 0xF;
    InStreams = (Cap >> 12) & 0xF;
    BiStreams = (Cap >> 3) & 0b11111;
   //globalRenderer->printf("64 Bit Addressing Support: %hh | OUT: %hh | IN: %hh | BI: %hh\n", Cap & 0x1, OutStreams, InStreams, BiStreams);
   //globalRenderer->printf("Spec version %d.%d\n", *(uint8_t*)(bar0 + 0x3), *(uint8_t*)(bar0 + 0x2));
    // SET IRQ ETC...

    Reset();



    ResetCORB();
    ResetRIRB();
    EnumerateCodecs();
    BDL = (uint8_t *)GlobalAllocator.RequestPages((uint16_t)10);


    if (pin_speaker_default_node_number != 0)
    {
        // initalize speaker
        if (pin_speaker_node_number != 0)
        {
            globalRenderer->printf("Init speaker\n");
            InitializeOutPin(0, pin_speaker_node_number); // initalize speaker with connected output device
            hda_pin_output_node_number = pin_speaker_node_number;                 // save speaker node number
        }
        else
        {
            globalRenderer->printf("Default speaker output\n");
            InitializeOutPin(0, pin_speaker_default_node_number); // initalize default speaker
            hda_pin_output_node_number = pin_speaker_default_node_number;                 // save speaker node number
        }

        // save speaker path
        hda_second_audio_output_node_number = hda_audio_output_node_number;
        hda_second_audio_output_node_sample_capabilities = hda_audio_output_node_sample_capabilities;
        hda_second_audio_output_node_stream_format_capabilities = hda_audio_output_node_stream_format_capabilities;
        hda_second_output_amp_node_number = hda_output_amp_node_number;
        hda_second_output_amp_node_capabilities = hda_output_amp_node_capabilities;

        // if codec has also headphone output, initalize it
        if (pin_headphone_node_number != 0)
        {
            globalRenderer->printf("Headphone output\n");
            InitializeOutPin(0, pin_headphone_node_number); // initalize headphone output
            hda_pin_headphone_node_number = pin_headphone_node_number;              // save headphone node number

            // if first path and second path share nodes, left only info for first path
            if (hda_audio_output_node_number == hda_second_audio_output_node_number)
            {
                hda_second_audio_output_node_number = 0;
            }
            if (hda_output_amp_node_number == hda_second_output_amp_node_number)
            {
                hda_second_output_amp_node_number = 0;
            }

            // find headphone connection status
            if (GetPinSense(0, hda_pin_headphone_node_number))
            {
                //hda_disable_pin_output(hda_codec_number, hda_pin_output_node_number);
                globalRenderer->printf(0xFF0000, "Disabling pin %d\n", hda_pin_output_node_number);
                SRCMD(0, hda_pin_output_node_number, 0x707, (SRCMD(0, hda_pin_output_node_number, 0xF07, 0x00) & ~0x40));
                hda_selected_output_node = hda_pin_headphone_node_number;
            }
            else
            {
                hda_selected_output_node = hda_pin_output_node_number;
            }

            // add task for checking headphone connection
            //create_task(hda_check_headphone_connection_change, TASK_TYPE_USER_INPUT, 50);
        }
    }
    else if (pin_headphone_node_number != 0)
    { // codec do not have speaker, but only headphone output
        globalRenderer->printf("Headphone output\n");
        //hda_is_initalized_useful_output = STATUS_TRUE;
        InitializeOutPin(0, pin_headphone_node_number); // initalize headphone output
        hda_pin_output_node_number = pin_headphone_node_number;                 // save headphone node number
    }
    else if (pin_alternative_output_node_number != 0)
    { // codec have only alternative output
        globalRenderer->printf("Alternative output\n");
        //hda_is_initalized_useful_output = STATUS_FALSE;
        InitializeOutPin(0, pin_alternative_output_node_number); // initalize alternative output
        hda_pin_output_node_number = pin_alternative_output_node_number;                 // save alternative output node number
    }
    else
    {
        globalRenderer->printf("codec does not have any output PINs\n");
    }

    globalRenderer->printf("[HDA DRIVER] Initialization Completed successfully!\n");
}