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

#define STATE_FRAMERATE_EX 0
#define STATE_COVER 0
#define STATE_RESOLUTION 0
#define STATE_OSD 0
#define COVER_TEST 0

char device_num[16];

SAMPLE_VI_CONFIG_S g_stViConfig;
SIZE_S g_stSize[2];
#ifdef SNS_IMX335
PIC_SIZE_E g_enSize[2] = {PIC_2592x1944, PIC_720P};
#else
PIC_SIZE_E g_enSize[2] = {PIC_1080P, PIC_720P};
#endif
PIC_SIZE_E g_resolution;
pthread_t vid = 0, aid = 0;
static int is_open = 1;

/* Cover Region */
HI_S32 g_coverState = 0;
HI_S32 g_coverAction = 0;

// RGN_HANDLE g_rgnHandle = 5;
// MPP_CHN_S g_stMppChnAttr;
// RGN_CHN_ATTR_S g_stRgnChnAttr;

typedef struct RtmpAttr
{
    char *url;
    void *rtmp_xiecc;
} RtmpAttr;

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

    pthread_create(&vid, NULL, LOTO_VENC_CLASSIC, NULL);
    pthread_create(&aid, NULL, LOTO_AENC_CLASSIC, NULL);
    // pthread_create(&aid, NULL, LOTO_OPUS_AudioEncode, NULL);

    LOGI("vid = %#x, aid = %#x\n", vid, aid);

    return s32Ret;
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

    g_coverState = 0;
    g_coverAction = 1;

    // const char *url = (const char *)p;
    // void *prtmp = rtmp_sender_alloc(url);

    RtmpAttr *rtmp_attr = (RtmpAttr *)p;
    const char *url = (const char *)rtmp_attr->url;
    void *prtmp = rtmp_attr->rtmp_xiecc;
    free(rtmp_attr);

    if (rtmp_sender_start_publish(prtmp, 0, 0) != 0)
    {
        LOGE("connect %s fail \n", url);
    }

    while (1)
    {
        if (!rtmp_sender_isOK(prtmp))
        {
            LOGI("Rebuild rtmp_sender\n");

            if (prtmp != NULL)
            {
                rtmp_sender_stop_publish(prtmp);
                rtmp_sender_free(prtmp);
                prtmp = NULL;
            }
            usleep(1000 * 100);
            prtmp = rtmp_sender_alloc(url);
            if (rtmp_sender_start_publish(prtmp, 0, 0) != 0)
            {
                LOGE("connect %s fail \n", url);
                rtmp_sender_stop_publish(prtmp);
                rtmp_sender_free(prtmp);
                prtmp = NULL;
            }
        }

        // cur_time = RTMP_GetTime();          // get current time(ms)
        cur_time = GetTimestamp(NULL, 1); // get current time(ms)

        /* send audio frame */
        a_ring_buf_len = ringget_audio(&a_ringinfo);
        if (a_ring_buf_len != 0)
        {
            if (pre_time == 0)
            {
                pre_time = cur_time;
            }

            a_time_count = cur_time - pre_time;

            // LOGD("a_time_count = %llu, cur_time = %llu, pre_time = %llu\n", a_time_count, cur_time, pre_time);
            // LOGD("audio_frame_size = %d bytes!\n", a_ringinfo.size);

            if (prtmp != NULL && is_open == 1)
            {
                s32Ret = rtmp_sender_write_audio_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, start_time);
                // s32Ret = rtmp_sender_write_opus_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, start_time);
                if (-1 == s32Ret)
                {
                    LOGE("Audio: Request reconnection.\n");
                }
            }
        }

        /* send vedio frame */
        v_ring_buf_len = ringget(&v_ringinfo);
        if (v_ring_buf_len != 0)
        {
            if (pre_time == 0)
            {
                pre_time = cur_time;
            }

            v_time_count = cur_time - pre_time;

            // LOGD("v_time_count = %llu, cur_time = %llu, pre_time = %llu\n", v_time_count, cur_time, pre_time);
            // LOGD("vedio_frame_size = %d bytes!\n", v_ringinfo.size);

            if (prtmp != NULL)
            {
                s32Ret = rtmp_sender_write_video_frame(prtmp, v_ringinfo.buffer, v_ringinfo.size, v_time_count, 0, start_time);
                if (-1 == s32Ret)
                {
                    LOGE("Vedio: Request reconnection.\n");
                }
            }
        }

        if (v_ring_buf_len == 0 && a_ring_buf_len == 0)
        {
            usleep(100);
        }
    }
}

