/******************************************************************************

  Copyright (C), 2022, Lotogram Tech. Co., Ltd.

 ******************************************************************************
  File Name     : Loto_venc.c
  Version       : Initial Draft
  Author        :
  Created       : 2022
  Description   :
******************************************************************************/

#include "loto_venc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>

#include "loto_comm.h"
#include "ringfifo.h"
#include "loto_osd.h"
#include "common.h"

#define BLACK_COVER_HANDLE 11

extern SAMPLE_VI_CONFIG_S g_stViConfig;
extern SIZE_S g_stSize[2];
extern PIC_SIZE_E g_enSize[2];
extern PIC_SIZE_E g_resolution;
extern int g_profile;
extern PAYLOAD_TYPE_E g_payload;

HI_S32 LOTO_VENC_VB_SYS_Init(HI_U32 u32SupplementConfig, SIZE_S stSnsSize)
{
    HI_S32 s32Ret;
    HI_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;
    PIC_SIZE_E enSnsSize;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));
    stVbConf.u32MaxPoolCnt = 2;

    // Video Buffer
    u64BlkSize = COMMON_GetPicBufferSize(stSnsSize.u32Width, stSnsSize.u32Height,
                                         PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8,
                                         COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = 10;

    u64BlkSize = COMMON_GetPicBufferSize(720, 576, PIXEL_FORMAT_YVU_SEMIPLANAR_422,
                                         DATA_BITWIDTH_8, COMPRESS_MODE_SEG, DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize = u64BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt = 10;

    if (HI_FALSE == u32SupplementConfig)
    {
        s32Ret = LOTO_COMM_SYS_Init(&stVbConf);
    }
    else
    {
        s32Ret = LOTO_COMM_SYS_InitWithVbSupplement(&stVbConf, u32SupplementConfig);
    }
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_ModifyResolution(SAMPLE_SNS_TYPE_E enSnsType, PIC_SIZE_E *penSize, SIZE_S *pstSize)
{
    HI_S32 s32Ret;
    SIZE_S stSnsSize;
    PIC_SIZE_E enSnsSize;

    s32Ret = LOTO_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = LOTO_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    *penSize = enSnsSize;
    pstSize->u32Width = stSnsSize.u32Width;
    pstSize->u32Height = stSnsSize.u32Height;

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_CheckSensor(SAMPLE_SNS_TYPE_E enSnsType, SIZE_S stSize)
{
    HI_S32 s32Ret;
    SIZE_S stSnsSize;
    PIC_SIZE_E enSnsSize;

    s32Ret = LOTO_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = LOTO_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    if ((stSnsSize.u32Width < stSize.u32Width) || (stSnsSize.u32Height < stSize.u32Height))
    {
        LOGE("Sensor size is (%d,%d), but encode chnl is (%d,%d) !\n",
             stSnsSize.u32Width, stSnsSize.u32Height, stSize.u32Width, stSize.u32Height);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_VI_Init(SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay,
                         HI_U32 u32SupplementConfig)
{
    HI_S32 s32Ret;
    SAMPLE_SNS_TYPE_E enSnsType;
    ISP_CTRL_PARAM_S stIspCtrlParam;
    HI_U32 u32FrameRate;

    HI_S32 s32ViCnt = 1;
    VI_DEV ViDev = 0;
    VI_PIPE ViPipe = 0;
    VI_CHN ViChn = 0;
    HI_S32 s32WorkSnsId = 0;

    WDR_MODE_E enWDRMode = WDR_MODE_NONE;
    DYNAMIC_RANGE_E enDynamicRange = DYNAMIC_RANGE_SDR8;
    PIXEL_FORMAT_E enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    VIDEO_FORMAT_E enVideoFormat = VIDEO_FORMAT_LINEAR;
    COMPRESS_MODE_E enCompressMode = COMPRESS_MODE_NONE;
    VI_VPSS_MODE_E enMastPipeMode = VI_OFFLINE_VPSS_OFFLINE;

    if (bLowDelay == HI_TRUE)
    {
        enMastPipeMode = VI_ONLINE_VPSS_ONLINE;
    }

    pstViConfig->s32WorkingViNum = s32ViCnt;
    for (int i = 0; i < s32ViCnt; i++)
    {
        ViDev = i;
        ViPipe = i;
        s32WorkSnsId = i;

        pstViConfig->as32WorkingViId[s32WorkSnsId] = s32WorkSnsId;
        pstViConfig->astViInfo[s32WorkSnsId].stSnsInfo.MipiDev = ViDev;
        pstViConfig->astViInfo[s32WorkSnsId].stSnsInfo.s32BusId = i;

        pstViConfig->astViInfo[s32WorkSnsId].stDevInfo.ViDev = ViDev;
        pstViConfig->astViInfo[s32WorkSnsId].stDevInfo.enWDRMode = enWDRMode;

        pstViConfig->astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = enMastPipeMode;
        pstViConfig->astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0] = i == 0 ? ViPipe : -1;
        pstViConfig->astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1] = i == 1 ? ViPipe : -1;
        pstViConfig->astViInfo[s32WorkSnsId].stPipeInfo.aPipe[2] = -1;
        pstViConfig->astViInfo[s32WorkSnsId].stPipeInfo.aPipe[3] = -1;

        pstViConfig->astViInfo[s32WorkSnsId].stChnInfo.ViChn = ViChn;
        pstViConfig->astViInfo[s32WorkSnsId].stChnInfo.enPixFormat = enPixFormat;
        pstViConfig->astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange = enDynamicRange;
        pstViConfig->astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat = enVideoFormat;
        pstViConfig->astViInfo[s32WorkSnsId].stChnInfo.enCompressMode = enCompressMode;
    }

    s32Ret = LOTO_VENC_VB_SYS_Init(u32SupplementConfig, g_stSize[0]);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_VENC_VB_SYS_Init failed! \n");
        return s32Ret;
    }

    s32Ret = LOTO_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_VI_SetParam failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    LOTO_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_ISP_GetCtrlParam failed with %#x!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl = u32FrameRate / 30;

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_ISP_SetCtrlParam failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = LOTO_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        LOTO_COMM_SYS_Exit();
        LOGE("LOTO_COMM_VI_StartVi failed with %#x!\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL *pabChnEnable,
                           DYNAMIC_RANGE_E enDynamicRange, PIXEL_FORMAT_E enPixelFormat,
                           SIZE_S *stSize, SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 i;
    HI_S32 s32Ret;
    PIC_SIZE_E enSnsSize;
    SIZE_S stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr;
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    s32Ret = LOTO_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = LOTO_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    memset_s(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat = enPixelFormat;
    stVpssGrpAttr.u32MaxW = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;

    for (i = 0; i < VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if (HI_TRUE == pabChnEnable[i])
        {
            stVpssChnAttr[i].u32Width = stSize[i].u32Width;
            stVpssChnAttr[i].u32Height = stSize[i].u32Height;
            stVpssChnAttr[i].enChnMode = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_MODE_SEG;
            stVpssChnAttr[i].enDynamicRange = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate = 30;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate = 30;
            stVpssChnAttr[i].u32Depth = 0;
            stVpssChnAttr[i].bMirror = HI_FALSE;
            stVpssChnAttr[i].bFlip = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode = ASPECT_RATIO_NONE; // ASPECT_RATIO_AUTO;//ASPECT_RATIO_NONE;
        }
    }

    s32Ret = LOTO_COMM_VPSS_Start(VpssGrp, pabChnEnable, &stVpssGrpAttr, stVpssChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_VPSS_Start failed! \n");
    }

    return s32Ret;
}

void *LOTO_VENC_CLASSIC(void *arg)
{
    LOGI("=== LOTO_VENC_CLASSIC ===\n");

    HI_S32 s32Ret;
    VI_PIPE workingViPipe = 0;
    VI_CHN ViChn = 0;
    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn[2] = {0, 1};
    HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {1, 0, 0};

    // HI_U32 u32Profile[2] = {0, 2};
    PAYLOAD_TYPE_E enPayLoad[2] = {PT_H265, PT_H264};
    VENC_GOP_MODE_E enGopMode;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode;
    HI_BOOL bRcnRefShareBuf = HI_TRUE;
    HI_S32 s32ChnNum = 1;
    VENC_CHN VencChn[2] = {0, 1};
    pthread_t venc_pid;
    VPSS_LOW_DELAY_INFO_S stLowDelayInfo;

    LOGI("Encode PIC_SIZE:    %d\n", g_resolution);

    s32Ret = LOTO_VENC_VPSS_Init(VpssGrp, abChnEnable, DYNAMIC_RANGE_SDR8,
                                 PIXEL_FORMAT_YVU_SEMIPLANAR_420, g_stSize,
                                 g_stViConfig.astViInfo[0].stSnsInfo.enSnsType);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_VENC_VPSS_Init failed! \n");
        goto EXIT_VI_STOP;
    }

    LOGD("workingViPipe = %d\n", workingViPipe);
    s32Ret = LOTO_COMM_VI_Bind_VPSS(workingViPipe, ViChn, VpssGrp);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_COMM_VI_Bind_VPSS failed! \n");
        goto EXIT_VPSS_STOP;
    }

    // stLowDelayInfo.bEnable = HI_TRUE;
    // stLowDelayInfo.u32LineCnt = 128;//s_stSize[1].u32Height/2;
    // s32Ret = HI_MPI_VPSS_SetLowDelayAttr(VpssGrp,VpssChn[0],&stLowDelayInfo);
    // if(s32Ret != HI_SUCCESS)
    // {
    //     LOGE("Set LowDelay Attr for %#x!\n", s32Ret);
    //     goto EXIT_VPSS_STOP;
    // }

    /******************************************
     start stream venc
     ******************************************/
    enRcMode    = SAMPLE_RC_CBR;
    // enRcMode = SAMPLE_RC_VBR;

    enGopMode   = VENC_GOPMODE_NORMALP;
    // enGopMode = VENC_GOPMODE_DUALP;
    // enGopMode   = VENC_GOPMODE_SMARTP;

    s32Ret = LOTO_COMM_VENC_GetGopAttr(enGopMode, &stGopAttr);
    if (VENC_GOPMODE_DUALP == enGopMode)
    {
        LOGD("enGopMode = %d, u32SPInterval = %d \n", enGopMode, stGopAttr.stDualP.u32SPInterval);
    }
    else if (VENC_GOPMODE_SMARTP == enGopMode)
    {
        LOGD("enGopMode = %d, u32BgInterval = %d \n", enGopMode, stGopAttr.stSmartP.u32BgInterval);
    }

    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Venc Get GopAttr failed with %#x! \n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    /* Encode h.264 */
    /* Modify the resolution   g_enSize[0]: 1080p; g_enSize[1]: 720p */
    // g_resolution = PIC_2592x1944;
    // switch (g_resolution)
    // {
    // case PIC_1080P:
    //     s32Ret = LOTO_COMM_VENC_Start(VencChn[0], g_payload, PIC_1080P, enRcMode, g_profile, bRcnRefShareBuf, &stGopAttr);
    //     break;
    // case PIC_720P:
    //     s32Ret = LOTO_COMM_VENC_Start(VencChn[0], g_payload, PIC_720P, enRcMode, g_profile, bRcnRefShareBuf, &stGopAttr);
    //     break;
    // case PIC_2592x1944:
    //     s32Ret = LOTO_COMM_VENC_Start(VencChn[0], g_payload, PIC_2592x1944, enRcMode, g_profile, bRcnRefShareBuf, &stGopAttr);
    //     break;
    // default:
    //     s32Ret = LOTO_COMM_VENC_Start(VencChn[0], g_payload, PIC_1080P, enRcMode, g_profile, bRcnRefShareBuf, &stGopAttr);
    //     break;
    // }

    s32Ret = LOTO_COMM_VENC_Start(VencChn[0], g_payload, g_resolution, enRcMode, g_profile, bRcnRefShareBuf, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Venc Start failed with %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    s32Ret = LOTO_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[0], VencChn[0]);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Venc Get GopAttr failed with %#x!\n", s32Ret);
        goto EXIT_VENC_H264_STOP;
    }

    /******************************************
     stream save process
    ******************************************/
    s32ChnNum = 1;
    s32Ret = LOTO_COMM_VENC_StartGetStream(&venc_pid, VencChn, s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("Start Venc failed!\n");
        goto EXIT_VENC_H264_UnBind;
    }

    pthread_join(venc_pid, 0);

    /******************************************
     exit process
    ******************************************/
    LOTO_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
    LOTO_COMM_VPSS_UnBind_VENC(VpssGrp, VpssChn[0], VencChn[0]);
EXIT_VENC_H264_STOP:
    LOTO_COMM_VENC_Stop(VencChn[0]);
EXIT_VI_VPSS_UNBIND:
    LOTO_COMM_VI_UnBind_VPSS(workingViPipe, ViChn, VpssGrp);
EXIT_VPSS_STOP:
    LOTO_COMM_VPSS_Stop(VpssGrp, abChnEnable);
EXIT_VI_STOP:
    LOTO_COMM_VI_StopVi(&g_stViConfig);
    LOTO_COMM_SYS_Exit();

    return NULL;
}

HI_S32 LOTO_VENC_FramerateDown(HI_BOOL enable)
{
    HI_S32 s32Ret;
    const HI_S32 target_frame_rate = 5;
    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = VPSS_CHN0;
    VENC_CHN VencChn = 0;
    VPSS_GRP_ATTR_S pstVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S pstVpssChnAttr = {0};
    VENC_CHN_ATTR_S pstVencChnAttr = {0};

    s32Ret = HI_MPI_VPSS_GetGrpAttr(VpssGrp, &pstVpssGrpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Get Vpss Group Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &pstVpssChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Get Vpss Channel Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &pstVencChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Get Venc Channel Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    if (HI_TRUE == enable)
    {
        /* Group attributes */
        pstVpssGrpAttr.stFrameRate.s32SrcFrameRate = 30;
        pstVpssGrpAttr.stFrameRate.s32DstFrameRate = target_frame_rate;

        /* Channel attributes */
        pstVpssChnAttr.stFrameRate.s32SrcFrameRate = 30;
        pstVpssChnAttr.stFrameRate.s32DstFrameRate = target_frame_rate;

        if (VENC_RC_MODE_H264VBR == pstVencChnAttr.stRcAttr.enRcMode)
        {
            pstVencChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRate = target_frame_rate;
            pstVencChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate = 128;
        }
    }
    else if (HI_FALSE == enable)
    {
        pstVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
        pstVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
        pstVpssChnAttr.stFrameRate.s32SrcFrameRate = -1;
        pstVpssChnAttr.stFrameRate.s32DstFrameRate = -1;

        if (VENC_RC_MODE_H264VBR == pstVencChnAttr.stRcAttr.enRcMode)
        {
            pstVencChnAttr.stRcAttr.stH264Vbr.fr32DstFrameRate = 30;
            pstVencChnAttr.stRcAttr.stH264Vbr.u32MaxBitRate = 1024 * 3;
        }
    }

    s32Ret = HI_MPI_VPSS_SetGrpAttr(VpssGrp, &pstVpssGrpAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Set Vpss Group Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &pstVpssChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Set Vpss Channel Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_SetChnAttr(VencChn, &pstVencChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("Set Venc Channel Attribute failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

static RGN_HANDLE gs_rgnHandle = 5;
static MPP_CHN_S gs_stMppChnAttr = {0};
static RGN_CHN_ATTR_S gs_stRgnChnAttr = {0};

HI_S32 LOTO_RGN_CreateCoverRegion(RGN_HANDLE rgnHandle, MPP_CHN_S *stMppChnAttr, RGN_CHN_ATTR_S *stRgnChnAttr)
{
    HI_S32 ret;
    RGN_ATTR_S stRgnAttr;

    // stRgnAttr.enType = COVER_RGN;

    // stMppChnAttr->enModId = HI_ID_VPSS;
    // stMppChnAttr->s32DevId = 0;
    // stMppChnAttr->s32ChnId = 0;

    // stRgnChnAttr->bShow = HI_TRUE;
    // stRgnChnAttr->enType = COVER_RGN;
    // stRgnChnAttr->unChnAttr.stCoverChn.enCoverType = AREA_RECT;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.s32X = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.s32Y = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.u32Width = 1920;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.u32Height = 1080;
    // stRgnChnAttr->unChnAttr.stCoverChn.u32Color = 0xFF4500;
    // stRgnChnAttr->unChnAttr.stCoverChn.u32Layer = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR;

    stRgnAttr.enType = COVEREX_RGN;

    stMppChnAttr->enModId = HI_ID_VPSS;
    stMppChnAttr->s32DevId = 0;
    stMppChnAttr->s32ChnId = 0;

    stRgnChnAttr->bShow = HI_TRUE;
    stRgnChnAttr->enType = COVEREX_RGN;
    stRgnChnAttr->unChnAttr.stCoverExChn.enCoverType = AREA_RECT;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.s32X = 1080;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.s32Y = 0;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.u32Width = 1080;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.u32Height = 1920;
    // stRgnChnAttr->unChnAttr.stCoverExChn.u32Color = 0xFF4500;
    stRgnChnAttr->unChnAttr.stCoverExChn.u32Color = 0x202020;
    stRgnChnAttr->unChnAttr.stCoverExChn.u32Layer = 0;

    ret = HI_MPI_RGN_Create(rgnHandle, &stRgnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Create failed with %#x!\n", ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 LOTO_RGN_DestroyCoverRegion(RGN_HANDLE rgnHandle)
{
    HI_S32 ret;
    ret = HI_MPI_RGN_Destroy(rgnHandle);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Destroy failed with %#x!\n", ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 LOTO_RGN_AttachCover(RGN_HANDLE rgnHandle, const MPP_CHN_S *stMppChnAttr, const RGN_CHN_ATTR_S *stRgnChnAttr)
{
    HI_S32 ret;

    ret = HI_MPI_RGN_AttachToChn(rgnHandle, stMppChnAttr, stRgnChnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_AttachToChn failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    // LOGI("cover_add!\n");
    return HI_SUCCESS;
}

HI_S32 LOTO_RGN_DetachCover(RGN_HANDLE rgnHandle, const MPP_CHN_S *stMppChnAttr)
{
    HI_S32 ret;

    ret = HI_MPI_RGN_DetachFromChn(rgnHandle, stMppChnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_DetachFromChn failed with %#x\n", ret);
        return HI_FAILURE;
    }

    // LOGI("cover_remove!\n");
    return HI_SUCCESS;
}

HI_S32 LOTO_RGN_InitCoverRegion()
{
    return LOTO_RGN_CreateCoverRegion(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
}

HI_S32 LOTO_RGN_UninitCoverRegion()
{
    return LOTO_RGN_DestroyCoverRegion(gs_rgnHandle);
}

HI_S32 LOTO_VENC_AttachCover()
{
    return LOTO_RGN_AttachCover(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
}

HI_S32 LOTO_VENC_DetachCover()
{
    return LOTO_RGN_DetachCover(gs_rgnHandle, &gs_stMppChnAttr);
}

HI_S32 LOTO_VENC_AddCover() {
    HI_S32 ret;

    ret = HI_MPI_RGN_GetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_GetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 0;
    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 0;

    ret = HI_MPI_RGN_SetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_SetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    LOGI("cover_add!\n");

    return ret;
}

HI_S32 LOTO_VENC_RemoveCover () {
    HI_S32 ret;

    ret = HI_MPI_RGN_GetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_GetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 1080;
    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 0;

    ret = HI_MPI_RGN_SetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_SetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    LOGI("cover_remove!\n");

    return ret;
}