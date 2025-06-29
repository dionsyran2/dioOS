#pragma once
#include <stdint.h>
#include "../../BasicRenderer.h"
#include "../../pci.h"


#define GetParameters 0xF00
#define GetPinWidgetConfigDefault 0xF1C
#define GetConnectionListEntry 0xF02
#define AMPGain 0x300
#define SetCONVFormat 0x200
#define SetCONVCTRL 0x706
#define SetPinCTRL 0x707
#define SetPWRState 0x705
#define HDAGetPinSense 0xF09


#define VendorIDP 0x00
#define RevisionIDP 0x02
#define NodeCount 0x04
#define FunctionGroupType 0x05
#define AudioGroupCapabilities 0x08
#define AudioWidgetCapabilities 0x09
#define SupportedPCMRates 0x0A
#define SupportedFormats 0x0B
#define PinCapabilities 0x0C
#define InputAmplifierCapabilities 0x0D
#define OutputAmplifierCapabilities 0x12
#define ConnectionListLength 0x0E
#define SupportedPowerStates 0x0F
#define ProcessingCapabilities 0x10
#define GPIOCount 0x11
#define VolumeCapabilities 0x13



namespace PCI {
    class PCIDeviceHeader; // Forward declaration
}

struct StreamDescriptor{
    uint8_t StreamControl0;
    uint8_t smtShouldBeHere; //Blame the osdev wiki
    uint8_t StreamControl2;
    uint8_t StreamStatus;
    uint32_t LinkPositionBufferEntry;
    uint32_t CyclicBufferLength; // size of all entries in Buffer Descriptor List added together  
    uint16_t LastValidIndex; // number of entries in Buffer Descriptor List + 1
    uint8_t reserved2[6];
    uint16_t StreamFormat;
    uint8_t reserved3[4]; 
    uint32_t BufferDescriptorLow;
    uint32_t BufferDescriptorHigh;
};


struct BufferDescriptor{
    uint64_t address;
    uint32_t size;
    uint32_t IOC:1;
    uint32_t rsv:31;
}__attribute__((packed));

struct WidgetCapabilities{
    uint8_t stereo:1;
    uint8_t inputAMP:1;
    uint8_t outputAMP:1;
    uint8_t AMPOverride:1;
    uint8_t formatOverride:1;
    uint8_t stripe:1;
    uint8_t procWidget:1;//Processing Widget
    uint8_t unsolCapable:1;
    uint8_t connList:1;
    uint8_t digital:1;
    uint8_t pwrControl:1;
    uint8_t LRSwap:1;
    uint8_t CPCaps:1;
    uint8_t channelCount:3;
    uint8_t delay:4;
    uint8_t type:4;
    //... Reserved
};

struct connList{
    uint8_t connections[50];
    size_t size = 0;
};

struct PinCapabilitiesStruct{
    uint8_t impedance:1;
    uint8_t  triggerReq:1;
    uint8_t  presenceDetection:1;
    uint8_t  headphoneDrive:1;
    uint8_t  output:1;
    uint8_t  input:1;
    uint8_t  balanced:1;
    uint8_t  hdmi:1;
    uint8_t  vrefCTRL:1;
    uint8_t  EAPD:1;
    uint8_t  rsv;
    uint8_t  DP:1;
    uint8_t  rsv2:2;
    uint8_t  HBR:1;
    //..Reserved
};

struct AmpCapabilities{
    uint8_t offset:7;
    uint8_t rsv:1;
    uint8_t numOfSteps:7;
    uint8_t rsv2:1;
    uint8_t stepSize:7;
    uint8_t rsv3;
    uint8_t mute:1;
};
struct HDA{
    public:
    uint64_t bar0 = 0;
    uint8_t InStreams = 0;
    uint8_t OutStreams = 0;
    uint8_t BiStreams = 0;
    uint8_t pin_alternative_output_node_number = 0;
    uint8_t pin_speaker_default_node_number = 0;
    uint8_t pin_speaker_node_number = 0;
    uint8_t pin_headphone_node_number = 0;
    uint8_t hda_pin_output_node_number = 0;
    uint8_t hda_pin_headphone_node_number = 0;
    uint8_t hda_selected_output_node = 0;
    uint8_t hda_audio_output_node_number = 0;
    uint32_t hda_audio_output_node_sample_capabilities = 0;
    uint32_t hda_audio_output_node_stream_format_capabilities = 0;
    uint8_t hda_output_amp_node_number = 0;
    uint32_t hda_output_amp_node_capabilities = 0;
    uint8_t hda_second_audio_output_node_number = 0;
    uint32_t hda_second_audio_output_node_sample_capabilities = 0;
    uint32_t hda_second_audio_output_node_stream_format_capabilities = 0;
    uint8_t hda_second_output_amp_node_number = 0;
    uint8_t hda_second_output_amp_node_capabilities = 0;
    HDA(PCI::PCIDeviceHeader* pciHdr);
    void Reset();
    void ResetCORB();
    void ResetRIRB();
    void SendCommand(uint8_t codec_address, uint8_t node_index, uint16_t command_code, uint8_t data);
    void SendCommand16(uint8_t codec_address, uint8_t node_index, uint8_t command_code, uint16_t data);
    uint64_t ReadResponse();
    void EnumerateCodecs();
    void WaitForCommand();
    void PushBDLE(uint64_t address, uint32_t length, uint8_t IOC);
    uint16_t GetFormat(uint32_t frequency, uint32_t bps, uint32_t channels);
    void Play(uint8_t *data, size_t sz, uint32_t frequency, uint32_t bps, uint32_t channels);
    int RefillBuffer();
    uint16_t GetCORBSize();
    connList* GetConnectedNodes(int, int);
    void EnumerateWidgets(int, int);
    void EnumerateFunctionGroups(int);
    void SetVolume(uint8_t, uint8_t, uint8_t);
    void SetFormat(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    void SetConverterCTRL(uint8_t, uint8_t, uint8_t, uint8_t);
    void SetPin(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
    void FillHDAAudioBuffer(uint16_t*, size_t, float, uint32_t);
    uint64_t SRCMD(uint8_t, uint8_t, uint16_t, uint8_t);
    uint64_t SRCMD16(uint8_t, uint8_t, uint8_t, uint16_t);
    uint8_t FindPin();
    uint8_t GetPinSense(uint8_t codec, uint8_t node);
    uint8_t findDacConnToPin(uint8_t codec, uint8_t node);
    void InitializeOutPin(uint8_t codec, uint8_t node);
    void InitializeMixer(uint8_t codec, uint8_t node);
    uint32_t GetNodeType(int codec, int node);
    void InitializeDAC(uint8_t codec, uint8_t node);
    void InitializeAudioSelector(uint8_t, uint8_t);
    void hda_set_node_gain(uint8_t codec, uint8_t node, uint8_t node_type, uint32_t capabilities, uint8_t gain);
};


extern void* TestSong;
extern uint8_t numOfHDAInstances;
extern HDA* hdaInstances[10];