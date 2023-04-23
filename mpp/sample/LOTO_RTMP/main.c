#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <inttypes.h>

#include "log.h"
#include "rtmp.h"
#include "xiecc_rtmp.h"

#include "ringfifo.h"
#include "common.h"
#include "loto_comm.h"
#include "loto_aenc.h"
#include "loto_venc.h"
#include "loto_osd.h"
#include "loto_opus.h"
#include "ConfigParser.h"
#include "loto_controller.h"
#include "WaInit.h"

#define AUDIO_ENCODER_AAC     0xAC
#define AUDIO_ENCODER_OPUS    0xFF

typedef struct RtmpThrArg {
    char *url;
    void *rtmp_xiecc;
} RtmpThrArg;

pthread_t vid = 0, aid = 0;

SAMPLE_VI_CONFIG_S g_stViConfig;
SIZE_S g_stSize[2];
#ifdef SNS_IMX335
PIC_SIZE_E g_enSize[2] = {PIC_2592x1944, PIC_720P};
#else
PIC_SIZE_E g_enSize[2] = {PIC_1080P, PIC_720P};
#endif
char g_device_num[16];
PIC_SIZE_E g_resolution;
PAYLOAD_TYPE_E g_payload = PT_BUTT;

static char gs_url_buf[1024] = {0};
// static int g_pushurl_switch = 0;
int g_profile = -1;
static int gs_audio_state = -1;
static int gs_audio_encoder = -1;

HI_S32 LOTO_RTMP_VA_CLASSIC()
{
    HI_S32 s32Ret = 0;
    HI_S32 s32ChnNum = 2;
    HI_S32 s32ViCnt = 2;
    HI_BOOL bLowDelay = HI_FALSE;

    /* get picture size */
    for (int i = 0; i < s32ChnNum; i++)
    {
        s32Ret = LOTO_COMM_SYS_GetPicSize(g_enSize[i], &g_stSize[i]);
        if (HI_SUCCESS != s32Ret)
        {
            LOGE("LOTO_COMM_SYS_GetPicSize failed!\n");
            return s32Ret;
        }
    }

    LOTO_COMM_VI_GetSensorInfo(&g_stViConfig);
    for (int i = 0; i < s32ViCnt; i++)
    {
        if (SAMPLE_SNS_TYPE_BUTT == g_stViConfig.astViInfo[i].stSnsInfo.enSnsType)
        {
            LOGE("Not set SENSOR%d_TYPE !\n", i);
            return HI_FAILURE;
        }
    }

    for (int i = 0; i < s32ViCnt; i++)
    {
        s32Ret = LOTO_VENC_CheckSensor(g_stViConfig.astViInfo[i].stSnsInfo.enSnsType, g_stSize[i]);
        if (s32Ret != HI_SUCCESS)
        {
            s32Ret = LOTO_VENC_ModifyResolution(g_stViConfig.astViInfo[i].stSnsInfo.enSnsType, &g_enSize[i], &g_stSize[0]);
            if (s32Ret != HI_SUCCESS)
            {
                return HI_FAILURE;
            }
        }
    }

    /* configure vi */
    s32Ret = LOTO_VENC_VI_Init(&g_stViConfig, bLowDelay, HI_FALSE);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_VENC_VI_Init failed! \n");
        return HI_FAILURE;
    }

    /* video thread */
    pthread_create(&vid, NULL, LOTO_VENC_CLASSIC, NULL);

    /* audio thread */
    if (gs_audio_state == 1) {
        if (gs_audio_encoder == AUDIO_ENCODER_AAC) {
            /* aac */
            pthread_create(&aid, NULL, LOTO_AENC_CLASSIC, NULL);
        } else if (gs_audio_encoder == AUDIO_ENCODER_OPUS) {
            /* opus */
            pthread_create(&aid, NULL, LOTO_OPUS_AudioEncode, NULL);
        }
    }

    LOGI("vid = %#x, aid = %#x\n", vid, aid);

    return s32Ret;
}

static int gs_cover_switch = 0;
static int gs_cover_state = COVER_OFF;

void get_cover_state(int *cover_state) {
    *cover_state = gs_cover_state;
}

void set_cover_switch(int *cover_switch) {
    gs_cover_switch = *cover_switch;
}

