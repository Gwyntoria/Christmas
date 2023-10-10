#include <arpa/inet.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "rtmp.h"
#include "xiecc_rtmp.h"

#include "ConfigParser.h"
#include "WaInit.h"
#include "common.h"
#include "flv.h"
#include "http_server.h"
#include "loto_aenc.h"
#include "loto_comm.h"
#include "loto_controller.h"
#include "loto_cover.h"
#include "loto_opus.h"
#include "loto_osd.h"
#include "loto_venc.h"
#include "ringfifo.h"

#define AUDIO_ENCODER_AAC  0xAC
#define AUDIO_ENCODER_OPUS 0xAF

typedef struct RtmpThrArg {
    char *url;
    void *rtmp_xiecc;
} RtmpThrArg;

pthread_t vid = 0, aid = 0;

SAMPLE_VI_CONFIG_S g_stViConfig;
SIZE_S             g_stSize[2];
#ifdef SNS_IMX335
PIC_SIZE_E g_enSize[2] = {PIC_2592x1944, PIC_720P};
#else
PIC_SIZE_E g_enSize[2] = {PIC_1080P, PIC_720P};
#endif
char           g_device_num[16];
PIC_SIZE_E     g_resolution;
PAYLOAD_TYPE_E g_payload = PT_BUTT;

static char APP_VERSION[16];

// static char gs_server_url_buf[1024] = {0};
static char gs_push_url_buf[1024] = {0};
// static int g_pushurl_switch = 0;
int        g_profile        = -1;
static int gs_audio_state   = -1;
static int gs_audio_encoder = -1;

// static int gs_server_option = SERVER_OFFI;

time_t     program_start_time;
DeviceInfo device_info = {0};

// int  get_server_option() { return gs_server_option; }
// void set_server_option(int server_option) { gs_server_option = server_option; }

void LotoRtmpHandleSignal(HI_S32 signo)
{
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if (SIGINT == signo || SIGTERM == signo) {
        LOTO_AUDIO_DestoryTrdAenc(0);
        LOTO_COMM_VENC_StopGetStream();
        // LOTO_COMM_All_ISP_Stop();
        // LOTO_COMM_SYS_Exit();
        printf("\033[0;31mProgram termination abnormally!\033[0;39m\n");
    }

    exit(-1);
}

HI_S32 LOTO_RTMP_VA_CLASSIC()
{
    /* video thread */
    pthread_create(&vid, NULL, LOTO_VENC_CLASSIC, NULL);

    /* audio thread */
    if (gs_audio_state == TRUE) {
        if (gs_audio_encoder == AUDIO_ENCODER_AAC) {
            pthread_create(&aid, NULL, LOTO_AENC_CLASSIC, NULL);

        } else if (gs_audio_encoder == AUDIO_ENCODER_OPUS) {
            pthread_create(&aid, NULL, LOTO_OPUS_AudioEncode, NULL);
        }
    }

    return HI_SUCCESS;
}

