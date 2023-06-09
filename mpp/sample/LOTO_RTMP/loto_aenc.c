/******************************************************************************

  Copyright (C), 2022, Lotogram Tech. Co., Ltd.

 ******************************************************************************
  File Name     : Loto_venc.c
  Version       : Initial Draft
  Author        :
  Created       : 2022
  Description   :
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include "loto_aenc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>

#include "opus.h"
#include "opus_types.h"
#include "hi_type.h"
#include "hi_comm_aenc.h"
#include "mpi_audio.h"

#include "common.h"
#include "loto_comm.h"
#include "loto_opus.h"
#include "audio_aac_adp.h"
#include "ringfifo.h"

#define AUDIO_ADPCM_TYPE ADPCM_TYPE_DVI4 /* ADPCM_TYPE_IMA, ADPCM_TYPE_DVI4*/
#define G726_BPS MEDIA_G726_40K          /* MEDIA_G726_16K, MEDIA_G726_24K ... */

#define G711_SAMPLES_PER_FRAME 480

static LOTO_AENC_S gs_stLotoAenc[AENC_MAX_CHN_NUM];

/******************************************************************************
 * function : get stream from Aenc, send it  to ringfifo
 ******************************************************************************/
void *LOTO_COMM_AUDIO_AencProc(void *parg)
{
    HI_S32 s32Ret;
    HI_S32 AencFd;
    LOTO_AENC_S *pstAencCtl = (LOTO_AENC_S *)parg;
    AUDIO_STREAM_S stStream;
    fd_set read_fds;
    struct timeval TimeoutVal;

    FD_ZERO(&read_fds);
    AencFd = HI_MPI_AENC_GetFd(pstAencCtl->AeChn);
    FD_SET(AencFd, &read_fds);

    while (pstAencCtl->bStart)
    {
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(AencFd, &read_fds);

        s32Ret = select(AencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            break;
        }
        else if (0 == s32Ret)
        {
            printf("%s: get aenc stream select time out\n", __FUNCTION__);
            break;
        }

        if (FD_ISSET(AencFd, &read_fds))
        {
            /* get stream from aenc chn */
            s32Ret = HI_MPI_AENC_GetStream(pstAencCtl->AeChn, &stStream, HI_FALSE);
            if (HI_SUCCESS != s32Ret)
            {
                printf("%s: HI_MPI_AENC_GetStream(%d), failed with %#x!\n",
                       __FUNCTION__, pstAencCtl->AeChn, s32Ret);
                pstAencCtl->bStart = HI_FALSE;
                return NULL;
            }

            HisiPutAACDataToBuffer(&stStream);

            /* finally you must release the stream */
            s32Ret = HI_MPI_AENC_ReleaseStream(pstAencCtl->AeChn, &stStream);
            if (HI_SUCCESS != s32Ret)
            {
                printf("%s: HI_MPI_AENC_ReleaseStream(%d), failed with %#x!\n",
                       __FUNCTION__, pstAencCtl->AeChn, s32Ret);
                pstAencCtl->bStart = HI_FALSE;
                return NULL;
            }
        }
    }

    pstAencCtl->bStart = HI_FALSE;
    return NULL;
}

/******************************************************************************
 * function : Destory the thread to get stream from aenc
 ******************************************************************************/
HI_S32 LOTO_AUDIO_DestoryTrdAenc(AENC_CHN AeChn)
{
    LOGI("=== LOTO_AUDIO_DestoryTrdAenc ===\n");
    LOTO_AENC_S *pstAenc = NULL;

    pstAenc = &gs_stLotoAenc[AeChn];
    if (pstAenc->bStart)
    {
        pstAenc->bStart = HI_FALSE;
    }

    return HI_SUCCESS;
}

/******************************************************************************
 * function : Create the thread to get stream from aenc and send to ringfifo
 ******************************************************************************/
HI_S32 LOTO_AUDIO_CreatTrdAenc(pthread_t *aencPid, AENC_CHN AeChn)
{
    LOGI("=== LOTO_AUDIO_CreatTrdAenc ===\n");
    LOTO_AENC_S *pstAenc = NULL;

    pstAenc = &gs_stLotoAenc[AeChn];
    pstAenc->AeChn = AeChn;
    pstAenc->bStart = HI_TRUE;
    HI_S32 nRet = pthread_create(&pstAenc->stAencPid, 0, LOTO_COMM_AUDIO_AencProc, pstAenc);

    if (nRet == 0)
    {
        *aencPid = pstAenc->stAencPid;
    }
    return nRet;
}

/******************************************************************************
 * function : Ai -> Aenc -> buffer
 ******************************************************************************/
