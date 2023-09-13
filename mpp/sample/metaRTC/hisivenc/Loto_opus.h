
#ifndef LOTO_OPUS_H
#define LOTO_OPUS_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include "hi_common.h"
#include "opus.h"
#include "opus_types.h"
#include "opus_defines.h"
#include "opus_projection.h"
#include "opus_multistream.h"

typedef enum OPUS_SAMPLING_RATE {
    OPUS_SAMPLING_RATE_8000 = 8000,
    OPUS_SAMPLING_RATE_12000 = 12000,
    OPUS_SAMPLING_RATE_16000 = 16000,
    OPUS_SAMPLING_RATE_24000 = 24000,
    OPUS_SAMPLING_RATE_48000 = 48000,
    OPUS_SAMPLING_RATE_BUTT
} OPUS_SAMPLING_RATE;

typedef struct OPUS_ENCODER_ATTR_S {
    opus_int32 samplingRate;
    opus_int32 channels;
    opus_int32 application;
    opus_int32 bitrate;
    opus_int32 framesize;
    opus_int32 vbr;          /* 0: CBR, 1: VBR(Default) */
    opus_int32 forceChannel; /* OPUS_AUTO: not forced; 1: forced mono; 2: forced: stereo */
} OPUS_ENCODER_ATTR_S;

typedef struct OPUS_ENCODER_S {
    OpusEncoder *opusEncoder;
    OPUS_ENCODER_ATTR_S stOpusAttr;
} OPUS_ENCODER_S;

typedef struct OPUS_THREAD_ARG {
    OpusEncoder *opusEncoder;
    AUDIO_DEV AiDevId;
    AI_CHN AiChn;
    HI_BOOL start;
} OPUS_THREAD_ARG;

void *LOTO_OPUS_AudioEncode(void *p);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif // LOTO_OPUS_H