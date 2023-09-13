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
#include "../loto_BufferManager.h"
#include "../common.h"


HI_S32 LOTO_VENC_SYS_Init(HI_U32 u32SupplementConfig,SAMPLE_SNS_TYPE_E  enSnsType)
{
    HI_S32 s32Ret;
    HI_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;
    PIC_SIZE_E enSnsSize;
    SIZE_S     stSnsSize;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        LOGD("[%s] SAMPLE_COMM_VI_GetSizeBySensor failed! \n", log_Time());
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        LOGD("[%s] SAMPLE_COMM_SYS_GetPicSize failed! \n", log_Time());
        return s32Ret;
    }

    u64BlkSize = COMMON_GetPicBufferSize(stSnsSize.u32Width, stSnsSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8, COMPRESS_MODE_SEG,DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize   = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt    = 10;

    u64BlkSize = COMMON_GetPicBufferSize(720, 576, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8, COMPRESS_MODE_SEG,DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize   = u64BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt    = 10;

    stVbConf.u32MaxPoolCnt = 2;

    if(0 == u32SupplementConfig)
    {
        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    }
    else
    {
        s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf,u32SupplementConfig);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        LOGD("[%s] SAMPLE_COMM_SYS_GetPicSize failed! \n", log_Time());
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_ModifyResolution(SAMPLE_SNS_TYPE_E   enSnsType,PIC_SIZE_E *penSize,SIZE_S *pstSize)
{
    HI_S32 s32Ret;
    SIZE_S          stSnsSize;
    PIC_SIZE_E      enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        LOGD("[%s] SAMPLE_COMM_VI_GetSizeBySensor failed! \n", log_Time());
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        LOGD("[%s] SAMPLE_COMM_SYS_GetPicSize failed! \n", log_Time());
        return s32Ret;
    }

    *penSize = enSnsSize;
    pstSize->u32Width  = stSnsSize.u32Width;
    pstSize->u32Height = stSnsSize.u32Height;

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_CheckSensor(SAMPLE_SNS_TYPE_E   enSnsType,SIZE_S  stSize)
{
    HI_S32 s32Ret;
    SIZE_S          stSnsSize;
    PIC_SIZE_E      enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        LOGD("[%s] SAMPLE_COMM_VI_GetSizeBySensor failed! \n", log_Time());
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        LOGD("[%s] SAMPLE_COMM_SYS_GetPicSize failed! \n", log_Time());
        return s32Ret;
    }

    if((stSnsSize.u32Width < stSize.u32Width) || (stSnsSize.u32Height < stSize.u32Height))
    {
        //SAMPLE_PRT("Sensor size is (%d,%d), but encode chnl is (%d,%d) !\n",
        //    stSnsSize.u32Width,stSnsSize.u32Height,stSize.u32Width,stSize.u32Height);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig)
{
    HI_S32              s32Ret;
    SAMPLE_SNS_TYPE_E   enSnsType;
    ISP_CTRL_PARAM_S    stIspCtrlParam;
    HI_U32              u32FrameRate;


    enSnsType = pstViConfig->astViInfo[0].stSnsInfo.enSnsType;

    pstViConfig->as32WorkingViId[0]                           = 0;
    //pstViConfig->s32WorkingViNum                              = 1;

    pstViConfig->astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);
    pstViConfig->astViInfo[0].stSnsInfo.s32BusId           = 0;

    //pstViConfig->astViInfo[0].stDevInfo.ViDev              = ViDev0;
    pstViConfig->astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    if(HI_TRUE == bLowDelay)
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_ONLINE_VPSS_ONLINE;
    }
    else
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_OFFLINE_VPSS_OFFLINE;
    }
    s32Ret = LOTO_VENC_SYS_Init(u32SupplementConfig,enSnsType);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Init SYS err for %#x!\n", s32Ret);
        LOGD("[%s] Init SYS err for %#x! \n", log_Time(), s32Ret);
        return s32Ret;
    }

    //pstViConfig->astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    //pstViConfig->astViInfo[0].stChnInfo.ViChn              = ViChn;
    //pstViConfig->astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    //pstViConfig->astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    pstViConfig->astViInfo[0].stChnInfo.enVideoFormat      = VIDEO_FORMAT_LINEAR;
    pstViConfig->astViInfo[0].stChnInfo.enCompressMode     = COMPRESS_MODE_SEG;//COMPRESS_MODE_SEG;
    s32Ret = SAMPLE_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %#x!\n", s32Ret);
        LOGD("[%s] SAMPLE_COMM_VI_SetParam failed with %d! \n", log_Time(), s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam failed with %#x!\n", s32Ret);
        LOGD("[%s] HI_MPI_ISP_GetCtrlParam failed with %d! \n", log_Time(), s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl  = u32FrameRate/30;

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam failed with %#x!\n", s32Ret);
        LOGD("[%s] HI_MPI_ISP_SetCtrlParam failed with %d! \n", log_Time(), s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %#x!\n", s32Ret);
        LOGD("[%s] SAMPLE_COMM_VI_StartVi failed with %d! \n", log_Time(), s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,PIXEL_FORMAT_E enPixelFormat,SIZE_S stSize[],SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 i;
    HI_S32 s32Ret;
    PIC_SIZE_E      enSnsSize;
    SIZE_S          stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        LOGD("[%s] SAMPLE_COMM_VI_GetSizeBySensor! \n", log_Time());
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        LOGD("[%s] SAMPLE_COMM_SYS_GetPicSize failed! \n", log_Time());
        return s32Ret;
    }

    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat  = enPixelFormat;
    stVpssGrpAttr.u32MaxW        = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH        = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_TRUE;
    stVpssGrpAttr.stNrAttr.enNrType = VPSS_NR_TYPE_VIDEO;
    stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;
    stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;

    for(i=0; i<VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if(HI_TRUE == pabChnEnable[i])
        {
            stVpssChnAttr[i].u32Width                     = stSize[i].u32Width;
            stVpssChnAttr[i].u32Height                    = stSize[i].u32Height;
            stVpssChnAttr[i].enChnMode                    = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode               = COMPRESS_MODE_NONE;//COMPRESS_MODE_SEG;
            stVpssChnAttr[i].enDynamicRange               = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat                = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate  = -1;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate  = -1;
            stVpssChnAttr[i].u32Depth                     = 0;
            stVpssChnAttr[i].bMirror                      = HI_FALSE;
            stVpssChnAttr[i].bFlip                        = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat                = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode         = ASPECT_RATIO_NONE;//ASPECT_RATIO_AUTO;//ASPECT_RATIO_NONE;
        }
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, pabChnEnable,&stVpssGrpAttr,stVpssChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
        LOGD("[%s] start VPSS fail for %#x! \n", log_Time(), s32Ret);
    }

    return s32Ret;
}

extern SAMPLE_VI_CONFIG_S   _stViConfig;
extern SIZE_S               _stSize[2];
extern PIC_SIZE_E           _enSize[2];


HI_S32 LOTO_VENC_DisplayCover()
{
    HI_S32              s32Ret;
    RGN_HANDLE          Handle  = 11;
    MPP_CHN_S           stMppChnAttr;
    RGN_ATTR_S          stRgnAttr;
    RGN_CHN_ATTR_S      stRgnChnAttr;


    PIC_SIZE_E      enSnsSize;
    SIZE_S          stSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(_stViConfig.astViInfo[0].stSnsInfo.enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    stRgnAttr.enType            = COVEREX_RGN;

    s32Ret = HI_MPI_RGN_Create(Handle, &stRgnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_Create fail for %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    stMppChnAttr.enModId        = HI_ID_VPSS;
    stMppChnAttr.s32DevId       = 0;
    stMppChnAttr.s32ChnId       = 0;

    stRgnChnAttr.bShow                                  = HI_TRUE;
    stRgnChnAttr.enType                                 = COVEREX_RGN;
    stRgnChnAttr.unChnAttr.stCoverChn.enCoverType       = AREA_RECT;
    stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32X       = 0;
    stRgnChnAttr.unChnAttr.stCoverChn.stRect.s32Y       = 0;
    stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Width   = stSnsSize.u32Width;
    stRgnChnAttr.unChnAttr.stCoverChn.stRect.u32Height  = stSnsSize.u32Height;
    stRgnChnAttr.unChnAttr.stCoverChn.u32Color          = 0x0;
    stRgnChnAttr.unChnAttr.stCoverChn.u32Layer          = 0;
    stRgnChnAttr.unChnAttr.stCoverChn.enCoordinate      = RGN_ABS_COOR;

    s32Ret = HI_MPI_RGN_AttachToChn(Handle, &stMppChnAttr, &stRgnChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_AttachToChn fail for %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    SAMPLE_PRT("HI_MPI_RGN_AttachToChn success!\n");

    s32Ret = HI_MPI_RGN_SetDisplayAttr(Handle, &stMppChnAttr, &stRgnChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_SetDisplayAttr fail for %#x!\n");
        return HI_FAILURE;
    }
    SAMPLE_PRT("To be Dark!\n");

    return HI_SUCCESS;
}

HI_S32 LOTO_VENC_RemoveCover()
{
    HI_S32              s32Ret;
    RGN_HANDLE          Handle  = 11;
    MPP_CHN_S           stMppChnAttr;

    stMppChnAttr.enModId        = HI_ID_VPSS;
    stMppChnAttr.s32DevId       = 0;
    stMppChnAttr.s32ChnId       = 0;

    s32Ret = HI_MPI_RGN_DetachFromChn(Handle, &stMppChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_DetachFromChn fail for %#x\n", s32Ret);
        LOGD("[%s] HI_MPI_RGN_DetachFromChn fail for %#x! \n", log_Time(), s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_RGN_Destroy(Handle);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_RGN_Destroy fail for %#x\n", s32Ret);
        LOGD("[%s] HI_MPI_RGN_Destroy fail for %#x! \n", log_Time(), s32Ret);
        return HI_FAILURE;
    }
    SAMPLE_PRT("Light!\n");

    return HI_SUCCESS;
}

void* LOTO_VENC_CLASSIC(void *p)
{
    HI_S32          s32Ret;
    VI_PIPE         ViPipe       = 0;
    VI_CHN          ViChn        = 0;
    VPSS_GRP        VpssGrp        = 0;
    VPSS_CHN        VpssChn[2]     = {0,1};
    HI_BOOL         abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {HI_TRUE,HI_FALSE,HI_FALSE};

    HI_U32          u32Profile[2] = {0,2};
    PAYLOAD_TYPE_E  enPayLoad[2]  = {PT_H264, PT_H264};
    VENC_GOP_MODE_E enGopMode;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E     enRcMode;
    HI_BOOL         bRcnRefShareBuf = HI_TRUE;
    HI_S32          s32ChnNum     = 1;
    VENC_CHN        VencChn[2]    = {0,1};
    pthread_t       venc_Pid;
    VPSS_LOW_DELAY_INFO_S  stLowDelayInfo;
    

    s32Ret = LOTO_VENC_VPSS_Init(VpssGrp,abChnEnable,DYNAMIC_RANGE_SDR8,PIXEL_FORMAT_YVU_SEMIPLANAR_420, _stSize, _stViConfig.astViInfo[0].stSnsInfo.enSnsType);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Init VPSS err for %#x!\n", s32Ret);
        LOGD("[%s] Init VPSS err for %#x! \n", log_Time(), s32Ret);
        goto EXIT_VI_STOP;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VI Bind VPSS err for %#x!\n", s32Ret);
        LOGD("[%s] VI Bind VPSS err for %#x! \n", log_Time(), s32Ret);
        goto EXIT_VPSS_STOP;
    }

    // stLowDelayInfo.bEnable = HI_TRUE;
    // stLowDelayInfo.u32LineCnt = 128;//s_stSize[1].u32Height/2;
    // s32Ret = HI_MPI_VPSS_SetLowDelayAttr(VpssGrp,VpssChn[0],&stLowDelayInfo);
    // if(s32Ret != HI_SUCCESS)
    // {
    //     SAMPLE_PRT("Set LowDelay Attr for %#x!\n", s32Ret);
    //     goto EXIT_VPSS_STOP;
    // }

   /******************************************
    start stream venc
    ******************************************/

    enRcMode = SAMPLE_RC_CBR;//SAMPLE_VENC_GetRcMode();

    enGopMode = VENC_GOPMODE_NORMALP;//SAMPLE_VENC_GetGopMode();
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(enGopMode,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr for %#x!\n", s32Ret);
        LOGD("[%s] Venc Get GopAttr for %#x! \n", log_Time(), s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

   /***encode h.264 **/
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn[0], enPayLoad[1], _enSize[1], enRcMode, u32Profile[1] ,bRcnRefShareBuf, &stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
        LOGD("[%s] Venc Start failed for %#x! \n", log_Time(), s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn[0], VencChn[0]);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr failed for %#x!\n", s32Ret);
        LOGD("[%s] Venc Get GopAttr failed for %#x! \n", log_Time(), s32Ret);
        goto EXIT_VENC_H264_STOP;
    }

    /******************************************
     stream save process
    ******************************************/
	s32ChnNum     = 1;
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(&venc_Pid, VencChn,s32ChnNum);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        LOGD("[%s]Start Venc failed! \n", log_Time());
        goto EXIT_VENC_H264_UnBind;
    }

    pthread_join(venc_Pid, 0);

/******************************************
 exit process
******************************************/
    SAMPLE_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn[0],VencChn[0]);
EXIT_VENC_H264_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn[0]);
EXIT_VI_VPSS_UNBIND:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe,ViChn,VpssGrp);
EXIT_VPSS_STOP:
    SAMPLE_COMM_VPSS_Stop(VpssGrp,abChnEnable);
EXIT_VI_STOP:
    SAMPLE_COMM_VI_StopVi(&_stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return NULL;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */