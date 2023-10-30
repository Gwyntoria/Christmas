#include "loto_aac.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "aacenc_lib.h" 

#include "acodec.h"
#include "hi_comm_aenc.h"
#include "loto_comm.h"
#include "common.h"
#include "hi_type.h"
#include "hi_comm_aenc.h"
#include "ringfifo.h"

#define AAC_ENC_SAMPLERATE 44100
#define AAC_ENC_FRAME_SIZE 1024
#define AAC_ENC_CHANNAL_COUNT 2
#define AAC_ENC_BITRATE 128000
#define AAC_MAX_DATA_SIZE 1024

#define AAC_AI_DEV 0
#define AAC_AI_CHN 0

typedef struct AAC_THREAD_ARG {
    HANDLE_AACENCODER aacEncoder;
    AUDIO_DEV AiDevId;
    AI_CHN AiChn;
    HI_BOOL start;
} AAC_THREAD_ARG;

HI_S32 LOTO_AAC_StartAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt,
                         AIO_ATTR_S *pstAioAttr, AUDIO_SAMPLE_RATE_E enOutSampleRate,
                         HI_BOOL bResampleEn, HI_VOID *pstAiVqeAttr, HI_U32 u32AiVqeType) {
    HI_S32 i;
    HI_S32 ret;

    /* Sets attributes of an AI device */
    ret = HI_MPI_AI_SetPubAttr(AiDevId, pstAioAttr);
    if (ret) {
        LOGE("HI_MPI_AI_SetPubAttr(%d) failed with %#x\n", AiDevId, ret);
        return ret;
    }

    /* Enables an AI device */
    ret = HI_MPI_AI_Enable(AiDevId);
    if (ret) {
        LOGE("HI_MPI_AI_Enable(%d) failed with %#x\n", AiDevId, ret);
        return ret;
    }

    for (i = 0; i < s32AiChnCnt >> pstAioAttr->enSoundmode; i++) {
        /* Enables an AI channel */
        ret = HI_MPI_AI_EnableChn(AiDevId, i);
        if (ret) {
            LOGE("HI_MPI_AI_EnableChn(%d,%d) failed with %#x\n", AiDevId, i, ret);
            return ret;
        }

        if (HI_TRUE == bResampleEn) {
            /* Enables AI resampling */
            ret = HI_MPI_AI_EnableReSmp(AiDevId, i, enOutSampleRate);
            if (ret) {
                LOGE("HI_MPI_AI_EnableReSmp(%d,%d) failed with %#x\n", AiDevId, i, ret);
                return ret;
            }
        }

        if (NULL != pstAiVqeAttr) {
            HI_BOOL bAiVqe = HI_TRUE;
            switch (u32AiVqeType) {
            case 0:
                ret = HI_SUCCESS;
                bAiVqe = HI_FALSE;
                break;
            case 1:
                /* Sets the AI VQE (record) attributes */
                ret = HI_MPI_AI_SetRecordVqeAttr(AiDevId, i, (AI_RECORDVQE_CONFIG_S *)pstAiVqeAttr);
                break;
            default:
                ret = HI_FAILURE;
                break;
            }

            if (ret) {
                LOGE("%s: SetAiVqe%d(%d,%d) failed with %#x\n", __FUNCTION__, u32AiVqeType, AiDevId, i, ret);
                return ret;
            }

            if (bAiVqe) {
                /* Enables the VQE functions of an AI channel */
                ret = HI_MPI_AI_EnableVqe(AiDevId, i);
                if (ret) {
                    LOGE("HI_MPI_AI_EnableVqe(%d,%d) failed with %#x\n", AiDevId, i, ret);
                    return ret;
                }
            }
        }
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_AAC_StopAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt,
                        HI_BOOL bResampleEn, HI_BOOL bVqeEn) {
    HI_S32 i;
    HI_S32 ret;

    for (i = 0; i < s32AiChnCnt; i++) {
        if (HI_TRUE == bResampleEn) {
            ret = HI_MPI_AI_DisableReSmp(AiDevId, i);
            if (HI_SUCCESS != ret) {
                printf("[Func]:%s [Line]:%d [Info]:%s\n", __FUNCTION__, __LINE__, "failed");
                return ret;
            }
        }

        if (HI_TRUE == bVqeEn) {
            ret = HI_MPI_AI_DisableVqe(AiDevId, i);
            if (HI_SUCCESS != ret) {
                printf("[Func]:%s [Line]:%d [Info]:%s\n", __FUNCTION__, __LINE__, "failed");
                return ret;
            }
        }

        ret = HI_MPI_AI_DisableChn(AiDevId, i);
        if (HI_SUCCESS != ret) {
            printf("[Func]:%s [Line]:%d [Info]:%s\n", __FUNCTION__, __LINE__, "failed");
            return ret;
        }
    }

    ret = HI_MPI_AI_Disable(AiDevId);
    if (HI_SUCCESS != ret) {
        printf("[Func]:%s [Line]:%d [Info]:%s\n", __FUNCTION__, __LINE__, "failed");
        return ret;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_AAC_InitAacEncoder(HANDLE_AACENCODER *aacEncoder) {
    LOGI("=== LOTO_AAC_InitAacEncoder ===\n");

    // 配置编码参数
    if (aacEncOpen(aacEncoder, 0x01, AAC_ENC_CHANNAL_COUNT) != AACENC_OK) {
        LOGE("Failed to open encoder.\n");
        return -1;
    }

    if (aacEncoder_SetParam(*aacEncoder, AACENC_AOT, 2) != AACENC_OK) {  // LC-AAC
        LOGE("Failed to set AOT.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    if (aacEncoder_SetParam(*aacEncoder, AACENC_SAMPLERATE, AAC_ENC_SAMPLERATE) != AACENC_OK) {
        LOGE("Failed to set sample rate.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    CHANNEL_MODE channelMode;

    if (AAC_ENC_CHANNAL_COUNT == 1){
        channelMode = MODE_1;
    } else if (AAC_ENC_CHANNAL_COUNT == 2) {
        channelMode = MODE_2;
    }

    if (aacEncoder_SetParam(*aacEncoder, AACENC_CHANNELMODE, channelMode) != AACENC_OK) {
        LOGE("Failed to set channel mode.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    if (aacEncoder_SetParam(*aacEncoder, AACENC_BITRATE, AAC_ENC_BITRATE) != AACENC_OK) {
        LOGE("Failed to set bitrate.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    if (aacEncoder_SetParam(*aacEncoder, AACENC_TRANSMUX, TT_MP4_ADTS) != AACENC_OK) {  // ADTS
        LOGE("Failed to set transmux.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    if (aacEncEncode(*aacEncoder, NULL, NULL, NULL, NULL) != AACENC_OK) {
        LOGE("Failed to initialize encoder.\n");
        aacEncClose(aacEncoder);
        return -1;
    }

    AACENC_InfoStruct encInfo = {0};
    if (aacEncInfo(*aacEncoder, &encInfo) != AACENC_OK) {
        LOGE("Failed to get encoder info\n");
        aacEncClose(aacEncoder);
        return -1;
    }
}

HI_S32 LOTO_AAC_CloseEncoder(HANDLE_AACENCODER *aacEncoder) {
    if (aacEncClose(aacEncoder) != AACENC_OK) {
        LOGE("Failed to close encoder\n");
        return -1;
    }
}


void *LOTO_AAC_StartEncode(void *arg) {
    LOGI("=== LOTO_AAC_StartEncode ===\n");

    HI_S32 ret;
    AAC_THREAD_ARG *aacArg = (AAC_THREAD_ARG *)arg;
    HANDLE_AACENCODER aacEncoder = aacArg->aacEncoder;
    AUDIO_DEV AiDevId = aacArg->AiDevId;
    AI_CHN AiChn = aacArg->AiChn;
    HI_BOOL start = aacArg->start;
    free(aacArg);

    AUDIO_FRAME_S pstFrm;
    AEC_FRAME_S pstAecFrm;

    HI_U32 point_num = 0;
    HI_U32 water_line = 0;
    HI_U32 frame_size = AAC_ENC_FRAME_SIZE;
    HI_U32 channel_num = AAC_ENC_CHANNAL_COUNT;
    HI_U16 inputBuffer[AAC_ENC_FRAME_SIZE * AAC_ENC_CHANNAL_COUNT] = {0};
    HI_U8 outputBuffer[AAC_MAX_DATA_SIZE] = {0};
    HI_U8 outputBufferTem[AAC_MAX_DATA_SIZE] = {0};

    HI_S32 ai_fd;
    fd_set read_fds;
    struct timeval timeoutVal;

    FD_ZERO(&read_fds);
    ai_fd = HI_MPI_AI_GetFd(AiDevId, AiChn);
    FD_SET(ai_fd, &read_fds);

    LOGD("AiDevId = %d, AiChn = %d\n", AiDevId, AiChn);

    void* inBuffer[] = {inputBuffer};
    INT inBufferIds[] = {IN_AUDIO_DATA};
    INT inBufferSize[] = {frame_size};
    INT inBufferElSize[] = {sizeof(HI_U16)};

    void* outBuffer[] = {outputBuffer};
    INT outBufferIds[]= {OUT_BITSTREAM_DATA};
    INT outBufferSize[] = {sizeof(outputBuffer)};
    INT outBufferElSize[] = {sizeof(HI_U8)};

    AACENC_BufDesc inBufDesc;
    AACENC_BufDesc outBufDesc;

    inBufDesc.numBufs = 1;
    inBufDesc.bufs = (void**)&inBuffer;
    inBufDesc.bufferIdentifiers = inBufferIds;
    inBufDesc.bufSizes = inBufferSize;
    inBufDesc.bufElSizes = inBufferElSize;

    outBufDesc.numBufs = 1;
    outBufDesc.bufs = (void**)&outBuffer;
    outBufDesc.bufferIdentifiers = outBufferIds;
    outBufDesc.bufSizes = outBufferSize;
    outBufDesc.bufElSizes = outBufferElSize;

    AACENC_InArgs inargs = {0};
    AACENC_OutArgs outargs = {0};

    inargs.numInSamples = frame_size;

    HI_U64 time_pre = 0;
    HI_S32 aac_data_len = 0;

    while (start) {

        timeoutVal.tv_sec = 1;
        timeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(ai_fd, &read_fds);

        ret = select(ai_fd + 1, &read_fds, NULL, NULL, &timeoutVal);
        if (ret < 0) {
            break;
        } else if (0 == ret) {
            LOGE("Get ai stream select time out\n");
            break;
        }

        if (FD_ISSET(ai_fd, &read_fds)) {
            /* Get audio stream from AI */
            ret = HI_MPI_AI_GetFrame(AiDevId, AiChn, &pstFrm, &pstAecFrm, 0);
            if (ret != HI_SUCCESS) {
                LOGE("HI_MPI_AI_GetFrame failed with %#x\n", ret);
                break;
            }

            /* period */
            if (time_pre == 0) {
                time_pre = pstFrm.u64TimeStamp;
            } else {
                LOGD("period = %llu\n", pstFrm.u64TimeStamp - time_pre);
                time_pre = pstFrm.u64TimeStamp;
            }

            do {
                /* AAC encode */
                point_num = pstFrm.u32Len / (pstFrm.enBitwidth + 1);
                water_line = AAC_ENC_FRAME_SIZE;

                LOGD("length = %d, bit_width = %d bytes\n", pstFrm.u32Len, pstFrm.enBitwidth + 1);

                if (point_num != water_line) {
                    LOGE("Invalid point number %d for default opus encoder setting!\n", point_num);
                    break;
                }
                
                if (channel_num == 2) {
                    for (int i = frame_size - 1; i >= 0; i--) {
                        inputBuffer[2 * i] = *((HI_U16 *)pstFrm.u64VirAddr[0] + i);
                        inputBuffer[2 * i + 1] = *((HI_U16 *)pstFrm.u64VirAddr[1] + i);
                    }
                    // HexToString(inputBuffer, pstFrm.u32Len * 2);
                } else {
                    HI_S16 *tem = (HI_U16 *)pstFrm.u64VirAddr[0];
                    for (int i = frame_size - 1; i >= 0; i--) {
                        inputBuffer[i] = *(tem + i);
                    }
                    // HexToString(inputBuffer, pstFrm.u32Len);
                }

                /* void* inBuffer[] = {inputBuffer};
                INT inBufferIds[] = {IN_AUDIO_DATA};
                INT inBufferSize[] = {frame_size};
                INT inBufferElSize[] = {sizeof(HI_S16)};

                void* outBuffer[] = {outputBuffer};
                INT outBufferIds[]= {OUT_BITSTREAM_DATA};
                INT outBufferSize[] = {sizeof(outputBuffer)};
                INT outBufferElSize[] = {sizeof(HI_U8)};

                AACENC_BufDesc inBufDesc;
                AACENC_BufDesc outBufDesc;

                inBufDesc.numBufs = 1;
                inBufDesc.bufs = (void**)&inBuffer;
                inBufDesc.bufferIdentifiers = inBufferIds;
                inBufDesc.bufSizes = inBufferSize;
                inBufDesc.bufElSizes = inBufferElSize;

                outBufDesc.numBufs = 1;
                outBufDesc.bufs = (void**)&outBuffer;
                outBufDesc.bufferIdentifiers = outBufferIds;
                outBufDesc.bufSizes = outBufferSize;
                outBufDesc.bufElSizes = outBufferElSize;

                AACENC_InArgs inargs = {0};
                inargs.numInSamples = frame_size;
                AACENC_OutArgs outargs = {0}; */

                ret = aacEncEncode(aacEncoder, &inBufDesc, &outBufDesc, &inargs, &outargs);
                if (ret != AACENC_OK) {
                    LOGE("Failed to encode AAC frame with %#x\n", ret);
                    break;
                } else {
                    if (outargs.numOutBytes > 0) {
                        aac_data_len = outargs.numOutBytes;
                        memcpy(outputBufferTem, outputBuffer, aac_data_len);
                    } else if (outargs.numOutBytes == 0) {
                        memcpy(outputBuffer, outputBufferTem, aac_data_len);
                        memset(outputBufferTem, 0, sizeof(outputBufferTem));
                    }
                }

                LOGD("aac_data_len = %d\n", aac_data_len);

                HexToString(outputBuffer, aac_data_len);

                ret = HisiPutOpusDataToBuffer(outputBuffer, aac_data_len, pstFrm.u64TimeStamp);
                if (ret != HI_SUCCESS) {
                    LOGE("HisiPutOpusDataToBuffer failed!\n");
                    break;
                }

            } while (0);

            /* Release audio frame */
            ret = HI_MPI_AI_ReleaseFrame(AiDevId, AiChn, &pstFrm, &pstAecFrm);
            if (ret != HI_SUCCESS)
            {
                LOGE("HI_MPI_AI_ReleaseFrame failed with %#x\n", ret);
                start = HI_FALSE;
                break;
            }

            memset(inputBuffer, 0, sizeof(inputBuffer));
            memset(outputBuffer, 0, sizeof(outputBuffer));
        }
    }

    start = HI_FALSE;
}

HI_S32 LOTO_AAC_CreateAacThread(pthread_t *aac_aenc_id, HANDLE_AACENCODER aacEncoder, 
                                AUDIO_DEV AiDevId, AI_CHN AiChn) {
    AAC_THREAD_ARG *aacArg = (AAC_THREAD_ARG *)malloc(sizeof(AAC_THREAD_ARG));
    aacArg->aacEncoder = aacEncoder;
    aacArg->AiDevId = AiDevId;
    aacArg->AiChn = AiChn;
    aacArg->start = HI_TRUE;

    if(pthread_create(aac_aenc_id, NULL, LOTO_AAC_StartEncode, (void *)aacArg) != 0) {
        LOGE("Failed to start aac enc thread\n");
        return -1;
    }
    return 0;
}

void *LOTO_AAC_AudioEncoder(void *arg) {
    LOGI("=== LOTO_AAC_AudioEncoder ===\n");

    HI_S32 ret;
    AUDIO_DEV AiDev = AAC_AI_DEV;
    AI_CHN AiChn = AAC_AI_CHN;
    HI_S32 s32AiChnCnt = -1;
    HANDLE_AACENCODER aacEncoder;

    pthread_t aac_aenc_id;

    AIO_ATTR_S stAioAttr;
    stAioAttr.enSamplerate = AUDIO_SAMPLE_RATE_44100;
    stAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode = AIO_MODE_PCM_MASTER_STD;
    if (AAC_ENC_CHANNAL_COUNT == 1) {
        stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
    } else if (AAC_ENC_CHANNAL_COUNT == 2) {
        stAioAttr.enSoundmode = AUDIO_SOUND_MODE_STEREO;
    }
    stAioAttr.u32EXFlag = 1;
    stAioAttr.u32FrmNum = 5;
    stAioAttr.u32PtNumPerFrm = AAC_ENC_FRAME_SIZE;
    stAioAttr.u32ChnCnt = AAC_ENC_CHANNAL_COUNT;
    stAioAttr.u32ClkSel = 0;
    stAioAttr.enI2sType = AIO_I2STYPE_INNERCODEC;

    s32AiChnCnt = stAioAttr.u32ChnCnt;

    LOGD("sound_mode = %d, sample_rate = %d\n", stAioAttr.enSoundmode + 1, stAioAttr.enSamplerate);

    do {
        /* Initialize AI */
        ret = LOTO_AAC_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
        if (ret != 0) {
            LOGE("LOTO_AAC_StartAi failed!\n");
            break;
        }

        do {
            ret = LOTO_AAC_InitAacEncoder(&aacEncoder);
            if (ret != 0) {
                LOGE("LOTO_AAC_InitAacEncoder failed\n");
                break;
            }

            /* Create a thread to get stream and encode */
            ret = LOTO_AAC_CreateAacThread(&aac_aenc_id, aacEncoder, AiDev, AiChn);
            if (ret != 0)
            {
                LOGE("LOTO_AAC_CreateAacThread failed!\n");
            }
            pthread_join(aac_aenc_id, 0);

            ret = LOTO_AAC_CloseEncoder(&aacEncoder);
            if (ret != 0)
            {
                LOGE("LOTO_AAC_CloseEncoder failed!\n");
            }

        } while (0);

        ret = LOTO_AAC_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
        if (ret != HI_SUCCESS) {
            LOGE("LOTO_OPUS_StopAi failed\n");
        }

    } while (0);
}