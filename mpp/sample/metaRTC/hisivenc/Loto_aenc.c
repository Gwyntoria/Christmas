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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>
#include "sample_comm.h"

#include "./adp/audio_aac_adp.h"
#include "../common.h"
#include  "../loto_BufferManager.h"


typedef struct tagLOTO_AENC_S
{
    HI_BOOL bStart;
    pthread_t stAencPid;
    HI_S32  AeChn;
} LOTO_AENC_S;

#define AUDIO_ADPCM_TYPE ADPCM_TYPE_DVI4/* ADPCM_TYPE_IMA, ADPCM_TYPE_DVI4*/
#define G726_BPS MEDIA_G726_40K         /* MEDIA_G726_16K, MEDIA_G726_24K ... */

static LOTO_AENC_S gs_stLotoAenc[AENC_MAX_CHN_NUM];


#define LOTO_DBG(s32Ret)\
    do{\
        printf("s32Ret=%#x,fuc:%s,line:%d\n", s32Ret, __FUNCTION__, __LINE__);\
    }while(0)



/******************************************************************************
* function : get stream from Aenc, send it  to ringfifo
******************************************************************************/
void* LOTO_COMM_AUDIO_AencProc(void* parg)
{
    HI_S32 s32Ret;
    HI_S32 AencFd;
    LOTO_AENC_S* pstAencCtl = (LOTO_AENC_S*)parg;
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
            LOGD("[%s] get aenc stream select time out! \n", log_Time());
            break;
        }

        if (FD_ISSET(AencFd, &read_fds))
        {
            /* get stream from aenc chn */
              s32Ret = HI_MPI_AENC_GetStream(pstAencCtl->AeChn, &stStream, HI_FALSE);
            if (HI_SUCCESS != s32Ret )
            {
                printf("%s: HI_MPI_AENC_GetStream(%d), failed with %#x!\n", \
                       __FUNCTION__, pstAencCtl->AeChn, s32Ret);
                LOGD("[%s] HI_MPI_AENC_GetStream(%d), failed with %#x! \n", log_Time(), pstAencCtl->AeChn, s32Ret);
                pstAencCtl->bStart = HI_FALSE;
                return NULL;
            }

            HisiPutAACDataToBuffer(&stStream);

            /* finally you must release the stream */
            s32Ret = HI_MPI_AENC_ReleaseStream(pstAencCtl->AeChn, &stStream);
            if (HI_SUCCESS != s32Ret )
            {
                printf("%s: HI_MPI_AENC_ReleaseStream(%d), failed with %#x!\n", \
                       __FUNCTION__, pstAencCtl->AeChn, s32Ret);
                LOGD("[%s] HI_MPI_AENC_ReleaseStream(%d), failed with %#x! \n", log_Time(), pstAencCtl->AeChn, s32Ret);
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
    LOTO_AENC_S* pstAenc = NULL;

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
HI_S32 LOTO_AUDIO_CreatTrdAenc(pthread_t* aencPid, AENC_CHN AeChn)
{
    LOTO_AENC_S* pstAenc = NULL;

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
    HI_S32      i, j, s32Ret;
    AI_CHN      AiChn;
    HI_S32      s32AiChnCnt;
    HI_S32      s32AencChnCnt;
    AENC_CHN    AeChn = 0;
    AIO_ATTR_S  stAioAttr;
    pthread_t   aenc_Pid;

    AUDIO_DEV   AiDev = SAMPLE_AUDIO_INNER_AI_DEV;
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_44100;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_I2S_MASTER;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_STEREO;
    stAioAttr.u32EXFlag      = 1;
    stAioAttr.u32FrmNum      = 5;
    stAioAttr.u32PtNumPerFrm = AACLC_SAMPLES_PER_FRAME;
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;

    /********************************************
      step 1: start Ai
    ********************************************/
    s32AiChnCnt = stAioAttr.u32ChnCnt;
    s32Ret = SAMPLE_COMM_AUDIO_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] SAMPLE_COMM_AUDIO_StartAi failed for %#x! \n", log_Time(), s32Ret);
        goto AIAENC_ERR6;
    }

    /********************************************
      step 2: config audio codec
    ********************************************/
    s32Ret = SAMPLE_COMM_AUDIO_CfgAcodec(&stAioAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] SAMPLE_COMM_AUDIO_CfgAcodec failed for %#x! \n", log_Time(), s32Ret);
        goto AIAENC_ERR5;
    }

    /********************************************
      step 3: start Aenc
    ********************************************/
    s32AencChnCnt = stAioAttr.u32ChnCnt >> stAioAttr.enSoundmode;
    s32Ret = SAMPLE_COMM_AUDIO_StartAenc(s32AencChnCnt, &stAioAttr, PT_AAC);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] SAMPLE_COMM_AUDIO_StartAenc failed for %#x! \n", log_Time(), s32Ret);
        goto AIAENC_ERR5;
    }

    /********************************************
      step 4: Aenc bind Ai Chn
    ********************************************/
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        s32Ret = SAMPLE_COMM_AUDIO_AencBindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            LOTO_DBG(s32Ret);
            for (j=0; j<i; j++)
            {
                SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, j, j);
            }
            goto AIAENC_ERR4;
        }
        printf("Ai(%d,%d) bind to AencChn:%d ok!\n", AiDev , AiChn, AeChn);
    }

    /********************************************
      step 5: start Adec & Ao. ( if you want )
    ********************************************/
    s32Ret = LOTO_AUDIO_CreatTrdAenc(&aenc_Pid, AeChn);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] LOTO_AUDIO_CreatTrdAenc failed for %#x! \n", log_Time(), s32Ret);
        goto AIAENC_ERR3;
    }

    printf("LOTO_AUDIO_CreatTrdAenc: aenc_Pid = %d!\n", aenc_Pid);
    pthread_join(aenc_Pid, 0);

    printf("LOTO_AUDIO_CreatTrdAenc!\n");

    /********************************************
      step 6: exit the process
    ********************************************/
    s32Ret = LOTO_AUDIO_DestoryTrdAenc(AeChn);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] LOTO_AUDIO_DestoryTrdAenc failed for %#x! \n", log_Time(), s32Ret);
    }