HI_S32 LOTO_AUDIO_AiAenc(HI_VOID)
{
    LOGI("=== LOTO_AUDIO_AiAenc ===\n");

    HI_S32 i, j, s32Ret;
    AI_CHN AiChn;
    HI_S32 s32AiChnCnt;
    HI_S32 s32AencChnCnt;
    AENC_CHN AeChn = 0;
    AIO_ATTR_S stAioAttr;
    pthread_t aenc_thread_id;

    AUDIO_DEV AiDev = SAMPLE_AUDIO_INNER_AI_DEV;

    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_44100;

    stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode = AIO_MODE_I2S_MASTER;
    // stAioAttr.enWorkmode = AIO_MODE_PCM_MASTER_STD;
    stAioAttr.enSoundmode = AUDIO_SOUND_MODE_STEREO;
    stAioAttr.u32EXFlag = 1;
    stAioAttr.u32FrmNum = 5;
    stAioAttr.u32PtNumPerFrm = AACLC_SAMPLES_PER_FRAME;
    stAioAttr.u32ChnCnt = 2;
    stAioAttr.u32ClkSel = 0;
    stAioAttr.enI2sType = AIO_I2STYPE_INNERCODEC;

    LOGD("Ai attributes: samplerate = %d, bitwitdth = %d, soundmode = %d\n", stAioAttr.enSamplerate, stAioAttr.enBitwidth, stAioAttr.enSoundmode);

    /********************************************
         step 1: start Ai
    ********************************************/
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = LOTO_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_AUDIO_StartAi failed with %#x!\n", s32Ret);
        goto AIAENC_ERR6;
    }

    /********************************************
         step 2: config audio codec
    ********************************************/
    s32Ret = LOTO_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_AUDIO_CfgAcodec failed with %#x\n", s32Ret);
        goto AIAENC_ERR5;
    }

    /********************************************
         step 3: start Aenc
    ********************************************/
    s32AencChnCnt = stAioAttr.u32ChnCnt >> stAioAttr.enSoundmode;
    // s32AencChnCnt = 1;
    s32Ret = LOTO_COMM_AUDIO_StartAenc(s32AencChnCnt, &stAioAttr, PT_AAC);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_AUDIO_StartAenc failed with %#x!\n", s32Ret);
        goto AIAENC_ERR5;
    }

    /********************************************
         step 4: Aenc bind Ai Chn
    ********************************************/
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        s32Ret = LOTO_COMM_AUDIO_AencBindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            LOGE("LOTO_COMM_AUDIO_AencBindAi failed with %#x!\n", s32Ret);
            for (j = 0; j < i; j++)
            {
                LOTO_COMM_AUDIO_AencUnbindAi(AiDev, j, j);
            }
            goto AIAENC_ERR4;
        }
        LOGI("Ai(%d,%d) bind to AencChn:%d ok!\n", AiDev, AiChn, AeChn);
    }

    /********************************************
        step 5: start Adec & Ao. ( if you want )
    ********************************************/
    s32Ret = LOTO_AUDIO_CreatTrdAenc(&aenc_thread_id, AeChn);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_AUDIO_CreatTrdAenc failed with %#x!\n", s32Ret);
        goto AIAENC_ERR3;
    }
    LOGD("aenc_thread_id = %#x!\n", aenc_thread_id);

    pthread_join(aenc_thread_id, 0);

    /********************************************
        step 6: exit the process
    ********************************************/
    s32Ret = LOTO_AUDIO_DestoryTrdAenc(AeChn);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_AUDIO_DestoryTrdAenc failed with %#x!\n", s32Ret);
    }

AIAENC_ERR3:
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        s32Ret |= LOTO_COMM_AUDIO_AencUnbindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            LOGE("LOTO_COMM_AUDIO_AencUnbindAi failed with %#x!\n", s32Ret);
        }
    }

AIAENC_ERR4:
    s32Ret |= LOTO_COMM_AUDIO_StopAenc(s32AencChnCnt);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_AUDIO_StopAenc failed with %#x!\n", s32Ret);
    }

AIAENC_ERR5:
    s32Ret |= LOTO_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_AUDIO_StopAi failed with %#x!\n", s32Ret);
    }

AIAENC_ERR6:

    return s32Ret;
}

void *LOTO_AENC_CLASSIC(void *p)
{
    LOGI("=== LOTO_AENC_CLASSIC ===\n");

    /* 注册 AAC 编码器 */
    HI_MPI_AENC_AacInit();

    /* AI --> AENC --> Buffer */
    LOTO_AUDIO_AiAenc();

    /* 注销 AAC 编码器 */
    HI_MPI_AENC_AacDeInit();

    /* Exit system */
    // LOTO_COMM_SYS_Exit();

    return NULL;
}

HI_S32 LOTO_AUDIO_SetMute(HI_BOOL mute_mode) {
    HI_S32 ret = 0;
    AENC_CHN aenc_chn = 0;
    HI_BOOL enalbe;

    ret = HI_MPI_AENC_GetMute(aenc_chn, &enalbe);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_AENC_GetMute failed with %#x\n", ret);
        return HI_FAILURE;
    }

    if (mute_mode != enalbe) {
        ret = HI_MPI_AENC_SetMute(aenc_chn, mute_mode);
        if (ret != HI_SUCCESS) {
            LOGE("HI_MPI_AENC_SetMute failed with %#x\n", ret);
            return HI_FAILURE;
        }
    }

    // if (mute_mode == HI_FALSE) {
    //     LOGD("Unmute!\n");
    // } else if (mute_mode == HI_TRUE) {
    //     LOGD("Mute!\n");
    // }

    return HI_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */