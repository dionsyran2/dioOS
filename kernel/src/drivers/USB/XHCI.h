#pragma once
#include "../../memory.h"
#include <stdint.h>
#include "../../pci.h"
#include "../../scheduling/apic/apic.h"
#include "../../BasicRenderer.h"
#include "../../paging/PageTableManager.h"
#include "../../memory/heap.h"
#include "../../paging/PageFrameAllocator.h"
#include "../../cstr.h"
#include "../../acpi.h"
#include "../../scheduling/apic/apic.h"
#include <lai/helpers/sci.h>
#include <lai/helpers/pci.h>
#include <lai/core.h>

namespace PCI{
    struct PCIHeader0;
}

//Capabilities
#define XHCSPARAMS1 0x04
#define DBOFF       0x14
#define RTSOFF      0x18 // Runtime Register Space Offset

//Operational
#define XUSBCMD 0x00
#define XUSBSTS 0x04
#define DNCTRL  0x14
#define XCRCR   0x18
#define BCBAREG 0x30
#define XUSBCFG 0x38


//Values
#define USBCMD_RS       (1<<0)
#define USBCMD_HCRST    (1<<1)
#define USBCMD_INTE     (1<<2)
#define USBCMD_HSEE     (1<<3)
#define USBCMD_LHCRST   (1<<7)
#define USBCMD_CSS      (1<<8)
#define USBCMD_CRS      (1<<9)
#define USBCMD_EWE      (1<<10)
#define USBCMD_EU3S     (1<<11)

#define USBSTS_HCH      (1<<0)
#define USBSTS_HSE      (1<<2)
#define USBSTS_EINT     (1<<3)
#define USBSTS_PCD      (1<<4)
#define USBSTS_SSS      (1<<8)
#define USBSTS_RSS      (1<<9)
#define USBSTS_SRE      (1<<10)
#define USBSTS_CNR      (1<<11)
#define USBSTS_HCE      (1<<12)

namespace XHCI{

    struct SLOT_CONTEXT{
        /***************/
        /* OFFSET 0x00 */
        /***************/


        uint32_t RouteString:20;
        uint8_t rsv:4; /* SPEED - This field is deprecated in this version of the specification and shall be Reserved */
        uint8_t rsv2:1;
        /* 
        This flag is set to '1' by software if this is a High-speed hub that supports
        Multiple TTs and the Multiple TT Interface has been enabled by software, or if this is a Low-
        /Full-speed device or Full-speed hub and connected to the xHC through a parent108 High-speed
        hub that supports Multiple TTs and the Multiple TT Interface of the parent hub has been
        enabled by software, or ‘0’ if not.
        */
        uint8_t MTT:1;
        uint8_t Hub:1; //This flag is set to '1' by software if this device is a USB hub, or '0' if it is a USB function.
        /*
        This field identifies the index of the last valid Endpoint Context within this
        Device Context structure. The value of ‘0’ is Reserved and is not a valid entry for this field. Valid
        entries for this field shall be in the range of 1-31. This field indicates the size of the Device
        Context structure. For example, ((Context Entries+1) * 32 bytes) = Total bytes for this structure.
        Note, Output Context Entries values are written by the xHC, and Input Context Entries values are
        written by software.
        */
        uint8_t ContextEntries:5;

        /***************/
        /* OFFSET 0x04 */
        /***************/

        /*
        The Maximum Exit Latency is in microseconds, and indicates the worst case
        time it takes to wake up all the links in the path to the device, given the current USB link level
        power management settings.
        */
        uint16_t MaxExitLatency;

        /*
        This field identifies the Root Hub Port Number used to access the USB
        device. Refer to section 4.19.7 for port numbering information.
        Note: Ports are numbered from 1 to MaxPorts.
        */
        uint8_t RootHubPortNum;

        /*
        If this device is a hub (Hub = ‘1’), then this field is set by software to identify
        the number of downstream facing ports supported by the hub. Refer to the bNbrPorts field
        description in the Hub Descriptor (Table 11-13) of the USB2 spec. If this device is not a hub (Hub
        = ‘0’), then this field shall be ‘0’
        */
        uint8_t NumOfPorts;


        /***************/
        /* OFFSET 0x08 */
        /***************/


        /*
        If this device is Low-/Full-speed and connected through a High-speed hub,
        then this field shall contain the Slot ID of the parent High-speed hub109.
        For SS and SSP bus instance, if this device is connected through a higher rank hub110 then this
        field shall contain the Slot ID of the parent hub. For example, a Gen1 x1 connected behind a
        Gen1 x2 hub, or Gen1 x2 device connected behind Gen2 x2 hub.
        This field shall be ‘0’ if any of the following are true:
        • Device is attached to a Root Hub port
        • Device is a High-Speed device
        • Device is the highest rank SS/SSP device supported by xHCI
        */
        uint8_t ParentHubSlotID;

        /* 
        If this device is Low-/Full-speed and connected through a High-speed
        hub, then this field shall contain the number of the downstream facing port of the parent High-
        speed hub109.
        For SS and SSP bus instance, if this device is connected through a higher rank hub110 then this
        field shall contain the number of the downstream facing port of the parent hub. For example, a
        Gen1 x1 connected behind a Gen1 x2 hub, or Gen1 x2 device connected behind Gen2 x2 hub.
        This field shall be ‘0’ if any of the following are true:
        • Device is attached to a Root Hub port
        • Device is a High-Speed device
        • Device is the highest rank SS/SSP device supported by xHCI
        */
        uint8_t ParentPortNum;

        /*
        If this is a High-speed hub (Hub = ‘1’ and Speed = High-Speed), then this
        field shall be set by software to identify the time the TT of the hub requires to proceed to the
        next full-/low-speed transaction.
        Value Think Time
        0 TT requires at most 8 FS bit times of inter-transaction gap on a full-/low-speed
        downstream bus.
        1 TT requires at most 16 FS bit times.
        2 TT requires at most 24 FS bit times.
        3 TT requires at most 32 FS bit times.
        Refer to the TT Think Time sub-field of the wHubCharacteristics field description in the Hub
        Descriptor (Table 11-13) and section 11.18.2 of the USB2 spec for more information on TT
        Think Time. If this device is not a High-speed hub (Hub = ‘0’ or Speed != High-speed), then this
        field shall be ‘0’.
        */
        uint8_t TTT:2;
        uint8_t rsv3:4;

        /*
        This field defines the index of the Interrupter that will receive Bandwidth
        Request Events and Device Notification Events generated by this slot, or when a Ring Underrun
        or Ring Overrun condition is reported (refer to section 4.10.3.1). Valid values are between 0 and
        MaxIntrs-1
        */
        uint16_t InterrupterTarget:10;

        /***************/
        /* OFFSET 0x04 */
        /***************/

        /*
        This field identifies the address assigned to the USB device by the xHC,
        and is set upon the successful completion of a Set Address Command. Refer to the USB2 spec
        for a more detailed description.
        As Output, this field is invalid if the Slot State = Disabled or Default.
        As Input, software shall initialize the field to ‘0’
        */
        uint8_t USBDevAddr;

        uint32_t rsv4:19;

        /*
        Slot State. This field is updated by the xHC when a Device Slot transitions from one state to
        another.
        Value Slot State
        0 Disabled/Enabled
        1 Default
        2 Addressed
        3 Configured
        31-4 Reserved
        Slot States are defined in section 4.5.3.
        As Output, since software initializes all fields of the Device Context data structure to ‘0’, this field
        shall initially indicate the Disabled state.
        As Input, software shall initialize the field to ‘0’.
        Refer to section 4.5.3 for more information on Slot State.
        */
        uint8_t SlotState:5;

        uint8_t RsvdO[sizeof(uint32_t) * 4];

    }__attribute__((packed));

    struct ENDPOINT_CONTEXT{
        /***************/
        /* OFFSET 0x00 */
        /********** ****/