AIAENC_ERR3:
    for (i = 0; i < s32AencChnCnt; i++)
    {
        AeChn = i;
        AiChn = i;

        s32Ret |= SAMPLE_COMM_AUDIO_AencUnbindAi(AiDev, AiChn, AeChn);
        if (s32Ret != HI_SUCCESS)
        {
            LOTO_DBG(s32Ret);
            LOGD("[%s] SAMPLE_COMM_AUDIO_AencUnbindAi failed for %#x! \n", log_Time(), s32Ret);
        }
    }

AIAENC_ERR4:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAenc(s32AencChnCnt);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] SAMPLE_COMM_AUDIO_StopAenc failed for %#x! \n", log_Time(), s32Ret);
    }

AIAENC_ERR5:
    s32Ret |= SAMPLE_COMM_AUDIO_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        LOTO_DBG(s32Ret);
        LOGD("[%s] SAMPLE_COMM_AUDIO_StopAi failed for %#x! \n", log_Time(), s32Ret);
    }

AIAENC_ERR6:

    return s32Ret;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_AUDIO_HandleSig(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo)
    {

        // SAMPLE_COMM_AUDIO_DestoryAllTrd();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram exit abnormally!\033[0;39m\n");
    }

    exit(0);
}

void * LOTO_AENC_AAC_CLASSIC(void *p)
{
    // signal(SIGINT, SAMPLE_AUDIO_HandleSig);
    // signal(SIGTERM, SAMPLE_AUDIO_HandleSig);

    HI_MPI_AENC_AacInit();
    LOTO_AUDIO_AiAenc();
    HI_MPI_AENC_AacDeInit();
    SAMPLE_COMM_SYS_Exit();

    return  NULL;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */