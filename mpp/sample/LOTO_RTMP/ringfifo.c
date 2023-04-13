/*ringbuf .c*/

#include "ringfifo.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "rtmp.h"
#include "rtputils.h"

#include "loto_comm.h"
#include "common.h"
#include "hi_comm_aio.h"

#define NMAX 32

int iput = 0;
int iget = 0; 
int n = 0;   

int a_iput = 0;
int a_iget = 0;
int a_n = 0;

struct ringbuf ringfifo[NMAX];
struct ringbuf a_ringfifo[NMAX];

extern int UpdateSpsOrPps(unsigned char *data, int frame_type, int len);

void ringmalloc(int size)
{
    int i;
    for (i = 0; i < NMAX; i++)
    {
        ringfifo[i].buffer = malloc(size);
        ringfifo[i].size = 0;
        ringfifo[i].frame_type = 0;
        // LOGD("FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
    }
    iput = 0; 
    iget = 0; 
    n = 0;    
}

void ringreset()
{
    iput = 0;
    iget = 0;
    n = 0;    
}

void ringfree(void)
{
    int i;
    LOGD("begin free mem\n");
    for (i = 0; i < NMAX; i++)
    {
        // LOGD("FREE FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
        free(ringfifo[i].buffer);
        ringfifo[i].size = 0;
    }
}

int addring(int i)
{
    return (i + 1) == NMAX ? 0 : i + 1;
}

/**
 * @brief get frame info
 *
 * @param getinfo
 * @return int
 */
int ringget(struct ringbuf *getinfo)
{
    int Pos;
    if (n > 0)
    {
        int is_delay = (n > 3);
        if (is_delay)
            LOGD("Enter ringget video n = %d\n", n);

        Pos = iget;
        iget = addring(iget);
        n--;

        while (n > 0)
        {
            if (ringfifo[Pos].frame_type == FRAME_TYPE_I)
                break;

            Pos = iget;
            iget = addring(iget);
            n--;
        }

        getinfo->buffer = (ringfifo[Pos].buffer);
        getinfo->frame_type = ringfifo[Pos].frame_type;
        getinfo->size = ringfifo[Pos].size;
        getinfo->timestamp = ringfifo[Pos].timestamp;
        getinfo->getframe_timestamp = ringfifo[Pos].getframe_timestamp;
        // LOGD("Get FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",Pos,getinfo->size,(int)(getinfo->buffer),getinfo->frame_type);

        if (is_delay)
            LOGD("Exit ringget video n = %d\n", n);

        return ringfifo[Pos].size;
    }
    else
    {
        // LOGD("Buffer is empty\n");
        return 0;
    }
}

void ringput(unsigned char *buffer, int size, int encode_type)
{

    if (n < NMAX)
    {
        memcpy(ringfifo[iput].buffer, buffer, size);
        ringfifo[iput].size = size;
        ringfifo[iput].frame_type = encode_type;
        // LOGD("Put FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",iput,ringfifo[iput].size,(int)(ringfifo[iput].buffer),ringfifo[iput].frame_type);
        iput = addring(iput);
        n++;
    }
    else
    {
        //  LOGD("Buffer is full\n");
    }
}

/**
 * @brief
 *
 * @param pstStream h264 venc stream data
 * @return HI_S32
 */
HI_S32 HisiPutH264DataToBuffer(VENC_STREAM_S *pstStream)
{
    if (pstStream == NULL)
    {
        LOGE("Stream data is empty! \n");
        return HI_FAILURE;
    }

    HI_S32 i, j;
    HI_S32 len = 0, off = 0, len2 = 2;
    unsigned char *pstr;
    int iframe = 0;

    // FILE *buffer;
    // buffer = fopen("/root/video_frame_data", "a+");

    // LOGD("HisiPutH264DataToBuffer: count = %d \n ", pstStream->u32PackCount);

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        len += pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
    }

    if (n < NMAX)
    {
        ringfifo[iput].getframe_timestamp = RTMP_GetTime();
        for (i = 0; i < pstStream->u32PackCount; i++)
        {
            memcpy(ringfifo[iput].buffer + off, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset, pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);
            off += pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;
            pstr = pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset;
            if (pstr[4] == 0x67)
            {
                iframe = 1;
            }
            // if(pstr[4]==0x68)
            // {
            // UpdatePps(ringfifo[iput].buffer+off,4);
            // }

            // if (pstStream->pstPack[i].u32Len[1] > 0)
            // {
            // memcpy(ringfifo[iput].buffer+off,pstStream->pstPack[i].pu8Addr[1], pstStream->pstPack[i].u32Len[1]);
            // off+=pstStream->pstPack[i].u32Len[1];
            // }
        }

        ringfifo[iput].size = len;
        ringfifo[iput].timestamp = pstStream->pstPack[0].u64PTS;

        // LOGD("HisiPutH264DataToBuffer: ringfifo[iput].timestamp = %"PRIu64" \n ", ringfifo[iput].timestamp);
        // LOGD("iframe=%d\n",iframe);
        if (iframe)
        {
            ringfifo[iput].frame_type = FRAME_TYPE_I;
        }

        else
            ringfifo[iput].frame_type = FRAME_TYPE_P;
        iput = addring(iput);
        n++;

        // fwrite(ringfifo[iput].buffer, len, 1, buffer);
    }
    return HI_SUCCESS;
}