        /*
        The Endpoint State identifies the current operational state of the
        endpoint.
        Value Definition
        0 Disabled The endpoint is not operational
        1 Running The endpoint is operational, either waiting for a doorbell ring or processing
        TDs.
        2 HaltedThe endpoint is halted due to a Halt condition detected on the USB. SW shall issue
        Reset Endpoint Command to recover from the Halt condition and transition to the Stopped
        state. SW may manipulate the Transfer Ring while in this state.
        3 Stopped The endpoint is not running due to a Stop Endpoint Command or recovering
        from a Halt condition. SW may manipulate the Transfer Ring while in this state.
        4 Error The endpoint is not running due to a TRB Error. SW may manipulate the Transfer
        Ring while in this state.
        5-7 Reserved
        As Output, a Running to Halted transition is forced by the xHC if a STALL condition is detected
        on the endpoint. A Running to Error transition is forced by the xHC if a TRB Error condition is
        detected.
        As Input, this field is initialized to ‘0’ by software.
        Refer to section 4.8.3 for more information on Endpoint State
        */
        uint8_t EPState:3;
        uint8_t rsv:5;


        /*
        If LEC = ‘0’, then this field indicates the maximum number of bursts within an Interval that
        this endpoint supports. Mult is a “zero-based” value, where 0 to 3 represents 1 to 4 bursts,
        respectively. The valid range of values is ‘0’ to ‘2’.111 This field shall be ‘0’ for all endpoint types
        except for SS Isochronous.
        If LEC = ‘1’, then this field shall be RsvdZ and Mult is calculated as:
        ROUNDUP(Max ESIT Payload / Max Packet Size / (Max Burst Size + 1)) - 1.
        */
        uint8_t Mult:2;


        /*
        This field identifies the maximum number of Primary
        Stream IDs this endpoint supports. Valid values are defined below. If the value of this field is ‘0’,
        then the TR Dequeue Pointer field shall point to a Transfer Ring. If this field is > '0' then the TR
        Dequeue Pointer field shall point to a Primary Stream Context Array. Refer to section 4.12 for
        more information.
        A value of ‘0’ indicates that Streams are not supported by this endpoint and the Endpoint
        Context TR Dequeue Pointer field references a Transfer Ring.
        A value of ‘1’ to ‘15’ indicates that the Primary Stream ID Width is MaxPstreams+1 and the
        Primary Stream Array contains 2MaxPStreams+1 entries.
        For SS Bulk endpoints, the range of valid values for this field is defined by the MaxPSASize field
        in the HCCPARAMS1 register (refer to Table 5-13).
        This field shall be '0' for all SS Control, Isoch, and Interrupt endpoints, and for all non-SS
        endpoints
        */
       uint8_t MaxPStreams:5;


        /*
        This field identifies how a Stream ID shall be interpreted.
        Setting this bit to a value of ‘1’ shall disable Secondary Stream Arrays and a Stream ID shall be
        interpreted as a linear index into the Primary Stream Array, where valid values for MaxPStreams
        are ‘1’ to ‘15’.
        A value of ‘0’ shall enable Secondary Stream Arrays, where the low order (MaxPStreams+1) bits
        of a Stream ID shall be interpreted as a linear index into the Primary Stream Array, where valid
        values for MaxPStreams are ‘1’ to ‘7’. And the high order bits of a Stream ID shall be interpreted
        as a linear index into the Secondary Stream Array.
        If MaxPStreams = ‘0’, this field RsvdZ.
        Refer to section 4.12.2 for more information
        */
        uint8_t LSA:1;

        uint8_t Interval; //The period between consecutive requests to a USB endpoint to send or receive data. Expressed in 125 μs. increments
        
        /*
        If LEC = '1', then this
        field indicates the high order 8 bits of the Max ESIT Payload value. If LEC = '0', then this field
        shall be RsvdZ. Refer to section 6.2.3.8 for more information
        */
        uint8_t MaxESITPayloadHi;


        /***************/
        /* OFFSET 0x04 */
        /***************/

        uint8_t rsv2:1;
        uint8_t ErrCnt:2; //This field defines a 2-bit down count, which identifies the number of consecutive USB Bus Errors allowed while executing a TD. 
        

        /*
        This field identifies whether an Endpoint Context is Valid, and if so,
        what type of endpoint the context defines.
        Value Endpoint Type Direction
        0 Not Valid N/A
        1 Isoch Out
        2 Bulk Out
        3 Interrupt Out
        4 Control Bidirectional
        5 Isoch In
        6 Bulk In
        7 Interrupt In
        */
        uint8_t EPType:3;
        uint8_t rsv3:1;

