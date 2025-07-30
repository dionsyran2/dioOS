#pragma once
#include <stdint.h>
#include <BasicRenderer.h>
#include <pci.h>
#include <scheduling/task/scheduler.h>
#include <VFS/vfs.h>
// To-do: cleanup... this is a mess

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

#define HDA_SUPPORT_RATE_8kHz (1 << 0)
#define HDA_SUPPORT_RATE_11kHz (1 << 1) // 11,025
#define HDA_SUPPORT_RATE_16kHz (1 << 2)
#define HDA_SUPPORT_RATE_22kHz (1 << 3) // 22,05
#define HDA_SUPPORT_RATE_32kHz (1 << 4)
#define HDA_SUPPORT_RATE_44kHz (1 << 5) // 44,1
#define HDA_SUPPORT_RATE_48kHz (1 << 6)
#define HDA_SUPPORT_RATE_88kHz (1 << 7) // 88,2
#define HDA_SUPPORT_RATE_96kHz (1 << 8)
#define HDA_SUPPORT_RATE_176kHz (1 << 9) // 176,4
#define HDA_SUPPORT_RATE_192kHz (1 << 10)
#define HDA_SUPPORT_RATE_384kHz (1 << 11)


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


struct snd_hwdep_info {
	unsigned int device;		/* WR: device number */
	int card;			/* R: card number */
	unsigned char id[64];		/* ID (user selectable) */
	unsigned char name[80];		/* hwdep name */
	int iface;			/* hwdep interface */
	unsigned char reserved[64];	/* reserved for future */
};

/* generic DSP loader */
struct snd_hwdep_dsp_status {
	unsigned int version;		/* R: driver-specific version */
	unsigned char id[32];		/* R: driver-specific ID string */
	unsigned int num_dsps;		/* R: number of DSP images to transfer */
	unsigned int dsp_loaded;	/* R: bit flags indicating the loaded DSPs */
	unsigned int chip_ready;	/* R: 1 = initialization finished */
	unsigned char reserved[16];	/* reserved for future use */
};

struct snd_hwdep_dsp_image {
	unsigned int index;		/* W: DSP index */
	unsigned char name[64];		/* W: ID (e.g. file name) */
	unsigned char *image;	/* W: binary image */
	size_t length;			/* W: size of image in bytes */
	unsigned long driver_data;	/* W: driver-specific data */
};

struct snd_ctl_card_info {
	int card;			/* card number */
	int pad;			/* reserved for future (was type) */
	unsigned char id[16];		/* ID of card (user selectable) */
	unsigned char driver[16];	/* Driver name */
	unsigned char name[32];		/* Short name of soundcard */
	unsigned char longname[80];	/* name + info text about soundcard */
	unsigned char reserved_[16];	/* reserved for future (was ID of mixer) */
	unsigned char mixername[80];	/* visual mixer identification */
	unsigned char components[128];	/* card components / fine identification, delimited with one space (AC97 etc..) */
};
typedef int snd_pcm_hw_param_t;
#define	SNDRV_PCM_HW_PARAM_ACCESS	0	/* Access type */
#define	SNDRV_PCM_HW_PARAM_FORMAT	1	/* Format */
#define	SNDRV_PCM_HW_PARAM_SUBFORMAT	2	/* Subformat */
#define	SNDRV_PCM_HW_PARAM_FIRST_MASK	SNDRV_PCM_HW_PARAM_ACCESS
#define	SNDRV_PCM_HW_PARAM_LAST_MASK	SNDRV_PCM_HW_PARAM_SUBFORMAT

#define	SNDRV_PCM_HW_PARAM_SAMPLE_BITS	8	/* Bits per sample */
#define	SNDRV_PCM_HW_PARAM_FRAME_BITS	9	/* Bits per frame */
#define	SNDRV_PCM_HW_PARAM_CHANNELS	10	/* Channels */
#define	SNDRV_PCM_HW_PARAM_RATE		11	/* Approx rate */
#define	SNDRV_PCM_HW_PARAM_PERIOD_TIME	12
                        /* Approx distance between
						 * interrupts in us
						 */
#define	SNDRV_PCM_HW_PARAM_PERIOD_SIZE	13
                        /* Approx frames between
						 * interrupts
						 */
