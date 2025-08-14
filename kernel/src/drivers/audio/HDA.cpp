#include <drivers/audio/HDA.h>
#include <paging/PageFrameAllocator.h>
#include <scheduling/apic/apic.h>
#include <cstr.h>
#include <syscalls/syscalls.h>

/*
* If you are reading this, you are making a big mistake... leave while you can!
* I warned you!
* If I knew what I was doing before writing this it would be much much cleaner and readable
*/



#define FRAMES_PER_BD     64
#define BD_COUNT          64
#define INTERRUPT_FREQ    8


void HDA::Reset()
{ // Perform a cold reset
    volatile uint32_t *control = (volatile uint32_t *)(bar0 + 0x08);
    *control &= ~0x01;
    while (*control & 0x01);
    Sleep(1);
    *control |= 0x01;
    while ((*control & 0x01) == 0);
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
    while (*ctrl & 0b10);
    Sleep(1);

    uint8_t *size = (uint8_t *)(bar0 + 0x4E);
    uint8_t entries2 = (*size >> 4) & 0b0001;
    uint8_t entries16 = (*size >> 4) & 0b0010;
    uint8_t entries256 = (*size >> 4) & 0b0100;

    if (entries256 == 1 && (entries16 == 1 || entries2 == 1))
    { // We need to check if there is at least one more supported. If only one is then the register is read only

        *size &= ~0b11;
        *size |= 0b10;
    }
    else if (entries16 == 1 && entries2 == 1)
    { // We need to check if there is at least one more supported. If only one is then the register is read only

        *size &= ~0b11;
        *size |= 0b01;
    } // If none of those are true, then there is only one size supported, the register is Read Only and the value is set to 0 (2 entries)

    uint64_t mem = (uint64_t)GlobalAllocator.RequestPage(); // Allocate memory for the corb commands
    memset((void *)mem, 0, 0x2000);
    CORB = (uint64_t*)mem;

    volatile uint32_t *memLow = (volatile uint32_t *)(bar0 + 0x40);
    volatile uint32_t *memHigh = (volatile uint32_t *)(bar0 + 0x44);

    mem = globalPTM.getPhysicalAddress((void*)mem);

    *memLow = (uint32_t)(mem & 0xFFFFFFFF); // Set the low part of the memory address

    uint32_t Cap = *(uint32_t*)bar0;

    if (Cap & 0x1)
    {
        *memHigh = (uint32_t)((mem >> 32) & 0xFFFFFFFF); // Set the high part of the memory address
    }

    uint16_t *CORBRP = (uint16_t *)(bar0 + 0x4A);
    *CORBRP |= (1 << 15);

    while((*CORBRP) & (1 << 15) == 0);
    Sleep(1);
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
    while (*ctrl & 0b10);

    uint8_t *size = (uint8_t *)(bar0 + 0x5E);
    uint8_t entries2 = (*size >> 4) & 0b0001;
    uint8_t entries16 = (*size >> 4) & 0b0010;
    uint8_t entries256 = (*size >> 4) & 0b0100;

    if (entries256 == 1 && (entries16 == 1 || entries2 == 1))
    { // We need to check if there is at least one more supported. If only one is then the register is read only
        *size &= ~0b11;
        *size |= 0b10;
    }
    else if (entries16 == 1 && entries2 == 1)
    {
        *size &= ~0b11;
        *size |= 0b01;
    } // If none of those are true, then there is only one size supported, the register is Read Only and the value is set to 0 (2 entries)

    uint64_t mem = (uint64_t)GlobalAllocator.RequestPages((uint32_t)2); // Allocate memory for the RIRB
    memset((void *)mem, 0, 0x2000);

    volatile uint32_t *memLow = (volatile uint32_t *)(bar0 + 0x50);
    volatile uint32_t *memHigh = (volatile uint32_t *)(bar0 + 0x54);
    RIRB = (uint64_t *)mem;


    mem = globalPTM.getPhysicalAddress((void*)mem);

    *memLow = (uint32_t)(mem); // Set the low part of the memory address

    uint32_t Cap = *(uint32_t*)bar0;
    if (Cap & 0x1)
    {
        *memHigh = (uint32_t)((mem >> 32)); // Set the high part of the memory address
    }

    

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
        // kprintf(0xffde21, "[HDA DRIVER] (%d) CONNECTION [%d]: %d\n", widget, lf ? i - 3 : i - 1, connections[lf ? i - 3 : i - 1]);
    }

    connList *list = new connList;
    memcpy(list->connections, connections, entries * sizeof(uint8_t));
    list->size = entries;
    return list; // how tf did it work without this return statement...
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
            // kprintf(0xFF0000, "[HDA DRIVER] AFG Function Group Located at %hh\n", function);

            uint32_t cap = SRCMD(codec, function, GetParameters, SupportedPCMRates);
            hda_output_amp_node_rates = cap; // default rates/sizes

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
           //kprintf("[HDA DRIVER] CODEC LOCATED (%d)[%x]\n", codec, res);
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
    return 0;
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

