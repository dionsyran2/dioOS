#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ALLOW_MONO_STEREO_TRANSITION
#include <drivers/audio/decoding/mp3.h>

/*void playMP3(DirEntry* fileD){
    uint8_t* file = (uint8_t*)fileD->volume->LoadFile(fileD);

    globalRenderer->printf("Loaded file: %s | size: %d MB\n", fileD->name, (fileD->size / 1024) / 1024);
    mp3dec_t mp3d;
    mp3dec_file_info_t info;

    // Initialize the decoder
    mp3dec_init(&mp3d);

    // Decode MP3 from memory    
    int result = mp3dec_load_buf(&mp3d, file, fileD->size, &info, NULL, NULL);
    if (result) {
        // Error handling
        globalRenderer->printf("MP3 decoding failed!\n");
        return;
    }


    int mp3_pos = 0, pcm_offset = 0;

    // Print MP3 information
    globalRenderer->printf("Decoded MP3: %d Hz, %d channels, %d samples, %llx buffer\n",
           info.hz, info.channels, info.samples, info.buffer);
    size_t buffer_size = info.samples * sizeof(int16_t);
    globalRenderer->printf("buffer size: %d\n", buffer_size);
    hdaInstances[0]->Play((uint8_t*)info.buffer, buffer_size, info.hz, 16, info.channels); //16 bits for mp3
    free(info.buffer);
    //free(file);
    GlobalAllocator.FreePages(file, ((fileD->size + 0x1000 - 1) / 0x1000) + 1);
}*/

void playMP3(uint8_t* file, size_t sz){
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    // Initialize the decoder
    mp3dec_init(&mp3d);


    // Decode MP3 from memory
    
    int result = mp3dec_load_buf(&mp3d, file, sz, &info, NULL, NULL);
    if (result) {
        // Error handling
        globalRenderer->printf("MP3 decoding failed!\n");
        return;
    }


    int mp3_pos = 0, pcm_offset = 0;

    // Print MP3 information
    globalRenderer->printf("Decoded MP3: %d Hz, %d channels, %d samples, %llx buffer\n",
           info.hz, info.channels, info.samples, info.buffer);
    size_t buffer_size = info.samples * sizeof(int16_t);
    hdaInstances[0]->Play((uint8_t*)info.buffer, buffer_size, info.hz, 16, info.channels); //16 bits for mp3
    free(info.buffer);
}