void *LOTO_VIDEO_AUDIO_RTMP(void *p)
{
    LOGI("=== LOTO_VIDEO_AUDIO_RTMP ===\n");

    HI_S32   s32Ret           = 0;
    uint64_t a_time_count     = 0;
    uint64_t a_time_count_pre = 0;
    uint64_t a_start_time     = 0;
    uint64_t v_time_count     = 0;
    uint64_t v_time_count_pre = 0;
    uint64_t v_start_time     = 0;
    uint64_t cur_time         = 0;

    struct ringbuf v_ringinfo;
    int            v_ring_buf_len = 0;

    struct ringbuf a_ringinfo;
    int            a_ring_buf_len = 0;

    RtmpThrArg *rtmp_attr = (RtmpThrArg *)p;
    char       *url       = (char *)rtmp_attr->url;
    void       *prtmp     = rtmp_attr->rtmp_xiecc;
    free(rtmp_attr);

    if (rtmp_sender_start_publish(prtmp, 0, 0) != 0) {
        LOGE("connect %s fail \n", url);
    }

    int low_bitrate_mode = 0;

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

        if (gs_audio_state == TRUE) {
            /* send audio frame */
            a_ring_buf_len = ringget_audio(&a_ringinfo);
            if (a_ring_buf_len != 0) {
                cur_time = GetTimestampU64(NULL, 1); // get current time(ms)

                if (a_start_time == 0) {
                    a_start_time = cur_time;
                }

                a_time_count = cur_time - a_start_time;

                if (a_time_count < a_time_count_pre) {
                    a_time_count += 10;
                }

                a_time_count_pre = a_time_count;

                // LOGD("a_time_count = %llu, cur_time = %llu, start_time = %llu\n", a_time_count, cur_time,
                // start_time); LOGD("audio_frame_size = %d bytes!\n", a_ringinfo.size);

                if (prtmp != NULL) {
                    if (gs_audio_encoder == AUDIO_ENCODER_AAC) {
                        s32Ret = rtmp_sender_write_aac_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, 0);

                    } else if (gs_audio_encoder == AUDIO_ENCODER_OPUS) {
                        s32Ret = rtmp_sender_write_opus_frame(prtmp, a_ringinfo.buffer, a_ringinfo.size, a_time_count, 0);
                    }

                    if (s32Ret == -1) {
                        LOGE("Audio: Request reconnection.\n");
                    }
                }
            }
        }

        /* send video frame */
        v_ring_buf_len = ringget(&v_ringinfo);
        if (v_ring_buf_len != 0) {
            cur_time = GetTimestampU64(NULL, 1); // get current time(ms)

            if (v_start_time == 0) {
                v_start_time = cur_time;
            }

            v_time_count = cur_time - v_start_time;

            if (v_time_count < v_time_count_pre) {
                v_time_count += 10;
            }

            v_time_count_pre = v_time_count;

            // LOGD("v_time_count = %llu, cur_time = %llu, start_time = %llu\n", v_time_count, cur_time, v_start_time);
            // LOGD("vedio_frame_size = %d bytes!\n", v_ringinfo.size);

            if (prtmp != NULL && !low_bitrate_mode) {
                if (g_payload == PT_H264) {
                    s32Ret = rtmp_sender_write_avc_frame(prtmp, v_ringinfo.buffer, v_ringinfo.size, v_time_count, 0);

                } else if (g_payload == PT_H265) {
                    s32Ret = rtmp_sender_write_hevc_frame(prtmp, v_ringinfo.buffer, v_ringinfo.size, v_time_count, 0);
                }

                if (-1 == s32Ret) {
                    LOGE("video: Request reconnection.\n");
                }
            }
        }

        if (v_ring_buf_len == 0 && a_ring_buf_len == 0) {
            usleep(100);
        }

        LOTO_COVER_ChangeCover();
        // LOTO_COVER_ChangeCover(&low_bitrate_mode);
    }
}