#define VER_MAJOR 1
#define VER_MINOR 3
#define VER_BUILD 1
#define VER_EXTEN 0

int main(int argc, char *argv[])
{
    int s32Ret;
    char url_buf[1024];
    char resolution[16];
    char config_file_path[] = "/root/push.config";

    /* Initialize avctl_log file */
    s32Ret = InitAvctlLogFile();
    if (s32Ret != 0)
    {
        LOGE("create avctl_log file failed! \n");
        exit(1);
    }

    LOGI("RTMP App Version: %d.%d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_BUILD, VER_EXTEN);

    s32Ret = time_sync();
    if (s32Ret != HI_SUCCESS) {
        LOGE("time sync failed\n");
        exit(1);
    }

    strcpy(resolution, GetIniKeyString("push", "resolution", config_file_path));
    strcpy(url_buf, GetIniKeyString("push", "push_url", config_file_path));
    strncpy(device_num, GetIniKeyString("device", "device_num", config_file_path), 3);

    LOGI("push_url = %s\n", url_buf);
    LOGI("resolution = %s\n", resolution);
    LOGI("device_num = %s\n", device_num);

    if (0 == strncmp("1080", resolution, 4))
    {
        g_resolution = PIC_1080P;
    }
    else if (0 == strncmp("1944", resolution, 4))
    {
        g_resolution = PIC_2592x1944;
    }
    else if (0 == strncmp("720", resolution, 3))
    {
        g_resolution = PIC_720P;
    }
    else
    {
        LOGE("inputted resolution is not supported!\n");
    }

    /* Initialize rtmp_log file */
    FILE *rtmp_log = NULL;
    s32Ret = InitRtmpLogFile(&rtmp_log);
    if (s32Ret != 0)
    {
        LOGE("create rtmp_log file failed! \n");
    }
    else
    {
        RTMP_LogSetOutput(rtmp_log);
    }

    /* Initialize video and audio cache */
    ringmalloc(500 * 1024);
    ringmalloc_audio(1024);

    /* Get video and audio stream from hardware. */
    s32Ret = LOTO_RTMP_VA_CLASSIC();
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_RTMP_VA_CLASSIC failed! \n");
        return HI_FAILURE;
    }

    usleep(1000 * 5);

#ifndef OSD_DEV_INFO_NOT
    /* Add OSD */
    s32Ret = LOTO_OSD_CreateVideoOsdThread();
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_CreateVideoOsdThread failed! \n");
        return HI_FAILURE;
    }

    usleep(1000 * 5);
#endif

    // s32Ret = LOTO_RGN_InitCoverRegion();
    // if (s32Ret != HI_SUCCESS)
    // {
    //     LOGE("LOTO_RGN_InitCoverRegion failed!\n");
    //     return HI_FAILURE;
    // }

    // usleep(1000 * 3);

    /* Initialize rtmp_sender */
    RtmpAttr *rtmp_attr = (RtmpAttr *)malloc(sizeof(RtmpAttr));
    rtmp_attr->url = url_buf;
    rtmp_attr->rtmp_xiecc = rtmp_sender_alloc((const char *)rtmp_attr->url);

    /* Push video and audio stream through rtmp.*/
    pthread_t rtmp_pid;
    pthread_create(&rtmp_pid, NULL, LOTO_VIDEO_AUDIO_RTMP, (void *)rtmp_attr);

    pthread_join(rtmp_pid, 0);

    // LOTO_RGN_UninitCoverRegion();

    LOGI("=================== END =======================\n");
    return 0;
}
