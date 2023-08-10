
#include "loto_opus.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "opus.h"
#include "opus_defines.h"
#include "opus_multistream.h"
#include "opus_projection.h"
#include "opus_types.h"
#include "opusenc.h"

#include "acodec.h"
#include "common.h"
#include "hi_comm_aenc.h"
#include "hi_type.h"
#include "loto_comm.h"
#include "ringfifo.h"

// #define OPUS_CODEC_FILE "/dev/opus"

#define LOTO_OPUS_ASSERT(x)                                                                                            \
    {                                                                                                                  \
        if (1 != (x))                                                                                                  \
            return -1;                                                                                                 \
    }

#define OPUS_LIB_NAME  "libopus.so"
#define ACODEC_FILE    "/dev/acodec"
#define OPUS_SAVE_DATA "audio_save.opus"

/* samples per frame for opus */
#define OPUS_ENC_SAMPLES_PER_FRAME 960
#define OPUS_DEC_SAMPLES_PER_FRAME 960

#define OPUS_MAX_DATA_SIZE 512

#define OPUS_ENCODE_BITRATE (8 * 2048)
#define OPUS_CHANNEL_NUM    2
#define OPUS_MONO           1
#define OPUS_STEREO         2
#define VBR_FALSE           0 /* Hard CBR */
#define VBR_TRUE            1 /* VBR */

#define OPUS_AI_DEV 0
#define OPUS_AI_CHN 0

/* opus encode lib */
typedef opus_int32 (*opusEncode_Callback)(OpusEncoder* st, const opus_int16* pcm, int frame_size, unsigned char* data,
                                          opus_int32 max_data_bytes);
typedef OpusEncoder* (*opusEncoderCreate_Callback)(opus_int32 Fs, int channels, int application, int* ret);
typedef int (*opusEncoderCtl_Callback)(OpusEncoder* st, int request, ...);
typedef void (*opusEncoderDestory_Callback)(OpusEncoder* st);

typedef struct OPUS_ENCODE_FUNC_S {
    void*                       libHandle;
    opusEncode_Callback         opusEncode;
    opusEncoderCreate_Callback  opusEncoderCreate;
    opusEncoderCtl_Callback     opusEncoderCtl;
    opusEncoderDestory_Callback opusEncoderDestory;
} OPUS_ENCODE_FUNC_S;

static OPUS_ENCODE_FUNC_S g_stOpusEncFunc = {0};

int opus_dl_open(void** libHandle, char* libName) {
    if (NULL == libHandle || NULL == libName) {
        return -1;
    }

    *libHandle = NULL;
    *libHandle = dlopen(libName, RTLD_LAZY | RTLD_LOCAL);

    if (NULL == *libHandle) {
        LOGE("dlopen %s failed, ret msg is %s! \n", libName, dlerror());
        return -1;
    }

    return 0;
}

int opus_dl_dlsym(void** funcHandle, void* libHandle, char* funcName) {
    if (NULL == libHandle) {
        LOGE("libHandle is empty!\n");
        return -1;
    }

    *funcHandle = NULL;
    *funcHandle = dlsym(libHandle, funcName);

    if (NULL == *funcHandle) {
        LOGE("dlsym %s failed, ret msg is %s! \n", funcName, dlerror());
        return -1;
    }

    return 0;
}

int opus_dl_close(void* libHandle) {
    if (NULL == libHandle) {
        LOGE("libHandle is empty!\n");
        return -1;
    }

    if (dlclose(libHandle)) {
        LOGE("dlclose lib failed, ret msg is %s! \n", dlerror());
        return -1;
    }

    return 0;
}

