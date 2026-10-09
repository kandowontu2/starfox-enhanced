#pragma once
#include <3ds/types.h>

// The adapter-facing declarations/values follow libctru's ndsp.h. This test
// double does not emulate DSP firmware, IPC, timing or hardware DMA.
typedef enum {NDSP_OUTPUT_MONO=0,NDSP_OUTPUT_STEREO=1,NDSP_OUTPUT_SURROUND=2} ndspOutputMode;
typedef struct {u16 index;s16 history0,history1;} ndspAdpcmData;
enum {NDSP_WBUF_FREE=0,NDSP_WBUF_QUEUED=1,NDSP_WBUF_PLAYING=2,NDSP_WBUF_DONE=3};
typedef struct tag_ndspWaveBuf ndspWaveBuf;
struct tag_ndspWaveBuf {
    union {s8* data_pcm8;s16* data_pcm16;u8* data_adpcm;const void* data_vaddr;};
    u32 nsamples;
    ndspAdpcmData* adpcm_data;
    u32 offset;
    bool looping;
    u8 status;
    u16 sequence_id;
    ndspWaveBuf* next;
};
Result ndspInit(void);
void ndspExit(void);
void ndspSetOutputMode(ndspOutputMode mode);
