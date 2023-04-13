#ifndef RINGFIFO_H
#define RINGFIFO_H

#include "hi_type.h"
#include "hi_comm_aio.h"
#include "hi_comm_venc.h"

struct ringbuf
{
    unsigned char *buffer;
    int frame_type;
    int size;

    unsigned long long timestamp;
    unsigned long long getframe_timestamp;
};
int addring(int i);
int ringget(struct ringbuf *getinfo);
void ringput(unsigned char *buffer, int size, int encode_type);
void ringfree();
void ringmalloc(int size);
void ringreset();

int ringget_audio(struct ringbuf *getinfo);
void ringput_audio(unsigned char *buffer, int size);
void ringfree_audio();
void ringmalloc_audio(int size);
void ringreset_audio();

HI_S32 HisiPutAACDataToBuffer(AUDIO_STREAM_S *aacStream);
HI_S32 HisiPutH264DataToBuffer(VENC_STREAM_S *pstStream);
HI_S32 HisiPutOpusDataToBuffer(HI_U8 *opus_data, HI_U32 data_size, HI_U64 timestamp);

#endif