        /*
        This field affects Stream enabled endpoints, allowing the Host
        Initiated Stream selection feature to be disabled for the endpoint. Setting this bit to a value of
        ‘1’ shall disable the Host Initiated Stream selection feature. A value of ‘0’ will enable normal
        Stream operation. Refer to section 4.12.1.1 for more information.
        */
        uint8_t HID:1;

        /*
        This field indicates to the xHC the maximum number of consecutive USB
        transactions that should be executed per scheduling opportunity. This is a “zero-based” value,
        where 0 to 15 represents burst sizes of 1 to 16, respectively. Refer to section 6.2.3.4 for more
        information.
        */

        uint8_t MaxBurstSize;
        uint16_t MaxPacketSize;

        /***************/
        /* OFFSET 0x08 */
        /***************/

        /*
        This bit identifies the value of the xHC Consumer Cycle State (CCS)
        flag for the TRB referenced by the TR Dequeue Pointer. Refer to section 4.9.2 for more
        information. This field shall be ‘0’ if MaxPStreams > ‘0’.
        */
        uint8_t DCS:1;
        uint8_t rsv4:3;

        /*
        TR Dequeue Pointer. As Input, this field represents the high order bits of the 64-bit base
        address of a Transfer Ring or a Stream Context Array associated with this endpoint. If
        MaxPStreams = '0' then this field shall point to a Transfer Ring. If MaxPStreams > '0' then this
        field shall point to a Stream Context Array.
        As Output, if MaxPStreams = ‘0’ this field shall be used by the xHC to store the value of the
        Dequeue Pointer when the endpoint enters the Halted or Stopped states, and the value of the
        this field shall be undefined when the endpoint is not in the Halted or Stopped states. if
        MaxPStreams > ‘0’ then this field shall point to a Stream Context Array.
        The memory structure referenced by this physical memory pointer shall be aligned to a 16-byte
        boundary.
        */
        uint64_t TRDPointer:60;

        /***************/
        /* OFFSET 0x10 */
        /***************/

        uint16_t AvgTRBLength;
        uint16_t MaxESITPayloadLo;

        /* Reserved by HC */
        uint32_t RsvdO[3];
    }__attribute__((packed));

    struct DeviceContext{
        SLOT_CONTEXT slot;
        ENDPOINT_CONTEXT endpoints[31];
    }__attribute__((packed));

    struct TRB_Generic{
        uint64_t params;
        uint32_t sts;
        uint32_t ctrl;
    }__attribute__((packed));

    struct CR_MANAGER{
        TRB_Generic* trb;
        uint32_t size;
        uint32_t WP;
    };
    struct LinkTRB{
        uint8_t rsv:4;
        uint64_t RingSegPtr:60; //High and low combined here. normally the first 28 bits are the low, and the rest 32 are the high
        
        uint32_t rsv2:22;
        uint16_t IntTarget:10; //This field defines the index of the Interrupter that will receive TransferEvents generated by this TRB

        uint8_t Cycle:1;
        uint8_t TC:1;
        uint8_t rsv3:2;
        uint8_t chain:1;
        uint8_t IOC:1;
        uint8_t rsv4:4;
        uint8_t TRBType:6; //This field is set to Link TRB type. It should be 6
        uint16_t rsv5;
    }__attribute__((packed));

    struct EventRingSegTableEntry{
        uint32_t RingSegBaseAddressLo;
        uint32_t RingSegBaseAddressHi;
        uint16_t RingSegSize; //This field defines the number of TRBs supported by the ring segment, Valid values for this field are 16 to 4096, i.e. an Event Ring segment shall contain at least 16 entries.
        uint16_t rsvd2;
        uint32_t rsvd3;
    }__attribute__((packed));

    struct Driver{
        uint32_t* base;
        uint32_t* opbase;
        CR_MANAGER cr;
        uint8_t* MSIX_PBA;
        PCI::PCIHeader0* hdr;

        Driver(PCI::PCIDeviceHeader*);
        void Prefix();
        void IntHandler();
        void ResetHC();
        void ConfigureMaxSlots();
        void ConfigureDeviceContext();
        void ConfigureCRCR();
        void ConfigureInterrupts();
        void ConfigureEventRing();
        void Test();
    };

    void GenIntHndlr();

    extern Driver* Instances[]; //Table that consists of 20 entries... I have never seen more than 3 xhci controllers, how many do you really need
}