void *LOTO_VIDEO_AUDIO_RTMP(void *p)
{
    LOGI("=== LOTO_VIDEO_AUDIO_RTMP ===\n");

    HI_S32 s32Ret;

    uint32_t start_time = 0;
    uint64_t v_time_count = 0;
    uint64_t a_time_count = 0;
    uint64_t cur_time = 0;
    uint64_t pre_time = 0;

    // uint64_t count = 0;

    struct ringbuf v_ringinfo;
    int v_ring_buf_len = 0;

    struct ringbuf a_ringinfo;
    int a_ring_buf_len = 0;

    // const char *url = (const char *)p;
    // void *prtmp = rtmp_sender_alloc(url);

    RtmpThrArg *rtmp_attr = (RtmpThrArg *)p;
    char *url = (char *)rtmp_attr->url;
    void *prtmp = rtmp_attr->rtmp_xiecc;
    free(rtmp_attr);

    if (rtmp_sender_start_publish(prtmp, 0, 0) != 0) {
        LOGE("connect %s fail \n", url);
    }

    while (1) {
        if (!rtmp_sender_isOK(prtmp)) {
            LOGI("Rebuild rtmp_sender\n");

            if (prtmp != NULL) {
                rtmp_sender_stop_publish(prtmp);
                rtmp_sender_free(prtmp);
                prtmp = NULL;
            }
            usleep(1000 * 100);
            prtmp = rtmp_sender_alloc(url);
            if (rtmp_sender_start_publish(prtmp, 0, 0) != 0) {
                LOGE("connect %s fail \n", url);
                rtmp_sender_stop_publish(prtmp);
                rtmp_sender_free(prtmp);
                prtmp = NULL;
            }
        }

        // cur_time = RTMP_GetTime();          // get current time(ms)
        cur_time = GetTimestamp(NULL, 1); // get current time(ms)

        if (gs_audio_state == 1) {
            /* send audio frame */
            a_ring_buf_len = ringget_audio(&a_ringinfo);
            if (a_ring_buf_len != 0) {
                if (pre_time == 0) {
                    pre_time = cur_time;
                }

                a_time_count = cur_time - pre_time;

                // LOGD("a_time_count = %llu, cur_time = %llu, pre_time = %llu\n", a_time_count, cur_time, pre_time);
                // LOGD("audio_frame_size = %d bytes!\n", a_ringinfo.size);

                if (prtmp != NULL) {
                    if (gs_audio_encoder = AUDIO_ENCODER_AAC) {
                        s32Ret = rtmp_sender_write_audio_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, start_time);
                    } else if (gs_audio_encoder = AUDIO_ENCODER_OPUS) {
                        s32Ret = rtmp_sender_write_opus_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, start_time);
                    }
                    if (s32Ret == -1) {
                        LOGE("Audio: Request reconnection.\n");
                    }
                }
            }
        }


        /* send video frame */
        v_ring_buf_len = ringget(&v_ringinfo);
        if (v_ring_buf_len != 0)
        {
            if (pre_time == 0) {
                pre_time = cur_time;
            }

            v_time_count = cur_time - pre_time;

            // LOGD("v_time_count = %llu, cur_time = %llu, pre_time = %llu\n", v_time_count, cur_time, pre_time);
            // LOGD("vedio_frame_size = %d bytes!\n", v_ringinfo.size);

            if (prtmp != NULL) {
                s32Ret = rtmp_sender_write_video_frame(prtmp, v_ringinfo.buffer, v_ringinfo.size, v_time_count, 0, start_time);
                if (-1 == s32Ret) {
                    LOGE("video: Request reconnection.\n");
                }
            }
        }

        if (v_ring_buf_len == 0 && a_ring_buf_len == 0) {
            usleep(100);
        }

        if (gs_cover_state == COVER_OFF && gs_cover_switch == COVER_ON) {
            LOTO_VENC_DisplayCover();
            gs_cover_state = gs_cover_switch;
        } else if (gs_cover_state == COVER_ON && gs_cover_switch == COVER_OFF) {
            LOTO_VENC_RemoveCover();
            gs_cover_state = gs_cover_switch;
        }
        
    }
}

