#pragma once
#include <3ds/types.h>
#define CSND_TIMER(n) (0x3FEC3FC / ((u32)(n)))
#define SOUND_CHANNEL(n) ((u32)(n)&0x1F)
#define SOUND_LINEAR_INTERP BIT(6)
#define SOUND_ONE_SHOT (2U<<10)
#define SOUND_FORMAT_16BIT (1U<<12)
#define SOUND_ENABLE BIT(14)
static inline u32 CSND_VOL(float vol,float pan) {
    const u32 left=(u32)(vol*(1.F-pan)*.5F*0x8000);
    const u32 right=(u32)(vol*(1.F+pan)*.5F*0x8000);
    return left|(right<<16);
}
typedef struct {u8 active;} CSND_ChnInfo;
extern u32 csndChannels;
Result csndInit(void);
void csndExit(void);
u32* csndAddCmd(int id);
Result csndExecCmds(bool waitDone);
void CSND_SetPlayStateR(u32 channel,u32 value);
void CSND_SetChnRegs(u32 flags,u32 first,u32 second,u32 bytes,u32 volume,u32 capture);
Result CSND_FlushDataCache(const void* pointer,u32 bytes);
CSND_ChnInfo* csndGetChnInfo(u32 channel);