void hda_tsk(HDA* h);
void HDA::Prepare(){
    if (hda_task != nullptr) task_scheduler::remove_task(hda_task);

    hda_task = task_scheduler::create_process("HDA Driver", (function)hda_tsk);
    hda_task->rdi = (uint64_t)this;
    task_scheduler::mark_ready(hda_task);

    running = false;
    // to-do clear buffers
    
    // reset the stream and prepare the dac
    asm ("sti");
    uint8_t *Stream = (uint8_t *)(bar0 + 0x80 + (0x20 * InStreams));
    *Stream &= ~0b10; // Stop the stream
    *Stream |= 1;     // Reset the stream
    Sleep(1);
    *Stream &= ~1;
    while (*Stream & 0b1);
    while (*Stream & 0b10);


    *(uint16_t *)(Stream + 0x12) = selected_format;

    *(Stream + 0x02) &= 0b00001111;
    *(Stream + 0x02) |= 4 << 4;
    *(Stream + 0x03) = 0x1C;

    if (Buffer != nullptr) free(Buffer);
    Buffer = new uint64_t[4096];
    memset(Buffer, 0, 4096 * sizeof(uint64_t));
    BufferSz = 4096;
    BufferRP = 0;
    BufferWP = 0;

    SRCMD16(0, hda_output_amp_node_number, 0x2, selected_format);
    SRCMD(0, hda_output_amp_node_number, 0x706, 0x40);

    bd_list = (BufferDescriptor*)GlobalAllocator.RequestPage();
    memset(bd_list, 0, 0x1000);

    uint64_t bd_list_physical = globalPTM.getPhysicalAddress(bd_list);

    *(uint32_t *)(Stream + 0x18) = (uint32_t)bd_list_physical;
    *(uint32_t *)(Stream + 0x1C) = (uint32_t)(bd_list_physical >> 32);
    *(uint32_t *)(Stream + 0x08) = FRAMES_PER_BD * BD_COUNT * selected_frame_size;
    *(uint16_t *)(Stream + 0x0C) = BD_COUNT - 1;
    //kprintf("DAC CTRL: %x, DAC FORMAT: %x, PIN CONTROL: %x\n", SRCMD(0, hda_output_amp_node_number, 0xF06, 0), SRCMD(0, hda_output_amp_node_number, 0xA00, 0), SRCMD(0, 33, 0xF07, 0));
    for (int i = 0; i < BufferSz; i++){
        if (Buffer[i] != NULL){
            free((void*)Buffer[i]);
        }
        Buffer[i] = (uint64_t)malloc(FRAMES_PER_BD * selected_frame_size);
    }
    refill_needeed = true;
}

void HDA::QueueSample(void* sample){
    memcpy_simd((void*)Buffer[BufferWP], sample, selected_frame_size * FRAMES_PER_BD);
    BufferWP++;
    if (BufferWP >= BufferSz) BufferWP = 0;
}