static int init_opus_lib(void) {
    LOGI("=== Start initializing opus lib! ===\n");

    int ret;

    if (!g_stOpusEncFunc.libHandle) {
        ret = opus_dl_open(&(g_stOpusEncFunc.libHandle), OPUS_LIB_NAME);
        if (0 != ret) {
            LOGE("Load opus dynamic lib fail!\n");
            return -1;
        }

        ret = opus_dl_dlsym((void**)&(g_stOpusEncFunc.opusEncode), g_stOpusEncFunc.libHandle, "opus_encode");
        if (0 != ret) {
            LOGE("Find symbol opus_encode fail!\n");
            return -1;
        }

        ret = opus_dl_dlsym((void**)&(g_stOpusEncFunc.opusEncoderCreate), g_stOpusEncFunc.libHandle,
                            "opus_encoder_create");
        if (0 != ret) {
            LOGE("Find symbol opus_encoder_create fail!\n");
            return -1;
        }

        ret = opus_dl_dlsym((void**)&(g_stOpusEncFunc.opusEncoderCtl), g_stOpusEncFunc.libHandle, "opus_encoder_ctl");
        if (0 != ret) {
            LOGE("Find symbol opus_encoder_ctl fail!\n");
            return -1;
        }

        ret = opus_dl_dlsym((void**)&(g_stOpusEncFunc.opusEncoderDestory), g_stOpusEncFunc.libHandle,
                            "opus_encoder_destroy");
        if (0 != ret) {
            LOGE("Find symbol opus_encoder_destroy fail!\n");
            return -1;
        }
    }

    LOGI("=== init_opus_lib OK! ===\n");

    return 0;
}

void deinit_opus_lib() {
    if (NULL != g_stOpusEncFunc.libHandle) {
        opus_dl_close(g_stOpusEncFunc.libHandle);
        g_stOpusEncFunc.libHandle = NULL;
    }
    memset(&g_stOpusEncFunc, 0, sizeof(OPUS_ENCODE_FUNC_S));

    LOGE("=== deinit_opus_lib OK! ===\n");
}