#define	SNDRV_PCM_HW_PARAM_PERIOD_BYTES	14 
                        /* Approx bytes between
						 * interrupts
						 */
#define	SNDRV_PCM_HW_PARAM_PERIODS	15
                        /* Approx interrupts per
						 * buffer
						 */
#define	SNDRV_PCM_HW_PARAM_BUFFER_TIME	16
                        /* Approx duration of buffer
						 * in us
						 */
#define	SNDRV_PCM_HW_PARAM_BUFFER_SIZE	17	/* Size of buffer in frames */
#define	SNDRV_PCM_HW_PARAM_BUFFER_BYTES	18	/* Size of buffer in bytes */
#define	SNDRV_PCM_HW_PARAM_TICK_TIME	19	/* Approx tick duration in us */
#define	SNDRV_PCM_HW_PARAM_FIRST_INTERVAL	SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define	SNDRV_PCM_HW_PARAM_LAST_INTERVAL	SNDRV_PCM_HW_PARAM_TICK_TIME

#define SNDRV_PCM_HW_PARAMS_NORESAMPLE	(1<<0)	/* avoid rate resampling */
#define SNDRV_PCM_HW_PARAMS_EXPORT_BUFFER	(1<<1)	/* export buffer */
#define SNDRV_PCM_HW_PARAMS_NO_PERIOD_WAKEUP	(1<<2)/* disable period wakeups */
#define SNDRV_PCM_HW_PARAMS_NO_DRAIN_SILENCE	(1<<3)
                            /* suppress drain with the filling
							 * of the silence samples
							 */
#define	SNDRV_PCM_HW_PARAM_FIRST_INTERVAL	SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define	SNDRV_PCM_HW_PARAM_LAST_INTERVAL	SNDRV_PCM_HW_PARAM_TICK_TIME
#define PARAM_MASK_BIT(b)	(1U << (int)(b))

#define MASK_OFS(i)	((i) >> 5)
#define MASK_BIT(i)	(1U << ((i) & 31))

typedef int snd_pcm_subformat_t;
#define	SNDRV_PCM_SUBFORMAT_STD		((snd_pcm_subformat_t) 0)
typedef unsigned long snd_pcm_uframes_t;

struct snd_interval {
	unsigned int min, max;
	unsigned int openmin:1,
		     openmax:1,
		     integer:1,
		     empty:1,
             reserved:28;
};
#define SNDRV_MASK_MAX	256
struct snd_mask {
	uint32_t bits[(SNDRV_MASK_MAX+31)/32];
};
struct snd_pcm_hw_params {
	unsigned int flags;
	struct snd_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK -
			       SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
	struct snd_mask mres[5];	/* reserved masks */
	struct snd_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
				        SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
	struct snd_interval ires[9];	/* reserved intervals */
	unsigned int rmask;		/* W: requested masks */
	unsigned int cmask;		/* R: changed masks */
	unsigned int info;		/* R: Info flags for returned setup */
	unsigned int msbits;		/* R: used most significant bits (in sample bit-width) */
	unsigned int rate_num;		/* R: rate numerator */
	unsigned int rate_den;		/* R: rate denominator */
	snd_pcm_uframes_t fifo_size;	/* R: chip FIFO size in frames */
	unsigned char sync[16];		/* R: synchronization ID (perfect sync - one clock source) */
	unsigned char reserved[48];	/* reserved for future */
};

struct snd_pcm_sw_params {
	int tstamp_mode;			/* timestamp mode */
	unsigned int period_step;
	unsigned int sleep_min;			/* min ticks to sleep */
	snd_pcm_uframes_t avail_min;		/* min avail frames for wakeup */
	snd_pcm_uframes_t xfer_align;		/* obsolete: xfer size need to be a multiple */
	snd_pcm_uframes_t start_threshold;	/* min hw_avail frames for automatic start */
	snd_pcm_uframes_t stop_threshold;	/* min avail frames for automatic stop */
	snd_pcm_uframes_t silence_threshold;	/* min distance from noise for silence filling */
	snd_pcm_uframes_t silence_size;		/* silence block size */
	snd_pcm_uframes_t boundary;		/* pointers wrap point */
	unsigned int proto;			/* protocol version */
	unsigned int tstamp_type;		/* timestamp type (req. proto >= 2.0.12) */
	unsigned char reserved[56];		/* reserved for future */
};