void parse_config_file(const char *config_file_path)
{
    /* Get default push_url */
    strcpy(gs_push_url_buf, GetConfigKeyValue("push", "push_url", config_file_path));

    /* If push address should be requested, server address must be set first */
    if (strncmp("on", GetConfigKeyValue("push", "requested_url", config_file_path), 2) == 0) {
        /* Get server token */
        char server_token[1024] = {0};
        strcpy(server_token, GetConfigKeyValue("push", "server_token", config_file_path));
        // LOGD("server_token = %s\n", server_token);

        /* Get server url */
        char server_url[1024];
        // LOGI("Waiting for updating server address\n");
        // while (1) {
        //     if (gs_server_option != SERVER_NULL) {
        //         break;
        //     } else {
        //         LOGE("Waiting......\n");
        //         sleep(3);
        //     }
        // }

        // if (gs_server_option == SERVER_TEST) {
        //     strcpy(server_url, GetConfigKeyValue("push", "test_server_url", config_file_path));
        // } else if (gs_server_option == SERVER_OFFI) {
        //     strcpy(server_url, GetConfigKeyValue("push", "offi_server_url", config_file_path));
        // }

        strcpy(server_url, GetConfigKeyValue("push", "server_url", config_file_path));
        strcpy(device_info.server_url, server_url);

        LOGI("server_url = %s\n", server_url);

        loto_room_info *pRoomInfo = loto_room_init(server_url, server_token);
        if (!pRoomInfo) {
            LOGE("room_info is empty\n");
            exit(2);
        }
        memset(gs_push_url_buf, 0, sizeof(gs_push_url_buf));
        strcpy(gs_push_url_buf, pRoomInfo->szPushURL);
    }
    strcpy(device_info.push_url, gs_push_url_buf);
    LOGI("push_url = %s\n", gs_push_url_buf);

    /* resolution */
    char *resolution = GetConfigKeyValue("push", "resolution", config_file_path);
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

    /* device num */
    strncpy(g_device_num, GetConfigKeyValue("device", "device_num", config_file_path), 3);
    strcpy(device_info.device_num, g_device_num);
    LOGI("device_num = %s\n", g_device_num);

    const char *video_encoder = GetConfigKeyValue("push", "video_encoder", config_file_path);
    strcpy(device_info.video_encoder, video_encoder);
    LOGI("video_encoder = %s\n", video_encoder);

    if (strncmp("h264", video_encoder, 4) == 0) {
        /* video encoder */
        g_payload = PT_H264;

        char *video_encoder_profile = GetConfigKeyValue("h264", "profile", config_file_path);

        /* profile */
        if (strncmp("baseline", video_encoder_profile, 8) == 0) {
            g_profile = 0;

        } else if (strncmp("main", video_encoder_profile, 4) == 0) {
            g_profile = 1;

        } else if (strncmp("high", video_encoder_profile, 4) == 0) {
            g_profile = 2;

        } else {
            LOGE("The set value of profile is %s! Please set baseline, main or high.\n", video_encoder_profile);
            exit(1);
        }
    } else if (strncmp("h265", video_encoder, 4) == 0) {
        /* video encoder */
        g_payload = PT_H265;

        char *video_encoder_profile = GetConfigKeyValue("h265", "profile", config_file_path);

        /* profile */
        if (strncmp("main", video_encoder_profile, 4) == 0) {
            g_profile = 0;

        } else if (strncmp("main_10", video_encoder_profile, 7) == 0) {
            g_profile = 1;

        } else {
            LOGE("The set value of profile is %s! Please set main or main_10.\n", video_encoder_profile);
            exit(1);
        }
    }

    /* audio_state */
    char *audio_state = GetConfigKeyValue("push", "audio_state", config_file_path);
    strcpy(device_info.audio_state, audio_state);
    if (strncmp("off", audio_state, 3) == 0) {
        gs_audio_state = FALSE;
    } else if (strncmp("on", audio_state, 2) == 0) {
        gs_audio_state = TRUE;

        /* audio_encoder */
        const char *audio_encoder = GetConfigKeyValue("push", "audio_encoder", config_file_path);
        strcpy(device_info.audio_encoder, audio_encoder);
        LOGI("audio_encoder = %s\n", audio_encoder);

        if (strncmp("aac", audio_encoder, 3) == 0) {
            gs_audio_encoder = AUDIO_ENCODER_AAC;

        } else if (strncmp("opus", audio_encoder, 4) == 0) {
            gs_audio_encoder = AUDIO_ENCODER_OPUS;

        } else {
            LOGE("The set value of audio_encoder is %s! The supported audio encoders: aac, opus\n", audio_encoder);
            exit(1);
        }
    } else {
        LOGE("The set value of audio_state is %s! Please set on or off.\n", audio_state);
        exit(1);
    }
}

void *monitor_time_difference(void *arg)
{
    LOGI("======== monitor_time_difference =========\n");
    time_t current_time;

    while (1) {
        // Get the current time
        current_time = time(NULL);

        // Calculate time difference
        int time_difference = difftime(current_time, program_start_time);

        if (time_difference >= (7 * SECONDS_PER_DAY)) {
            if (LOTO_COVER_GetCoverState() == COVER_ON) {
                // Time difference reaches 7 days, and no one is online, execute restart
                LOGI("The running time has exceeded 7 days, reboot now\n");
                reboot_system();
                break;

            } else {
                LOGD("There is someone online. Wait for 10s\n");
                sleep(10);
                continue;
            }
        }

        // Check the time difference every other period of time
        sleep(12 * 60 * 60); // 12 hours
    }

    return NULL;
}

