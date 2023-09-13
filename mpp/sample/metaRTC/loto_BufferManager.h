/*
 * loto_BufferManager.h:
 *
 ***********************************************************************
 * by Jessica Mao
 * Lotogram Inc,. 2023/02/28
 *
 ***********************************************************************
 */

#ifndef BUFFERMANAGER_H
#define BUFFERMANAGER_H

#include "sample_comm.h"

#ifdef __cplusplus
extern "C" {
#endif


HI_S32 HisiPutH264DataToBuffer(VENC_STREAM_S *pstStream);
HI_S32 HisiPutAACDataToBuffer(AUDIO_STREAM_S *aacStream);
HI_S32 HisiPutOpusDataToBuffer(HI_U8 *opus_data, HI_U32 data_size, HI_U64 timestamp);


#ifdef __cplusplus
}
#endif

#endif