typedef signed long snd_pcm_sframes_t;
struct snd_xferi {
	snd_pcm_sframes_t result;
	void *buf;
	snd_pcm_uframes_t frames;
};
typedef int snd_ctl_elem_iface_t;
#define SNDRV_CTL_ELEM_ID_NAME_MAXLEN	44

struct snd_ctl_elem_id {
	unsigned int numid;		/* numeric identifier, zero = invalid */
	snd_ctl_elem_iface_t iface;	/* interface identifier */
	unsigned int device;		/* device/client number */
	unsigned int subdevice;		/* subdevice (substream) number */
	unsigned char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];		/* ASCII name of item */
	unsigned int index;		/* index of item */
};

struct snd_ctl_elem_info {
	struct snd_ctl_elem_id id;	/* W: element ID */
	int type;	/* R: value type - SNDRV_CTL_ELEM_TYPE_* */
	unsigned int access;		/* R: value access (bitmask) - SNDRV_CTL_ELEM_ACCESS_* */
	unsigned int count;		/* count of values */
	int owner;		/* owner's PID of this control */
	union {
		struct {
			long min;		/* R: minimum value */
			long max;		/* R: maximum value */
			long step;		/* R: step (0 variable) */
		} integer;
		struct {
			long long min;		/* R: minimum value */
			long long max;		/* R: maximum value */
			long long step;		/* R: step (0 variable) */
		} integer64;
		struct {
			unsigned int items;	/* R: number of items */
			unsigned int item;	/* W: item number */
			char name[64];		/* R: value name */
			uint64_t names_ptr;	/* W: names list (ELEM_ADD only) */
			unsigned int names_length;
		} enumerated;
		unsigned char reserved[128];
	} value;
	unsigned char reserved[64];
};

struct snd_ctl_elem_list {
	unsigned int offset;
	unsigned int space;
	unsigned int used;		/* R: count of element IDs set */
	unsigned int count;		/* R: count of all elements */
	struct snd_ctl_elem_id *pids; /* R: IDs */
	unsigned char reserved[50];
};
/********************************************************************************/
#define	SNDRV_PCM_ACCESS_RW_INTERLEAVED		3

#define SNDRV_PCM_INFO_MMAP		0x00000001	/* hardware supports mmap */
#define SNDRV_PCM_INFO_MMAP_VALID	0x00000002	/* period data are valid during transfer */
#define SNDRV_PCM_INFO_DOUBLE		0x00000004	/* Double buffering needed for PCM start/stop */
#define SNDRV_PCM_INFO_BATCH		0x00000010	/* double buffering */
#define SNDRV_PCM_INFO_SYNC_APPLPTR	0x00000020	/* need the explicit sync of appl_ptr update */
#define SNDRV_PCM_INFO_PERFECT_DRAIN	0x00000040	/* silencing at the end of stream is not required */
#define SNDRV_PCM_INFO_INTERLEAVED	0x00000100	/* channels are interleaved */
#define SNDRV_PCM_INFO_NONINTERLEAVED	0x00000200	/* channels are not interleaved */
#define SNDRV_PCM_INFO_COMPLEX		0x00000400	/* complex frame organization (mmap only) */
#define SNDRV_PCM_INFO_BLOCK_TRANSFER	0x00010000	/* hardware transfer block of samples */
#define SNDRV_PCM_INFO_OVERRANGE	0x00020000	/* hardware supports ADC (capture) overrange detection */
#define SNDRV_PCM_INFO_RESUME		0x00040000	/* hardware supports stream resume after suspend */
#define SNDRV_PCM_INFO_PAUSE		0x00080000	/* pause ioctl is supported */
#define SNDRV_PCM_INFO_HALF_DUPLEX	0x00100000	/* only half duplex */
#define SNDRV_PCM_INFO_JOINT_DUPLEX	0x00200000	/* playback and capture stream are somewhat correlated */
#define SNDRV_PCM_INFO_SYNC_START	0x00400000	/* pcm support some kind of sync go */
#define SNDRV_PCM_INFO_NO_PERIOD_WAKEUP	0x00800000	/* period wakeup can be disabled */
#define SNDRV_PCM_INFO_HAS_WALL_CLOCK   0x01000000      /* (Deprecated)has audio wall clock for audio/system time sync */
#define SNDRV_PCM_INFO_HAS_LINK_ATIME              0x01000000  /* report hardware link audio time, reset on startup */
#define SNDRV_PCM_INFO_HAS_LINK_ABSOLUTE_ATIME     0x02000000  /* report absolute hardware link audio time, not reset on startup */
#define SNDRV_PCM_INFO_HAS_LINK_ESTIMATED_ATIME    0x04000000  /* report estimated link audio time */
#define SNDRV_PCM_INFO_HAS_LINK_SYNCHRONIZED_ATIME 0x08000000  /* report synchronized audio/system time */
#define SNDRV_PCM_INFO_EXPLICIT_SYNC	0x10000000	/* needs explicit sync of pointers and data */
#define SNDRV_PCM_INFO_NO_REWINDS	0x20000000	/* hardware can only support monotonic changes of appl_ptr */
#define SNDRV_PCM_INFO_DRAIN_TRIGGER	0x40000000		/* internal kernel flag - trigger in drain */
#define SNDRV_PCM_INFO_FIFO_IN_FRAMES	0x80000000	/* internal kernel flag - FIFO size is in frames */