void fill_device_net_info(DeviceInfo *device_info)
{
    get_local_ip_address(device_info->ip_addr);
    get_local_mac_address(device_info->mac_addr);
}

#define VER_MAJOR 1
#define VER_MINOR 8
#define VER_BUILD 6

int main(int argc, char *argv[])
{
    int        s32Ret             = 0;
    const char config_file_path[] = PUSH_CONFIG_FILE_PATH;

    signal(SIGINT, LotoRtmpHandleSignal);
    signal(SIGTERM, LotoRtmpHandleSignal);

    /* Initialize avctl_log file */
    s32Ret = InitAvctlLogFile();
    if (s32Ret != 0) {
        LOGE("Create avctl_log file failed! \n");
        return -1;
    }

    /* sync local time from net_time */
    s32Ret = get_net_time();
    if (s32Ret != HI_SUCCESS) {
        LOGE("Time sync failed\n");
        exit(1);
    }

    /* Gets the program startup time */
    program_start_time = time(NULL);
    strcpy(device_info.start_time, GetTimestampString());

    sprintf(APP_VERSION, "%d.%d.%d", VER_MAJOR, VER_MINOR, VER_BUILD);
    strcpy(device_info.app_version, APP_VERSION);
    LOGI("RTMP App Version: %s\n", APP_VERSION);

    /* socket: server */
    // pthread_t socket_server_pid;
    // pthread_create(&socket_server_pid, NULL, socket_server_thread, NULL);

    /* get global variables from config file */
    parse_config_file(config_file_path);
    fill_device_net_info(&device_info);

    /* Initialize rtmp_log file */
    FILE *rtmp_log = NULL;
    s32Ret         = InitRtmpLogFile(&rtmp_log);
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

    usleep(1000 * 10);

#ifndef OSD_DEV_INFO_NOT
    /* Add OSD */
    s32Ret = LOTO_OSD_CreateVideoOsdThread();
    if (s32Ret != HI_SUCCESS) {
        LOGE("LOTO_OSD_CreateVideoOsdThread failed! \n");
        return HI_FAILURE;
    }

    usleep(1000 * 10);
#endif

    LOTO_COVER_InitCoverRegion();
    usleep(1000 * 10);

    /* Initialize rtmp_sender */
    RtmpThrArg *rtmp_attr = (RtmpThrArg *)malloc(sizeof(RtmpThrArg));
    rtmp_attr->url        = gs_push_url_buf;
    rtmp_attr->rtmp_xiecc = rtmp_sender_alloc((const char *)rtmp_attr->url);

    /* Push video and audio stream through rtmp. */
    pthread_t rtmp_pid;
    if (pthread_create(&rtmp_pid, NULL, LOTO_VIDEO_AUDIO_RTMP, (void *)rtmp_attr) != 0) {
        fprintf(stderr, "Failed to create LOTO_VIDEO_AUDIO_RTMP thread\n");
    }
    usleep(1000 * 10);

    /* socket: server */
    pthread_t socket_server_pid;
    if (pthread_create(&socket_server_pid, NULL, socket_server_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create socket_server_thread\n");
    }
    usleep(100);

    pthread_t sync_time_pid;
    if (pthread_create(&sync_time_pid, NULL, sync_time, NULL) != 0) {
        fprintf(stderr, "Failed to create sync_time thread\n");
    }
    usleep(100);

    pthread_t monitor_time_thread;
    if (pthread_create(&monitor_time_thread, NULL, monitor_time_difference, NULL) != 0) {
        fprintf(stderr, "Failed to create monitor_time_difference thread\n");
    }
    usleep(100);

    // http_server();
    pthread_t http_server_thread;
    if (pthread_create(&http_server_thread, NULL, http_server, NULL) != 0) {
        fprintf(stderr, "Failed to create http_server thread\n");
    }

    pthread_join(rtmp_pid, 0);

    LOGI("=================== END =======================\n");
    return 0;
}