HI_S32 LOTO_OPUS_StartAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt, AIO_ATTR_S* pstAioAttr,
                         AUDIO_SAMPLE_RATE_E enOutSampleRate, HI_BOOL bResampleEn, HI_VOID* pstAiVqeAttr,
                         HI_U32 u32AiVqeType) {
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

    for (i = 0; i<s32AiChnCnt> > pstAioAttr->enSoundmode; i++) {
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
                ret    = HI_SUCCESS;
                bAiVqe = HI_FALSE;
                break;
            case 1:
                /* Sets the AI VQE (record) attributes */
                ret = HI_MPI_AI_SetRecordVqeAttr(AiDevId, i, (AI_RECORDVQE_CONFIG_S*)pstAiVqeAttr);
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

HI_S32 LOTO_OPUS_StopAi(AUDIO_DEV AiDevId, HI_S32 s32AiChnCnt, HI_BOOL bResampleEn, HI_BOOL bVqeEn) {
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

HI_S32 LOTO_OPUS_ConfigAudioIn(AUDIO_SAMPLE_RATE_E enSample) {
    HI_S32         fdAcodec        = -1;
    HI_S32         ret             = HI_SUCCESS;
    ACODEC_FS_E    i2s_fs_sel      = 0;
    int            iAcodecInputVol = 0;
    ACODEC_MIXER_E input_mode      = 0;

    fdAcodec = open(ACODEC_FILE, O_RDWR);
    if (fdAcodec < 0) {
        LOGE("Can't open Acodec,%s\n", ACODEC_FILE);
        return HI_FAILURE;
    }
    if (ioctl(fdAcodec, ACODEC_SOFT_RESET_CTRL)) {
        LOGE("Reset audio codec error\n");
    }

    switch (enSample) {
    case AUDIO_SAMPLE_RATE_8000:
        i2s_fs_sel = ACODEC_FS_8000;
        break;

    case AUDIO_SAMPLE_RATE_16000:
        i2s_fs_sel = ACODEC_FS_16000;
        break;

    case AUDIO_SAMPLE_RATE_32000:
        i2s_fs_sel = ACODEC_FS_32000;
        break;

    case AUDIO_SAMPLE_RATE_11025:
        i2s_fs_sel = ACODEC_FS_11025;
        break;

    case AUDIO_SAMPLE_RATE_22050:
        i2s_fs_sel = ACODEC_FS_22050;
        break;

    case AUDIO_SAMPLE_RATE_44100:
        i2s_fs_sel = ACODEC_FS_44100;
        break;

    case AUDIO_SAMPLE_RATE_12000:
        i2s_fs_sel = ACODEC_FS_12000;
        break;

    case AUDIO_SAMPLE_RATE_24000:
        i2s_fs_sel = ACODEC_FS_24000;
        break;

    case AUDIO_SAMPLE_RATE_48000:
        i2s_fs_sel = ACODEC_FS_48000;
        break;

    case AUDIO_SAMPLE_RATE_64000:
        i2s_fs_sel = ACODEC_FS_64000;
        break;

    case AUDIO_SAMPLE_RATE_96000:
        i2s_fs_sel = ACODEC_FS_96000;
        break;

    default:
        LOGE("Not support enSample:%d\n", enSample);
        ret = HI_FAILURE;
        break;
    }

    if (ioctl(fdAcodec, ACODEC_SET_I2S1_FS, &i2s_fs_sel)) {
        LOGE("Set acodec sample rate failed\n");
        ret = HI_FAILURE;
    }

    input_mode = ACODEC_MIXER_IN1;
    if (ioctl(fdAcodec, ACODEC_SET_MIXER_MIC, &input_mode)) {
        LOGE("Select acodec input_mode failed\n");
        ret = HI_FAILURE;
    }
    if (1) /* should be 1 when micin */
    {
        /******************************************************************************************
        The input volume range is [-87, +86]. Both the analog gain and digital gain are adjusted.
        A larger value indicates higher volume.
        For example, the value 86 indicates the maximum volume of 86 dB,
        and the value -87 indicates the minimum volume (muted status).
        The volume adjustment takes effect simultaneously in the audio-left and audio-right channels.
        The recommended volume range is [+10, +56].
        Within this range, the noises are lowest because only the analog gain is adjusted,
        and the voice quality can be guaranteed.
        *******************************************************************************************/
        iAcodecInputVol = 50;
        if (ioctl(fdAcodec, ACODEC_SET_INPUT_VOL, &iAcodecInputVol)) {
            LOGE("Set acodec micin volume failed\n");
            return HI_FAILURE;
        }
    }

    close(fdAcodec);
    return ret;
}

HI_S32 LOTO_OPUS_InitEncoder(OpusEncoder** opusEncoder) {
    int ret = init_opus_lib();
    if (ret != 0) {
        LOGE("init_opus_lib failed!\n");
        return -1;
    }

    if (g_stOpusEncFunc.opusEncoderCreate == NULL || g_stOpusEncFunc.opusEncoderCtl == NULL) {
        LOGE("Call opus function failed!\n");
        return HI_ERR_AENC_NOT_SUPPORT;
    }

    OPUS_ENCODER_ATTR_S stOpusAttr = {0};
    stOpusAttr.samplingRate        = OPUS_SAMPLING_RATE_48000;
    stOpusAttr.channels            = OPUS_CHANNEL_NUM;
    stOpusAttr.application         = OPUS_APPLICATION_AUDIO;
    // stOpusAttr.bitrate = OPUS_ENCODE_BITRATE;
    stOpusAttr.bitrate      = OPUS_AUTO;
    stOpusAttr.framesize    = OPUS_FRAMESIZE_20_MS;
    stOpusAttr.vbr          = VBR_FALSE;
    stOpusAttr.forceChannel = OPUS_STEREO;

    *opusEncoder
        = g_stOpusEncFunc.opusEncoderCreate(stOpusAttr.samplingRate, stOpusAttr.channels, stOpusAttr.application, &ret);
    if (ret != OPUS_OK || *opusEncoder == NULL) {
        LOGE("Allocates and initializes an encoder state failed with %#x\n", ret);
        return -1;
    }

    ret = g_stOpusEncFunc.opusEncoderCtl(*opusEncoder, OPUS_SET_BITRATE(stOpusAttr.bitrate));
    if (ret != OPUS_OK) {
        LOGE("OPUS_SET_BITRATE failed with %#x\n", ret);
        return -1;
    }

    ret = g_stOpusEncFunc.opusEncoderCtl(*opusEncoder, OPUS_SET_EXPERT_FRAME_DURATION(stOpusAttr.framesize));
    if (ret != OPUS_OK) {
        LOGE("OPUS_SET_EXPERT_FRAME_DURATION failed with %#x\n", ret);
        return -1;
    }

    ret = g_stOpusEncFunc.opusEncoderCtl(*opusEncoder, OPUS_SET_VBR(stOpusAttr.vbr));
    if (ret != OPUS_OK) {
        LOGE("OPUS_SET_VBR failed with %#x\n", ret);
        return -1;
    }

    ret = g_stOpusEncFunc.opusEncoderCtl(*opusEncoder, OPUS_SET_FORCE_CHANNELS(stOpusAttr.forceChannel));
    if (ret != OPUS_OK) {
        LOGE("OPUS_SET_FORCE_CHANNELS failed with %#x\n", ret);
        return -1;
    }

    return 0;
}

HI_S32 LOTO_OPUS_DestroyEncoder(OpusEncoder** opusEncoder) {
    if (g_stOpusEncFunc.opusEncoderDestory == NULL) {
        LOGE("call opus function failed!\n");
        return HI_ERR_AENC_NOT_SUPPORT;
    }

    if (*opusEncoder) {
        g_stOpusEncFunc.opusEncoderDestory(*opusEncoder);
    } else {
        LOGE("Destroy Opus encoder failed, OpusEncoder is NULL!\n");
        return -1;
    }

    LOGI("=== Destroy opus encoder! ===\n");

    deinit_opus_lib();

    return 0;
}

void* LOTO_OPUS_StartAenc(void* arg) {
    LOGI("=== LOTO_OPUS_StartAenc ===\n");

    if (g_stOpusEncFunc.opusEncode == NULL) {
        LOGE("call opus function failed!\n");
        return HI_ERR_AENC_NOT_SUPPORT;
    }

    HI_S32           ret     = 0;
    OPUS_THREAD_ARG* opusArg = (OPUS_THREAD_ARG*)arg;

    OpusEncoder* opusEncoder = opusArg->opusEncoder;
    AUDIO_DEV    AiDevId     = opusArg->AiDevId;
    AI_CHN       AiChn       = opusArg->AiChn;
    HI_BOOL      start       = opusArg->start;
    free(opusArg);

    AUDIO_FRAME_S pstFrm    = {0};
    AEC_FRAME_S   pstAecFrm = {0};

    HI_U32     point_num                                           = 0;
    HI_U32     water_line                                          = 0;
    HI_U32     frame_size                                          = OPUS_ENC_SAMPLES_PER_FRAME;
    HI_U32     channel_num                                         = OPUS_CHANNEL_NUM;
    opus_int16 data[OPUS_ENC_SAMPLES_PER_FRAME * OPUS_CHANNEL_NUM] = {0};
    HI_U32     max_data_bytes                                      = OPUS_MAX_DATA_SIZE;
    HI_U8*     out_buf                                             = (uint8_t*)malloc(sizeof(uint8_t) * max_data_bytes);
    HI_S32     opus_data_len                                       = 0;

    HI_S32         ai_fd      = 0;
    fd_set         read_fds   = {0};
    struct timeval timeoutVal = {0};

    FD_ZERO(&read_fds);
    ai_fd = HI_MPI_AI_GetFd(AiDevId, AiChn);
    FD_SET(ai_fd, &read_fds);

    // FILE *audio_save = fopen(OPUS_SAVE_DATA, "wb");
    // if (audio_save == NULL)
    // {
    //     LOGE("Create file to save opus data failed!\n");
    // }

    LOGD("AiDevId = %d, AiChn = %d\n", AiDevId, AiChn);

    while (start) {
        timeoutVal.tv_sec  = 1;
        timeoutVal.tv_usec = 0;

        FD_ZERO(&read_fds);
        FD_SET(ai_fd, &read_fds);

        ret = select(ai_fd + 1, &read_fds, NULL, NULL, &timeoutVal);
        if (ret < 0) {
            start = HI_FALSE;
            break;
        } else if (0 == ret) {
            LOGE("Get aenc stream select time out\n");
            start = HI_FALSE;
            break;
        }

        if (FD_ISSET(ai_fd, &read_fds)) {
            /* Get audio stream from AI */
            ret = HI_MPI_AI_GetFrame(AiDevId, AiChn, &pstFrm, &pstAecFrm, 0);
            if (ret != HI_SUCCESS) {
                LOGE("HI_MPI_AI_GetFrame failed with %#x\n", ret);
                start = HI_FALSE;
                break;
            }

            do {
                /* Opus encode */
                point_num  = pstFrm.u32Len / (pstFrm.enBitwidth + 1);
                water_line = OPUS_ENC_SAMPLES_PER_FRAME;

                // LOGD("pstFrm.u32Len = %d, pstFrm.enBitwidth = %d, point_num = %d\n", pstFrm.u32Len,
                // pstFrm.enBitwidth, point_num);

                if (point_num != water_line) {
                    LOGE("Invalid point number %d for default opus encoder setting!\n", point_num);
                    start = HI_FALSE;
                    break;
                }

                if (channel_num == 2) {
                    for (int i = frame_size - 1; i >= 0; i--) {
                        data[2 * i]     = *((opus_int16*)pstFrm.u64VirAddr[0] + i);
                        data[2 * i + 1] = *((opus_int16*)pstFrm.u64VirAddr[1] + i);
                    }
                } else {
                    opus_int16* tem = (opus_int16*)pstFrm.u64VirAddr[0];
                    for (int i = frame_size - 1; i > 0; i--) {
                        data[i] = *(tem + i);
                    }
                }

                ret = g_stOpusEncFunc.opusEncode(opusEncoder, data, frame_size, out_buf, max_data_bytes);
                if (ret < 0) {
                    LOGE("Opus encode failed with %#x\n", ret);
                    start = HI_FALSE;
                    break;
                } else {
                    opus_data_len = ret;
                }

                ret = HisiPutOpusDataToBuffer(out_buf, opus_data_len, pstFrm.u64TimeStamp);
                if (ret != HI_SUCCESS) {
                    LOGE("HisiPutOpusDataToBuffer failed!\n");
                    start = HI_FALSE;
                    break;
                }

                // LOGD("opus_frame_size = %d\n", opus_data_len);

                // if (audio_save != NULL)
                // {
                //     ret = fwrite(out_buf, opus_data_len, 1, audio_save);
                //     // ret = fwrite(pstFrm.u64VirAddr[0], pstFrm.u32Len, 1, audio_save);
                //     if (ret < 0)
                //     {
                //         LOGE("Write opus audio data to file failed!\n");
                //     }
                // }

            } while (0);

            /* Release audio frame */
            ret = HI_MPI_AI_ReleaseFrame(AiDevId, AiChn, &pstFrm, &pstAecFrm);
            if (ret != HI_SUCCESS) {
                LOGE("HI_MPI_AI_ReleaseFrame failed with %#x\n", ret);
                start = HI_FALSE;
                break;
            }
        }
    }

    start = HI_FALSE;
    // fclose(audio_save);
    free(out_buf);
}

HI_S32 LOTO_OPUS_CreateOpusThread(pthread_t* opus_aenc_id, OpusEncoder* opusEncoder, AUDIO_DEV AiDevId, AI_CHN AiChn) {
    HI_S32           ret     = 0;
    OPUS_THREAD_ARG* opusArg = (OPUS_THREAD_ARG*)malloc(sizeof(OPUS_THREAD_ARG));
    opusArg->opusEncoder     = opusEncoder;
    opusArg->AiDevId         = AiDevId;
    opusArg->AiChn           = AiChn;
    opusArg->start           = HI_TRUE;

    ret = pthread_create(opus_aenc_id, NULL, LOTO_OPUS_StartAenc, (void*)opusArg);
    if (ret != 0)
        return -1;
    return 0;
}

void* LOTO_OPUS_AudioEncode(void* p) {
    LOGI("=== LOTO_OPUS_AudioEncode ===\n");

    HI_S32    ret         = 0;
    AUDIO_DEV AiDev       = OPUS_AI_DEV;
    AI_CHN    AiChn       = OPUS_AI_CHN;
    HI_S32    s32AiChnCnt = 1;

    pthread_t    opus_aenc_id = 0;
    OpusEncoder* opusEncoder  = NULL;

    AIO_ATTR_S stAioAttr     = {0};
    stAioAttr.enSamplerate   = AUDIO_SAMPLE_RATE_48000;
    stAioAttr.enBitwidth     = AUDIO_BIT_WIDTH_16;
    stAioAttr.enWorkmode     = AIO_MODE_PCM_MASTER_STD;
    stAioAttr.enSoundmode    = AUDIO_SOUND_MODE_STEREO;
    stAioAttr.u32EXFlag      = 1;
    stAioAttr.u32FrmNum      = 5;
    stAioAttr.u32PtNumPerFrm = OPUS_ENC_SAMPLES_PER_FRAME;
    stAioAttr.u32ChnCnt      = 2;
    stAioAttr.u32ClkSel      = 0;
    stAioAttr.enI2sType      = AIO_I2STYPE_INNERCODEC;

    s32AiChnCnt = stAioAttr.u32ChnCnt;
    do {
        /* Initialize AI */
        ret = LOTO_OPUS_StartAi(AiDev, s32AiChnCnt, &stAioAttr, AUDIO_SAMPLE_RATE_BUTT, HI_FALSE, NULL, 0);
        if (ret != 0) {
            LOGE("LOTO_OPUS_StartAi failed!\n");
            break;
        }

        // ret = LOTO_OPUS_ConfigAudioIn(stAioAttr.enSamplerate);
        // if (ret != 0)
        // {
        //     LOGE("LOTO_OPUS_ConfigAudioIn failed\n");
        //     break;
        // }

        do {
            /* Initialize Opus lib & Encoder */
            ret = LOTO_OPUS_InitEncoder(&opusEncoder);
            if (ret != 0) {
                LOGE("LOTO_OPUS_InitEncoder failed!\n");
                break;
            }

            /* Create a thread to get stream and encode */
            ret = LOTO_OPUS_CreateOpusThread(&opus_aenc_id, opusEncoder, AiDev, AiChn);
            if (ret != 0) {
                LOGE("LOTO_OPUS_CreateOpusThread failed!\n");
            }
            pthread_join(opus_aenc_id, 0);

            ret = LOTO_OPUS_DestroyEncoder(&opusEncoder);
            if (ret != 0) {
                LOGE("LOTO_OPUS_DestroyEncoder failed!\n");
            }

        } while (0);

        ret = LOTO_OPUS_StopAi(AiDev, s32AiChnCnt, HI_FALSE, HI_FALSE);
        if (ret != HI_SUCCESS) {
            LOGE("LOTO_OPUS_StopAi failed\n");
        }

    } while (0);

    return NULL;
}