#define SNDRV_PROTOCOL_VERSION(major, minor, subminor) (((major)<<16)|((minor)<<8)|(subminor))
#define SNDRV_HWDEP_VERSION		SNDRV_PROTOCOL_VERSION(1, 0, 1)
#define SNDRV_CTL_VERSION		SNDRV_PROTOCOL_VERSION(2, 0, 9)
#define SNDRV_PCM_VERSION		SNDRV_PROTOCOL_VERSION(2, 0, 18)


#define SNDRV_HWDEP_IOCTL_PVERSION	_IOR ('H', 0x00, int)
#define SNDRV_HWDEP_IOCTL_INFO		_IOR ('H', 0x01, struct snd_hwdep_info)
#define SNDRV_HWDEP_IOCTL_DSP_STATUS	_IOR('H', 0x02, struct snd_hwdep_dsp_status)
#define SNDRV_HWDEP_IOCTL_DSP_LOAD	_IOW('H', 0x03, struct snd_hwdep_dsp_image)


#define SNDRV_CTL_IOCTL_PVERSION	_IOR('U', 0x00, int)
#define SNDRV_CTL_IOCTL_CARD_INFO	_IOR('U', 0x01, struct snd_ctl_card_info)
#define SNDRV_CTL_IOCTL_ELEM_LIST	_IOWR('U', 0x10, struct snd_ctl_elem_list)
#define SNDRV_CTL_IOCTL_SUBSCRIBE_EVENTS _IOWR('U', 0x16, int)
#define SNDRV_CTL_IOCTL_PCM_PREFER_SUBDEVICE _IOW('U', 0x32, int)



#define SNDRV_PCM_IOCTL_PVERSION	_IOR('A', 0x00, int)
#define SNDRV_PCM_IOCTL_INFO		_IOR('A', 0x01, struct snd_pcm_info)
#define SNDRV_PCM_IOCTL_USER_PVERSION	_IOW('A', 0x04, int)
#define SNDRV_PCM_IOCTL_TTSTAMP		_IOW('A', 0x03, int)
#define SNDRV_PCM_IOCTL_HW_REFINE	_IOWR('A', 0x10, struct snd_pcm_hw_params)
#define SNDRV_PCM_IOCTL_HW_PARAMS	_IOWR('A', 0x11, struct snd_pcm_hw_params)
#define SNDRV_PCM_IOCTL_SW_PARAMS	_IOWR('A', 0x13, struct snd_pcm_sw_params)
#define SNDRV_PCM_IOCTL_PREPARE		_IO('A', 0x40)
#define SNDRV_PCM_IOCTL_HW_FREE		_IO('A', 0x12)
#define SNDRV_PCM_IOCTL_START		_IO('A', 0x42)
#define SNDRV_PCM_IOCTL_DROP		_IO('A', 0x43)
#define SNDRV_PCM_IOCTL_WRITEI_FRAMES _IOW('A', 0x50, struct snd_xferi)