void parse_config_file(const char *config_file_path){
    char resolution[16];

    /* device num */
    strncpy(g_device_num, GetIniKeyString("device", "device_num", config_file_path), 3);
    LOGI("device_num = %s\n", g_device_num);

    /* url */
    strcpy(gs_url_buf, GetIniKeyString("push", "push_url", config_file_path));
    if (strncmp("on", GetIniKeyString("push", "requested_url", config_file_path), 2) == 0) {
        loto_room_info *pRoomInfo = loto_room_init();
        memset(gs_url_buf, 0, sizeof(gs_url_buf));
        strcpy(gs_url_buf, pRoomInfo->szPushURL);
    }
    LOGI("push_url = %s\n", gs_url_buf);

    /* resolution */
    strcpy(resolution, GetIniKeyString("push", "resolution", config_file_path));
    if (0 == strncmp("1080", resolution, 4)) {
        g_resolution = PIC_1080P;
    } else if (0 == strncmp("1944", resolution, 4)) {
        g_resolution = PIC_2592x1944;
    } else if (0 == strncmp("720", resolution, 3)) {
        g_resolution = PIC_720P;
    } else {
        LOGE("The set value of resolution is not supported!\n");
    }
    LOGI("resolution = %s\n", resolution);

    const char *video_encoder = GetIniKeyString("push", "video_encoder", config_file_path);
    LOGI("video_encoder = %s\n", video_encoder);
    
    if (strncmp("h264", video_encoder, 4) == 0) {
        /* video encoder */
        g_payload = PT_H264;

        /* profile */
        if (strncmp("baseline", GetIniKeyString(video_encoder, "profile", config_file_path), 8) == 0) {
            g_profile = 0;
        } else if (strncmp("main", GetIniKeyString(video_encoder, "profile", config_file_path), 4) == 0) {
            g_profile = 1;
        } else if (strncmp("high", GetIniKeyString(video_encoder, "profile", config_file_path), 4) == 0) {
            g_profile = 2;
        } else {
            LOGE("The set value of profile is wrong! Please set baseline, main or high.\n");
            exit(1);
        }
    } else if (strncmp("h265", video_encoder, 4) == 0) {
        /* video encoder */
        g_payload = PT_H265;

         /* profile */
        if (strncmp("main", GetIniKeyString(video_encoder, "profile", config_file_path), 4) == 0) {
            g_profile = 0;
        } else if (strncmp("main_10", GetIniKeyString(video_encoder, "profile", config_file_path), 7) == 0) {
            g_profile = 1;
        } else {
            LOGE("The set value of profile is wrong! Please set main or main_10.\n");
            exit(1);
        }
    }

    /* audio_state */
    if (strncmp("off", GetIniKeyString("push", "audio_state", config_file_path), 3) == 0) {
        gs_audio_state = 0;
    } else if (strncmp("on", GetIniKeyString("push", "audio_state", config_file_path), 2) == 0) {
        gs_audio_state = 1;
    } else {
        LOGE("The set value of audio_state is wrong! Please set on or off.\n");
        exit(1);
    }

    /* audio_encoder */
    const char *audio_encoder = GetIniKeyString("push", "audio_encoder", config_file_path);
    LOGI("audio_encoder = %s\n", audio_encoder);
    
    if (strncmp("aac", audio_encoder, 3) == 0) {
        gs_audio_encoder = AUDIO_ENCODER_AAC;
    } else if (strncmp("opus", audio_encoder, 4) == 0) {
        gs_audio_encoder = AUDIO_ENCODER_OPUS;
    } else {
        LOGE("The set value of audio_encoder is wrong! The supported audio encoders: aac, opus\n");
        exit(1);
    }
}

#define VER_MAJOR 1
#define VER_MINOR 4
#define VER_BUILD 1
#define VER_EXTEN 9

int main(int argc, char *argv[]) {
    int s32Ret;
    char config_file_path[] = "/root/push.config";

    /* Initialize avctl_log file */
    s32Ret = InitAvctlLogFile();
    if (s32Ret != 0) {
        LOGE("Create avctl_log file failed! \n");
        return -1;
    }

    LOGI("RTMP App Version: %d.%d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_BUILD, VER_EXTEN);

    s32Ret = time_sync();
    if (s32Ret != HI_SUCCESS) {
        LOGE("Time sync failed\n");
        exit(1);
    }

    /* get global variables from config file */
    parse_config_file(config_file_path);

    /* Initialize rtmp_log file */
    FILE *rtmp_log = NULL;
    s32Ret = InitRtmpLogFile(&rtmp_log);
    if (s32Ret != 0) {
        LOGE("Create rtmp_log file failed! \n");
    } else {
        RTMP_LogSetOutput(rtmp_log);
    }

    /* Initialize video and audio cache */
    ringmalloc(500 * 1024);
    ringmalloc_audio(1024);

    /* Get video and audio stream from hardware. */
    s32Ret = LOTO_RTMP_VA_CLASSIC();
    if (s32Ret != HI_SUCCESS) {
        LOGE("LOTO_RTMP_VA_CLASSIC failed! \n");
        return HI_FAILURE;
    }

    usleep(1000 * 5);

#ifndef OSD_DEV_INFO_NOT
    /* Add OSD */
    s32Ret = LOTO_OSD_CreateVideoOsdThread();
    if (s32Ret != HI_SUCCESS) {
        LOGE("LOTO_OSD_CreateVideoOsdThread failed! \n");
        return HI_FAILURE;
    }

    usleep(1000 * 5);
#endif

    s32Ret = LOTO_RGN_InitCoverRegion();
    if (s32Ret != HI_SUCCESS) {
        LOGE("LOTO_RGN_InitCoverRegion failed!\n");
        return HI_FAILURE;
    }

    usleep(1000 * 3);

    /* Initialize rtmp_sender */
    RtmpThrArg *rtmp_attr = (RtmpThrArg *)malloc(sizeof(RtmpThrArg));
    rtmp_attr->url = gs_url_buf;
    rtmp_attr->rtmp_xiecc = rtmp_sender_alloc((const char *)rtmp_attr->url);

    /* Push video and audio stream through rtmp. */
    pthread_t rtmp_pid;
    pthread_create(&rtmp_pid, NULL, LOTO_VIDEO_AUDIO_RTMP, (void *)rtmp_attr);

    pthread_t socket_server_pid;
    pthread_create(&socket_server_pid, NULL, server_thread, NULL);
    pthread_join(socket_server_pid, 0);

    pthread_join(rtmp_pid, 0);

    LOTO_RGN_UninitCoverRegion();

    LOGI("=================== END =======================\n");
    return 0;
}
