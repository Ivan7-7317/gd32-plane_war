#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include "drivers.h"
#include "ff.h"

// ????
#define AUDIO_SAMPLE_RATE     44100
#define AUDIO_BUFFER_SIZE     4096   // ???????(??)
#define AUDIO_BUFFER_COUNT    2       // ???
#define AUDIO_FILE_PATH       "0:/music/audiot1.pcm"

// ????
typedef enum {
    AUDIO_STOPPED = 0,
    AUDIO_PLAYING =1,
    AUDIO_PAUSED,
    AUDIO_EOF
} audio_state_t;

// ??????
typedef struct {
    uint8_t* buffers[AUDIO_BUFFER_COUNT];  // ????
    uint32_t buffer_size;                   // ???????
    uint32_t current_buffer;                // ??????????
    uint32_t next_buffer;                   // ????????????
    uint32_t valid_size[AUDIO_BUFFER_COUNT];// ????????????
    uint32_t play_position;                 // ??????(????)
    volatile audio_state_t state;           // ????
    volatile uint8_t buffer_ready[AUDIO_BUFFER_COUNT]; // ????????
    FIL file;                               // ????
    uint8_t is_file_open;                   // ??????
} audio_stream_t;

// ????
void audio_stream_init(void);
void audio_stream_start(void);
void audio_stream_stop(void);
void audio_stream_pause(void);
void audio_stream_resume(void);
audio_state_t audio_stream_get_state(void);
void audio_stream_process(void);  // ??????,???????

#endif