/**
 * @brief for Audio
 */
void ringmalloc_audio(int size)
{
    int i;
    for (i = 0; i < NMAX; i++)
    {
        a_ringfifo[i].buffer = malloc(size);
        a_ringfifo[i].size = 0;
        a_ringfifo[i].frame_type = 0;
        // LOGD("FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
    }
    a_iput = 0; 
    a_iget = 0; 
    a_n = 0;    
}

void ringreset_audio()
{
    a_iput = 0; 
    a_iget = 0; 
    a_n = 0;    
}

void ringfree_audio(void)
{
    int i;
    LOGD("begin free mem\n");
    for (i = 0; i < NMAX; i++)
    {
        // LOGD("FREE FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
        free(a_ringfifo[i].buffer);
        a_ringfifo[i].size = 0;
    }
}

int ringget_audio(struct ringbuf *getinfo)
{
    int Pos;
    if (a_n > 0)
    {
        Pos = a_iget;
        a_iget = addring(a_iget);
        a_n--;
        getinfo->buffer = (a_ringfifo[Pos].buffer);
        getinfo->size = a_ringfifo[Pos].size;
        getinfo->timestamp = a_ringfifo[Pos].timestamp;
        getinfo->getframe_timestamp = a_ringfifo[Pos].getframe_timestamp;
        // LOGD("Get FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",Pos,getinfo->size,(int)(getinfo->buffer),getinfo->frame_type);
        return a_ringfifo[Pos].size;
    }
    else
    {
        // LOGD("Buffer is empty\n");
        return 0;
    }
}

void ringput_audio(unsigned char *buffer, int size)
{
    if (a_n < NMAX)
    {
        memcpy(a_ringfifo[a_iput].buffer, buffer, size);
        a_ringfifo[a_iput].size = size;
        // LOGD("Put FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",iput,ringfifo[iput].size,(int)(ringfifo[iput].buffer),ringfifo[iput].frame_type);
        a_iput = addring(a_iput);
        a_n++;
    }
    else
    {
        //  LOGD("Buffer is full\n");
    }
}

/**
 * @brief Put AAC Audio Data To Buffer
 * 
 * @param aacStream 
 * @return HI_S32 
 */
HI_S32 HisiPutAACDataToBuffer(AUDIO_STREAM_S *aacStream)
{
    HI_S32 i, j;

    if (a_n < NMAX)
    {
        a_ringfifo[a_iput].getframe_timestamp = RTMP_GetTime();
        memcpy(a_ringfifo[a_iput].buffer, aacStream->pStream, aacStream->u32Len);
        a_ringfifo[a_iput].size = aacStream->u32Len;
        a_ringfifo[a_iput].timestamp = aacStream->u64TimeStamp;

        // LOGD("HisiPutAACDataToBuffer: aacStream->u64TimeStamp = %"PRIu64" \n ", aacStream->u64TimeStamp);

        // LOGD("Put FIFO INFO:idx:%d,len:%d,ptr:%x\n",a_iput,aacStream->u32Len,(int)(aacStream->pStream));

        a_iput = addring(a_iput);
        a_n++;
    }
    return HI_SUCCESS;
}

HI_S32 HisiPutOpusDataToBuffer(HI_U8 *opus_data, HI_U32 data_size, HI_U64 timestamp)
{
    HI_S32 i, j;

    if (a_n < NMAX)
    {
        a_ringfifo[a_iput].getframe_timestamp = RTMP_GetTime();
        memcpy(a_ringfifo[a_iput].buffer, opus_data, data_size);
        a_ringfifo[a_iput].size = data_size;
        a_ringfifo[a_iput].timestamp = timestamp;

        a_iput = addring(a_iput);
        a_n++;
    }
    return HI_SUCCESS;
}