#define	SNDRV_PCM_FORMAT_S8	(0)
#define	SNDRV_PCM_FORMAT_U8	(1)
#define	SNDRV_PCM_FORMAT_S16_LE	(2)
#define	SNDRV_PCM_FORMAT_S16_BE	(3)
#define	SNDRV_PCM_FORMAT_U16_LE	(4)
#define	SNDRV_PCM_FORMAT_U16_BE	(5)
#define	SNDRV_PCM_FORMAT_S24_LE	(6) /* low three bytes */
#define	SNDRV_PCM_FORMAT_S24_BE	(7) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_LE	(8) /* low three bytes */
#define	SNDRV_PCM_FORMAT_U24_BE	(9) /* low three bytes */
/*
 * For S32/U32 formats, 'msbits' hardware parameter is often used to deliver information about the
 * available bit count in most significant bit. It's for the case of so-called 'left-justified' or
 * `right-padding` sample which has less width than 32 bit.
 */
#define	SNDRV_PCM_FORMAT_S32_LE	(10)
#define	SNDRV_PCM_FORMAT_S32_BE	(11)
#define	SNDRV_PCM_FORMAT_U32_LE	(12)
#define	SNDRV_PCM_FORMAT_U32_BE	(13)

enum {
	SNDRV_PCM_STREAM_PLAYBACK = 0,
	SNDRV_PCM_STREAM_CAPTURE,
	SNDRV_PCM_STREAM_LAST = SNDRV_PCM_STREAM_CAPTURE,
};


struct snd_pcm_info {
	unsigned int device;		/* RO/WR (control): device number */
	unsigned int subdevice;		/* RO/WR (control): subdevice number */
	int stream;			/* RO/WR (control): stream direction */
	int card;			/* R: card number */
	unsigned char id[64];		/* ID (user selectable) */
	unsigned char name[80];		/* name of this device */
	unsigned char subname[32];	/* subdevice name */
	int dev_class;			/* SNDRV_PCM_CLASS_* */
	int dev_subclass;		/* SNDRV_PCM_SUBCLASS_* */
	unsigned int subdevices_count;
	unsigned int subdevices_avail;
	unsigned char pad1[16];		/* was: hardware synchronization ID */
	unsigned char reserved[64];	/* reserved for future... */
};


struct HDA{
    public:
    int card_num;
    int id;
    uint64_t *CORB;
    uint64_t *RIRB;
    uint8_t NumOfCodecs = 0;
    uint8_t DAC[20];
    uint8_t numOfDACs = 0;
    uint8_t PINS[20];
    uint8_t numOfpins = 0;
    PCI::PCIHeader0 *hdr;
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
    uint32_t hda_output_amp_node_rates = 0;
    uint8_t hda_second_audio_output_node_number = 0;
    uint32_t hda_second_audio_output_node_sample_capabilities = 0;
    uint32_t hda_second_audio_output_node_stream_format_capabilities = 0;
    uint8_t hda_second_output_amp_node_number = 0;
    uint8_t hda_second_output_amp_node_capabilities = 0;
    uint32_t selected_format = 0;
    uint32_t selected_frame_size = 0;
    BufferDescriptor* bd_list = nullptr;
    uint8_t *current_sample_buffer = nullptr;
    
    uint16_t BufferRP = 0;
    uint16_t BufferWP = 0;
    uint16_t BufferSz = 0;
    uint64_t* Buffer = nullptr;
    uint8_t next_bd_refill = 0;
    bool running = false;
    bool refill_needeed = false;
    task_t* hda_task = nullptr;
    vnode_t* control;
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
    int RegisterControl();
    int RegisterOutput();
    int LowestSupportedRate();
    int HighestSupportedRate();
    int LowestSupportedSampleSize();
    int HighestSupportedSampleSize();
    void Prepare();
    uint32_t PlaySample(uint8_t* samples, size_t sample_cnt);
    void QueueSample(void* sample);
    void hwparams(snd_pcm_hw_params* params);
};


extern void* TestSong;
extern uint8_t numOfHDAInstances;
extern HDA* hdaInstances[10];