uint32_t HDA::PlaySample(uint8_t* samples, size_t sample_cnt){
    sample_cnt = DIV_ROUND_UP(sample_cnt, FRAMES_PER_BD);

    size_t written_samples = 0;
    while(refill_needeed == false);

    for (int i = 0; i < sample_cnt; i++){
        uint8_t* sample = samples + (i * selected_frame_size * FRAMES_PER_BD);
        QueueSample(sample);
        written_samples += FRAMES_PER_BD;
    }

    if (running == false){
        if (BufferWP < BD_COUNT) return written_samples;
        running = true;
        // Prefill
        uint8_t *Stream = (uint8_t *)(bar0 + 0x80 + (0x20 * InStreams));
        uint64_t dma_buffer = (uint64_t)GlobalAllocator.RequestPages(DIV_ROUND_UP(FRAMES_PER_BD * BD_COUNT * selected_frame_size, 0x1000));
        dma_buffer = virtual_to_physical(dma_buffer);
        for (int i = 0; i < BD_COUNT; i++){
            bd_list[i].address = dma_buffer;
            bd_list[i].size = FRAMES_PER_BD * selected_frame_size;
            dma_buffer += bd_list[i].size;
            bd_list[i].IOC = ((i + 1) % INTERRUPT_FREQ) == 0 ? 1 : 0;
            memcpy_simd((void*)physical_to_virtual(bd_list[i].address), (void*)Buffer[BufferRP], bd_list[i].size);
            BufferRP++;
        }
        next_bd_refill = 0;
        asm("wbinvd");
        Sleep(100);
        //Start
        *Stream |= (1 << 2);
        *Stream |= (1 << 1);
        while((*Stream & 0b10) == 0);
        task_scheduler::unblock(hda_task);
    }else{
        refill_needeed = false;
        task_scheduler::change_task(hda_task);
    }
    return written_samples;
}


void hda_tsk(HDA* h){
    HDA* hda = h;
    uint8_t *Stream = (uint8_t *)(hda->bar0 + 0x80 + (0x20 * hda->InStreams));

    while(true){
        if (hda->BufferRP >= hda->BufferSz) hda->BufferRP = 0;
        
        if (hda->running == false){ // Stream is stopped
            task_scheduler::yield();
        }

        if (hda->BufferWP == (hda->BufferRP + INTERRUPT_FREQ) || hda->BufferWP == hda->BufferRP) hda->refill_needeed = true; // keep it filled for the next iteration
        while((*(Stream + 0x03) & 0xF) == 0){
            hda->hda_task->counter = 0;
            //__asm__ __volatile__ ("int $0x23");
        }

        hda->hda_task->counter = 10;
        int last_bd = hda->next_bd_refill + INTERRUPT_FREQ;
        for (int i = hda->next_bd_refill; i < last_bd; i++){
            while (hda->BufferWP == hda->BufferRP) hda->refill_needeed = true;
            memcpy_simd((void*)physical_to_virtual(hda->bd_list[i].address), (void*)hda->Buffer[hda->BufferRP], hda->bd_list[i].size);
            hda->bd_list[i].IOC = ((i + 1) % INTERRUPT_FREQ) == 0 ? 1 : 0;
            hda->BufferRP++;
            hda->next_bd_refill++;
        }
        if (hda->next_bd_refill > (BD_COUNT - 1)) hda->next_bd_refill = 0;
        asm("wbinvd");
        asm volatile("mfence" ::: "memory");

        *(Stream + 0x03) |= *(Stream + 0x03);
    }
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
   kprintf("Initializing DAC %d\n", node);

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
        uint32_t cap = SRCMD(codec, node, GetParameters, SupportedPCMRates);
        if (cap != 0) hda_output_amp_node_rates = cap;
    }

     //because we are at end of node path, log all gathered info
    kprintf("Sample Capabilites: %x\n", hda_audio_output_node_sample_capabilities);
    kprintf("Stream Format Capabilites: %x\n", hda_audio_output_node_stream_format_capabilities);
    kprintf("Volume node: %hh\n", hda_output_amp_node_number);
    kprintf("Volume capabilities: %x\n", hda_output_amp_node_capabilities);

}

