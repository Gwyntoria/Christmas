/*
 * Loto_venc.h:
 *
 ***********************************************************************
 * by Jessica Mao
 * Lotogram Inc,. 2023/02/28
 *
 ***********************************************************************
 */

#ifndef LOTO_VENC_H
#define LOTO_VENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sample_comm.h"

void* LOTO_VENC_CLASSIC(void *p);
HI_S32 LOTO_VENC_CheckSensor(SAMPLE_SNS_TYPE_E   enSnsType,SIZE_S  stSize);
HI_S32 LOTO_VENC_ModifyResolution(SAMPLE_SNS_TYPE_E   enSnsType,PIC_SIZE_E *penSize,SIZE_S *pstSize);
HI_S32 LOTO_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig);


#ifdef __cplusplus
}
#endif

#endif