/*ringbuf .c*/

#include "loto_BufferManager.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <yangutil/buffer/YangAudioEncoderBuffer.h>
#include <yangutil/buffer/YangVideoEncoderBuffer.h>
#include <yangutil/sys/YangEndian.h>

#include "common.h"
#include "sample_comm.h"

extern YangAudioEncoderBuffer *g_audioBuffer;
extern YangVideoEncoderBuffer *g_videoBuffer;

extern long long basesatmp;

static uint32_t find_start_code(uint8_t *buf, uint32_t zeros_in_startcode)
{
    uint32_t info;
    uint32_t i;

    info = 1;
    if ((info = (buf[zeros_in_startcode] != 1) ? 0 : 1) == 0)
        return 0;

    for (i = 0; i < zeros_in_startcode; i++)
        if (buf[i] != 0) {
            info = 0;
            break;
        };

    return info;
}

static uint8_t *get_nal(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total)
{
    uint32_t info;
    uint8_t *q;
    uint8_t *p = *offset;
    *len       = 0;

    if ((p - start) >= total)
        return NULL;

    while (1) {
        info = find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total)
            return NULL;
    }

    q = p + 4;
    p = q;
    while (1) {
        info = find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total)
            break;
    }

    *len    = (p - q);
    *offset = p;

    return q;
}

HI_S32 HisiPutH264DataToBuffer(VENC_STREAM_S *pstStream)
{
    HI_S32         i;
    HI_S32         len = 0, off = 0;
    unsigned char *pstr;
    int            iframe = 0;
    YangFrame      videoFrame;

    uint32_t nal_len;
    uint32_t nal_len_n;
    uint8_t *nal;
    uint8_t *nal_n;
    char    *output;
    uint32_t offset = 0;
    uint8_t *buf_offset;

    long long curstamp = get_timestamp(NULL, 1);
    if (basesatmp == 0)
        basesatmp = curstamp;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        len += pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
    }

    videoFrame.pts     = curstamp - basesatmp;
    videoFrame.payload = (uint8_t *)malloc(len);

    for (i = 0; i < pstStream->u32PackCount; i++) {
        pstr       = pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset;
        buf_offset = pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset;

        nal = get_nal(&nal_len, &buf_offset, pstr, len);
        if (nal != NULL && nal[0] == 0x6) {
            len -= pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
            continue;
        }

        memcpy(videoFrame.payload + off, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);
        off += pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;

        // if (pstStream->u32PackCount > 1)
        // {
        //     char payload[256] = "";
        //     int count = pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset;
        //     if (count > 40)
        //         count = 40;
        //     for (int k = 0; k < count; k ++)
        //     {
        //         sprintf(payload, "%s 0x%x", payload, (pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset)[k]);
        //     }
        //     SAMPLE_PRT("\n Key Frame i = %d payload = %s\n", i, payload);
        // }
    }

    pstr       = videoFrame.payload;
    buf_offset = videoFrame.payload;

    nal = get_nal(&nal_len, &buf_offset, pstr, len);
    if (nal != NULL && nal[0] == 0x67) {
        iframe = 1;
        nal_n  = get_nal(&nal_len_n, &buf_offset, pstr, len); // get pps
        if (nal_n == NULL) {
            return HI_SUCCESS;
        }

        yang_put_be32((char *)(videoFrame.payload), nal_len);
        yang_put_be32((char *)(videoFrame.payload + 4 + nal_len), nal_len_n);
    } else if (nal != NULL) {
        memcpy(videoFrame.payload, videoFrame.payload + 4, len - 4);
        len -= 4;
    } else {
        return HI_SUCCESS;
    }

    videoFrame.nb = len;

    if (iframe) {
        videoFrame.frametype = YANG_Frametype_I;
    } else {
        videoFrame.frametype = YANG_Frametype_P;
    }

    g_videoBuffer->putEVideo(&videoFrame);

    free(videoFrame.payload);

    return HI_SUCCESS;
}

HI_S32 HisiPutAACDataToBuffer(AUDIO_STREAM_S *aacStream)
{
    YangFrame audioFrame;

    long long curstamp = get_timestamp(NULL, 1);
    if (basesatmp == 0)
        basesatmp = curstamp;

    audioFrame.pts     = curstamp - basesatmp;
    audioFrame.nb      = aacStream->u32Len;
    audioFrame.payload = (uint8_t *)malloc(aacStream->u32Len);
    memcpy(audioFrame.payload, aacStream->pStream, aacStream->u32Len);

    g_audioBuffer->putAudio(&audioFrame);

    return HI_SUCCESS;
}

HI_S32 HisiPutOpusDataToBuffer(HI_U8 *opus_data, HI_U32 data_size, HI_U64 timestamp)
{
    YangFrame opusFrame;

    long long curstamp = get_timestamp(NULL, 1);
    if (basesatmp == 0)
        basesatmp = curstamp;

    opusFrame.pts     = curstamp - basesatmp;
    opusFrame.nb      = data_size;
    opusFrame.payload = (uint8_t *)malloc(data_size);
    memcpy(opusFrame.payload, opus_data, data_size);

    g_audioBuffer->putAudio(&opusFrame);

    return HI_SUCCESS;
}