void HDA::InitializeAudioSelector(uint8_t codec, uint8_t node)
{
    kprintf("Initializing Selector %d\n", node);

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
    kprintf("Initializing Mixer %d\n", node);

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


    kprintf("Initializing PIN %d\n", node);
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
int snd_dev = -1;

HDA::HDA(PCI::PCIDeviceHeader *pciHdr)
{
    snd_dev++;
    card_num = snd_dev;

    pciHdr->Command |= 1 << 1 | 1 << 2;
   //kprintf("cmd:: %x\n", pciHdr->Command);
    PCI::PCIHeader0 *hdr0 = (PCI::PCIHeader0 *)pciHdr;
    hdr = hdr0;
    bar0 = (hdr->BAR0 & 0xFFFFFFF0);
    if ((hdr->BAR0 & 0b110) == 0b100){
       //kprintf("64-bit address\n");
        bar0 += ((uint64_t)hdr->BAR1) << 32;
    }

    void* physical = (void*)bar0;
    bar0 = physical_to_virtual(bar0);

    globalPTM.MapMemory((void*)bar0, physical);
    globalPTM.SetPageFlag((void*)bar0, PT_Flag::CacheDisabled, true);
   
    uint32_t Cap = *(uint32_t*)bar0;
    OutStreams = (Cap >> 8) & 0xF;
    InStreams = (Cap >> 12) & 0xF;
    BiStreams = (Cap >> 3) & 0b11111;

    Reset();
    ResetCORB();
    ResetRIRB();
    EnumerateCodecs();

    bool has_output = true;
    if (pin_speaker_default_node_number != 0)
    {
        // initalize speaker
        if (pin_speaker_node_number != 0)
        {
            kprintf("Init speaker\n");
            InitializeOutPin(0, pin_speaker_node_number); // initalize speaker with connected output device
            hda_pin_output_node_number = pin_speaker_node_number;                 // save speaker node number
        }
        else
        {
            kprintf("Default speaker output\n");
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
            kprintf("Headphone output\n");
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
                kprintf(0xFF0000, "Disabling pin %d\n", hda_pin_output_node_number);
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
        kprintf("Headphone output\n");
        //hda_is_initalized_useful_output = STATUS_TRUE;
        InitializeOutPin(0, pin_headphone_node_number); // initalize headphone output
        hda_pin_output_node_number = pin_headphone_node_number;                 // save headphone node number
    }
    else if (pin_alternative_output_node_number != 0)
    { // codec have only alternative output
        kprintf("Alternative output\n");
        //hda_is_initalized_useful_output = STATUS_FALSE;
        InitializeOutPin(0, pin_alternative_output_node_number); // initalize alternative output
        hda_pin_output_node_number = pin_alternative_output_node_number;                 // save alternative output node number
    }
    else
    {
        has_output = false;
        kprintf("codec does not have any output PINs\n");
    }

    if (has_output){
        RegisterControl();
        RegisterOutput();
    }

    

    kprintf("Supported rates: %x\n", hda_output_amp_node_rates);
    kprintf("Lowest supported rate: %d\n", LowestSupportedRate());
    kprintf("Highest supported rate: %d\n", HighestSupportedRate());
    kprintf("Lowest supported sample size: %d\n", LowestSupportedSampleSize());
    kprintf("Highest supported sample size: %d\n", HighestSupportedSampleSize());
    kprintf("[HDA DRIVER] Initialization Completed successfully!\n");
}


int hda_ioctl(int op, char* argp, vnode_t* node){
    HDA* hda = (HDA*)node->misc_data[0];
    uint64_t* arg = (uint64_t*)argp;

    switch ((unsigned int)op){
        case SNDRV_HWDEP_IOCTL_PVERSION: {
            *arg = SNDRV_HWDEP_VERSION;
            break; 
        }

        case SNDRV_HWDEP_IOCTL_INFO: {
            struct snd_hwdep_info info;
            
            memset(&info, 0, sizeof(info));
            info.card = hda->card_num;
            strcpy((char*)info.name, "NAME");
            strcpy((char*)info.id, "ID");
            info.iface = 0;
            memcpy_simd(arg, &info, sizeof(info));
            break;
        }
        case SNDRV_CTL_IOCTL_CARD_INFO: {
            struct snd_ctl_card_info info;
            info.card = hda->card_num;
            strcpy((char*)info.driver, "dioOS-HDA");
            strcpy((char*)info.name, (char*)PCI::GetDeviceName(hda->hdr->Header.VendorID, hda->hdr->Header.DeviceID));
            strcpy((char*)info.longname, (char*)PCI::GetVendorName(hda->hdr->Header.VendorID));
            strcat((char*)info.name, " - ");
            strcat((char*)info.name, (char*)PCI::GetDeviceName(hda->hdr->Header.VendorID, hda->hdr->Header.DeviceID));
            strcpy((char*)info.mixername, (char*)PCI::GetDeviceName(hda->hdr->Header.VendorID, hda->hdr->Header.DeviceID));
            strcpy((char*)info.components, "HDA"); // I am just guessing at this point
            break;
        }
        case SNDRV_CTL_IOCTL_PVERSION: {
            *(int*)arg = SNDRV_CTL_VERSION;
            break;
        }
        case SNDRV_CTL_IOCTL_PCM_PREFER_SUBDEVICE: {
            *(int*)arg = 0;
            break;
        }
        case SNDRV_CTL_IOCTL_ELEM_LIST: {
            serialf("%llx %llx\n", argp, arg);
            struct snd_ctl_elem_list* list = (struct snd_ctl_elem_list*)argp;
            list->count = 1;
            list->used = 0;
            if (list->space > 0){
                list->used = 1;
                list->pids[0].iface = 0;
                list->pids[0].device = hda->card_num;
                list->pids[0].subdevice = 0;
                list->pids[0].index = 0;
                strcpy((char*)list->pids[0].name, "Master");
            }
            break;
        }
        case SNDRV_CTL_IOCTL_SUBSCRIBE_EVENTS:{
            int a = *arg;
            node->data_read = false;
            break;
        }
        default:{
            return -ENOSYS;
        }
    }
    return 0;
}
int HDA::RegisterControl(){
    char name[48] = {};
    strcpy(name, "controlC");
    strcat(name, toString((uint64_t)snd_dev));
    
    vnode_t* dev = vfs::resolve_path("/dev");
    if (dev == nullptr) dev = vfs::mount_node("dev", VDIR, vfs::get_root_node());

    vnode_t* snd = vfs::resolve_path("/dev/snd");
    if (snd == nullptr) snd = vfs::mount_node("snd", VDIR, dev);

    vnode_t* control = vfs::mount_node(name, VCHR, snd);
    control->ops.iocntl = hda_ioctl;
    control->misc_data[0] = this;

    return 0;
}

static inline void snd_mask_none(struct snd_mask *mask)
{
	memset(mask, 0, sizeof(*mask));
}


static inline void snd_mask_set(struct snd_mask *mask, unsigned int val)
{
	mask->bits[MASK_OFS(val)] |= MASK_BIT(val);
}

static inline bool snd_mask_isset(struct snd_mask *mask, unsigned int val)
{
	return (mask->bits[MASK_OFS(val)] &= MASK_BIT(val)) != 0;
}

int HDA_SUPPORT_RATE_BIT_TO_FREQ[] = {
    8000, 11025, 16000, 22050,
    32000, 44100, 48000, 88200,
    96000, 176400, 192000, 384000  
};

int HDA::LowestSupportedRate(){
    // bits 11:0 are the supported rates (HDA spec rev. 1.0a page 206)
    for (int i = 0; i <= 11; i++){
        if (hda_output_amp_node_rates & (1 << i)) return HDA_SUPPORT_RATE_BIT_TO_FREQ[i];
    }
    return -1;
}

int HDA::HighestSupportedRate(){
    // bits 11:0 are the supported rates (HDA spec rev. 1.0a page 206)
    for (int i = 11; i >= 0; i--){
        if (hda_output_amp_node_rates & (1 << i)) return HDA_SUPPORT_RATE_BIT_TO_FREQ[i];
    }
    return -1;
}

int HDA_SUPPORT_SAMPLE_BIT[] = {
    8, 16, 20, 24, 32
};


int HDA::LowestSupportedSampleSize(){
    for (int i = 0; i < 5; i++){
        if (hda_output_amp_node_rates & (1 << (i + 16))) return HDA_SUPPORT_SAMPLE_BIT[i];
    }
    return -1;
}

int HDA::HighestSupportedSampleSize(){
    for (int i = 4; i >= 0; i--){
        if (hda_output_amp_node_rates & (1 << (i + 16))) return HDA_SUPPORT_SAMPLE_BIT[i];
    }
    return -1;
}




void HDA::hwparams(snd_pcm_hw_params* params){
    struct snd_interval *rate = &params->intervals[SNDRV_PCM_HW_PARAM_RATE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    unsigned int requested_rate = rate->min;
    rate->max = requested_rate;

    params->rate_num = requested_rate;
    params->rate_den = 1;

    struct snd_interval *channels = &params->intervals[SNDRV_PCM_HW_PARAM_CHANNELS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    unsigned int requested_channels = channels->max;
    channels->min = requested_channels;

    
    struct snd_interval *bits = &params->intervals[SNDRV_PCM_HW_PARAM_SAMPLE_BITS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    struct snd_mask *fmt = &params->masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK];
    int sample_size = 16;

    if (snd_mask_isset(fmt, SNDRV_PCM_FORMAT_S32_LE)){
        sample_size = 32;
    }else if (snd_mask_isset(fmt, SNDRV_PCM_FORMAT_S24_LE)){
        sample_size = 24;
    }else if (snd_mask_isset(fmt, SNDRV_PCM_FORMAT_S16_LE)){
        sample_size = 16;
    }else if (snd_mask_isset(fmt, SNDRV_PCM_FORMAT_S8)){
        sample_size = 8;
    }

    if (sample_size == 0){
        sample_size = bits->max;
        bits->min = sample_size;
    }

    if (sample_size == 8){
        snd_mask_set(fmt, SNDRV_PCM_FORMAT_S8);
    }else if (sample_size == 16){
        snd_mask_set(fmt, SNDRV_PCM_FORMAT_S16_LE);
    }else if (sample_size == 24){
        snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);
    }else if (sample_size == 32){
        snd_mask_set(fmt, SNDRV_PCM_FORMAT_S32_LE);
    }

    params->msbits = sample_size;

    uint32_t frame_bits = sample_size * requested_channels;
    uint32_t frame_bytes = DIV_ROUND_UP(frame_bits, 8);
    struct snd_interval *frame = &params->intervals[SNDRV_PCM_HW_PARAM_FRAME_BITS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    frame->min = frame->max = frame_bytes * 8;

    struct snd_interval *period_sz = &params->intervals[SNDRV_PCM_HW_PARAM_PERIOD_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    period_sz->max = period_sz->min = (FRAMES_PER_BD * BD_COUNT) / INTERRUPT_FREQ;

    struct snd_interval *period_bytes = &params->intervals[SNDRV_PCM_HW_PARAM_PERIOD_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    period_bytes->min = period_bytes->max = period_sz->min * frame_bytes;

    

    struct snd_interval *periods = &params->intervals[SNDRV_PCM_HW_PARAM_PERIODS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    periods->min = periods->max = BD_COUNT / INTERRUPT_FREQ;

    unsigned int period_time_us = (1000000 * period_sz->min) / requested_rate;
    struct snd_interval *period_time = &params->intervals[SNDRV_PCM_HW_PARAM_PERIOD_TIME - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    period_time->max = period_time->min = period_time_us;


    struct snd_interval *buffer_size = &params->intervals[SNDRV_PCM_HW_PARAM_BUFFER_SIZE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    buffer_size->min = buffer_size->max = FRAMES_PER_BD * BD_COUNT;


    struct snd_interval *buffer_bytes = &params->intervals[SNDRV_PCM_HW_PARAM_BUFFER_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    buffer_bytes->min = buffer_bytes->max = buffer_size->max * frame_bytes;


    selected_frame_size = frame_bytes;
    
    struct snd_interval *buffer_time = &params->intervals[SNDRV_PCM_HW_PARAM_BUFFER_TIME - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    unsigned int buffer_time_us = period_time->min * periods->min;
    buffer_time->min = buffer_time->max = buffer_time_us;


    params->info = SNDRV_PCM_INFO_INTERLEAVED;
    params->cmask = params->rmask;
    selected_format = GetFormat(requested_rate, sample_size, requested_channels);
}


int hda_pcm_ioctl(int op, char* argp, vnode_t* node){
    HDA* hda = (HDA*)node->misc_data[0];
    uint64_t* arg = (uint64_t*)argp;

    switch ((unsigned int)op){
        case SNDRV_PCM_IOCTL_INFO:
            struct snd_pcm_info info;
            info.device = hda->card_num;
            info.subdevice = 0;
            info.stream = SNDRV_PCM_STREAM_PLAYBACK;
            strcpy((char*)info.name, "HDA output device");
            strcpy((char*)info.subname, "HDA output device");
            strcpy((char*)info.id, "HDA output device");

            info.dev_class = hda->hdr->Header.Class;
            info.dev_subclass = hda->hdr->Header.Subclass;
            memcpy_simd(arg, &info, sizeof(info));
            return 0;
        case SNDRV_PCM_IOCTL_PVERSION:
            *(int*)arg = SNDRV_PCM_VERSION;
            return 0;
        case SNDRV_PCM_IOCTL_USER_PVERSION:
            *(int*)arg = SNDRV_PCM_VERSION;
            return 0;
        case SNDRV_PCM_IOCTL_TTSTAMP:
            return 0;
        case SNDRV_PCM_IOCTL_HW_REFINE: {
            struct snd_pcm_hw_params* params = (struct snd_pcm_hw_params*)arg;

            unsigned int highest_rate = hda->HighestSupportedRate();
            unsigned int lowest_rate = hda->LowestSupportedRate();
            unsigned int highest_sample = hda->HighestSupportedSampleSize();
            unsigned int lowest_sample = hda->LowestSupportedSampleSize();
            
            if (params->rmask & SNDRV_PCM_HW_PARAM_RATE){
                // Set rate interval
                struct snd_interval *rate = &params->intervals[SNDRV_PCM_HW_PARAM_RATE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
                *rate = {
                    .min = (rate->min < lowest_rate || rate->min > highest_rate) ? lowest_rate : rate->min,
                    .max = (rate->max > highest_rate || rate->max < lowest_rate) ? highest_rate : rate->max,
                    .integer = 1,
                    .empty = 0
                };
                params->cmask |= SNDRV_PCM_HW_PARAM_RATE;
            }


            if (params->rmask & SNDRV_PCM_HW_PARAM_CHANNELS){
                // Set channels (e.g. stereo)
                struct snd_interval *chans = &params->intervals[SNDRV_PCM_HW_PARAM_CHANNELS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
                *chans = {
                    .min = (chans->min < 1 || chans->min > 2) ? 1 : chans->min,
                    .max = (chans->max > 2 || chans->max < 1) ? 2 : chans->max,
                    .integer = 1,
                    .empty = 0,
                };
                params->cmask |= SNDRV_PCM_HW_PARAM_CHANNELS;
            }

            if (params->rmask & SNDRV_PCM_HW_PARAM_SAMPLE_BITS){
                // Set sample bits
                struct snd_interval *bits = &params->intervals[SNDRV_PCM_HW_PARAM_SAMPLE_BITS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
                *bits = {
                    .min = (bits->min < lowest_sample || bits->min > highest_sample) ? lowest_sample : bits->min,
                    .max = (bits->max > highest_sample || bits->max < lowest_sample) ? highest_sample : bits->max,
                    .integer = 1,
                    .empty = 0,
                };
                params->cmask |= SNDRV_PCM_HW_PARAM_SAMPLE_BITS;
            }

            if (params->rmask & SNDRV_PCM_HW_PARAM_FORMAT){
                // Format mask
                struct snd_mask *fmt = &params->masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK];
                snd_mask_none(fmt);
                if (hda->hda_output_amp_node_rates & (1 << 16)) snd_mask_set(fmt, SNDRV_PCM_FORMAT_S8);
                if (hda->hda_output_amp_node_rates & (1 << 17)) snd_mask_set(fmt, SNDRV_PCM_FORMAT_S16_LE);
                if (hda->hda_output_amp_node_rates & (1 << 19)) snd_mask_set(fmt, SNDRV_PCM_FORMAT_S24_LE);
                if (hda->hda_output_amp_node_rates & (1 << 20)) snd_mask_set(fmt, SNDRV_PCM_FORMAT_S32_LE);
                params->cmask |= SNDRV_PCM_HW_PARAM_FORMAT;
            }

            if (params->rmask & SNDRV_PCM_HW_PARAM_PERIOD_BYTES){
                // Period + buffer sizes
                struct snd_interval *period = &params->intervals[SNDRV_PCM_HW_PARAM_PERIOD_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
                *period = {
                    .min = 1024,
                    .max = 8192,
                    .integer = 1,
                    .empty = 0,
                };
                params->cmask |= SNDRV_PCM_HW_PARAM_PERIOD_BYTES;
            }

            if (params->rmask & SNDRV_PCM_HW_PARAM_BUFFER_BYTES){
                struct snd_interval *buffer = &params->intervals[SNDRV_PCM_HW_PARAM_BUFFER_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
                *buffer = {
                    .min = 2048,
                    .max = 16384,
                    .integer = 1,
                    .empty = 0,
                };
                params->cmask |= SNDRV_PCM_HW_PARAM_BUFFER_BYTES;
            }

            if (params->rmask & SNDRV_PCM_HW_PARAM_SUBFORMAT){
                struct snd_mask *subformat = &params->masks[SNDRV_PCM_HW_PARAM_SUBFORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK];
                snd_mask_none(subformat);
                snd_mask_set(subformat, SNDRV_PCM_SUBFORMAT_STD);
                params->cmask |= SNDRV_PCM_HW_PARAM_SUBFORMAT;
            }

            params->info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_BLOCK_TRANSFER;
            params->cmask = params->rmask;
            return 0;
        }
        case SNDRV_PCM_IOCTL_HW_PARAMS: {
            hda->hwparams((struct snd_pcm_hw_params*)arg);
            return 0;
        }
        case SNDRV_PCM_IOCTL_HW_FREE:
            return 0;
        case SNDRV_PCM_IOCTL_SW_PARAMS:{
            return 0;
        }
        case SNDRV_PCM_IOCTL_PREPARE:
            hda->Prepare();
            return 0;
        case SNDRV_PCM_IOCTL_DROP:{
            hda->Prepare();
            return 0;
        }
        case SNDRV_PCM_IOCTL_WRITEI_FRAMES: {
            struct snd_xferi* data = (struct snd_xferi*)arg;
            data->result = hda->PlaySample((uint8_t*)data->buf, data->frames);
            return 0;
        }
    }
    return -ENOSYS;
}

int HDA::RegisterOutput(){
    char name[48] = {};
    strcpy(name, "pcmC");
    strcat(name, toString((uint64_t)snd_dev));
    strcat(name, "D0p"); // to-do add support for multiple ports

    vnode_t* dev = vfs::resolve_path("/dev");
    if (dev == nullptr) dev = vfs::mount_node("dev", VDIR, vfs::get_root_node());

    vnode_t* snd = vfs::resolve_path("/dev/snd");
    if (snd == nullptr) snd = vfs::mount_node("snd", VDIR, dev);

    vnode_t* pcm = vfs::mount_node(name, VCHR, snd);
    pcm->ops.iocntl = hda_pcm_ioctl;
    pcm->misc_data[0] = this;

    return 0;
}