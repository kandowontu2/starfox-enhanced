#pragma once
#include <3ds/ndsp/ndsp.h>
enum {NDSP_FORMAT_STEREO_PCM16=2|(1<<2)};
typedef enum {NDSP_INTERP_POLYPHASE=0,NDSP_INTERP_LINEAR=1,NDSP_INTERP_NONE=2} ndspInterpType;
void ndspChnReset(int id);
void ndspChnSetFormat(int id,u16 format);
void ndspChnSetRate(int id,float rate);
void ndspChnSetInterp(int id,ndspInterpType type);
void ndspChnSetMix(int id,float mix[12]);
void ndspChnSetPaused(int id,bool paused);
void ndspChnWaveBufClear(int id);
void ndspChnWaveBufAdd(int id,ndspWaveBuf* buffer);
