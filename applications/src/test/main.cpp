#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <sound/asound.h>

#define CHECK(x) do { if ((x) < 0) { perror(#x); return 1; } } while (0)

int main(int argc, char** argv) {
    const char* device = argc > 1 ? argv[1] : "/dev/snd/pcmC0D0p";
    int fd = open(device, O_RDWR);
    CHECK(fd);

    struct snd_pcm_hw_params* params = (struct snd_pcm_hw_params*)calloc(1, sizeof(*params));
    if (!params) return 1;

    // Mark everything as requested
    params->rmask = ~0U;

    int err = ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, params);
    CHECK(err);

    printf("Dumping HW parameters for %s:\n", device);

    // Print sample rate interval
    struct snd_interval* rate = &params->intervals[SNDRV_PCM_HW_PARAM_RATE - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    printf("Rate: %u - %u (integer: %u)\n", rate->min, rate->max, rate->integer);

    // Channels
    struct snd_interval* chans = &params->intervals[SNDRV_PCM_HW_PARAM_CHANNELS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    printf("Channels: %u - %u\n", chans->min, chans->max);

    // Sample bits
    struct snd_interval* bits = &params->intervals[SNDRV_PCM_HW_PARAM_SAMPLE_BITS - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    printf("Sample Bits: %u - %u\n", bits->min, bits->max);

    // Buffer and period sizes
    struct snd_interval* period = &params->intervals[SNDRV_PCM_HW_PARAM_PERIOD_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    struct snd_interval* buffer = &params->intervals[SNDRV_PCM_HW_PARAM_BUFFER_BYTES - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
    printf("Period Bytes: %u - %u\n", period->min, period->max);
    printf("Buffer Bytes: %u - %u\n", buffer->min, buffer->max);

    // Format
    struct snd_mask* fmt = &params->masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK];
    printf("Formats: ");
    for (int i = 0; i < 64; i++) {
        if ((fmt->bits[i / 32] >> (i % 32)) & 1)
            printf("%d ", i);
    }
    printf("\n");

	struct snd_mask* subfmt = &params->masks[SNDRV_PCM_HW_PARAM_SUBFORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK];
	printf("Subformats: ");
	for (int i = 0; i < 64; i++) {
		if ((subfmt->bits[i / 32] >> (i % 32)) & 1)
			printf("%d ", i);
	}
	printf("\n");

    free(params);
    close(fd);
    return 0;
}
