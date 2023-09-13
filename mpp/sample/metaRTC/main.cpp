//
// Copyright (c) 2019-2022 yanggaofeng
//

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <yangutil/yangavinfotype.h>

// #include "yangpush/YangPushCommon.h"
// #include "yangpush/YangPushPublish.h"
#include "YangRtcPublish.h"
#include <yangutil/buffer/YangAudioEncoderBuffer.h>
#include <yangutil/buffer/YangVideoEncoderBuffer.h>
#include <yangutil/sys/YangLog.h>
#include <yangutil/sys/YangMath.h>
#include <yangutil/sys/YangUrl.h>

#include "common.h"
#include "Loto_aenc.h"
#include "Loto_opus.h"
#include "Loto_venc.h"
#include "sample_comm.h"

int             g_waitState = 0;
pthread_mutex_t g_lock;
pthread_cond_t  g_cond;

YangAudioEncoderBuffer *p_audioBuffer = NULL;
YangVideoEncoderBuffer *p_videoBuffer = NULL;
long long               basesatmp     = 0;

// YangPushPublish* p_cap = NULL;
YangContext    *g_context = NULL;
YangRtcPublish *g_rtcPub  = NULL;
YangUrlData     urlData;
bool            hasAudio = false;

char push_url[256];

/*
 * ctrl + c controller
 */
static int32_t b_exit = 0;
static void    ctrl_c_handler(int s)
{
    printf("\ncaught signal %d, exit.\n", s);
    b_exit = 1;
    pthread_mutex_lock(&g_lock);
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_lock);
}

static int32_t b_reload = 0;
static void    reload_handler(int s)
{
    printf("\ncaught signal %d, reload.\n", s);
    b_reload = 1;
}

SAMPLE_VI_CONFIG_S _stViConfig;
SIZE_S             _stSize[2];
PIC_SIZE_E         _enSize[2] = {PIC_2592x1944, PIC_1080P};
pthread_t          vid = 0, aid = 0;

HI_S32 LOTO_RTMP_VA_CLASSIC()
{
    HI_S32 s32Ret;
    HI_S32 s32ChnNum = 1;

    VI_DEV  ViDev  = 0;
    VI_PIPE ViPipe = 0;
    VI_CHN  ViChn  = 0;

    for (int i = 0; i < s32ChnNum; i++) {
        s32Ret = SAMPLE_COMM_SYS_GetPicSize(_enSize[i], &_stSize[i]);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
            return s32Ret;
        }
    }

    SAMPLE_COMM_VI_GetSensorInfo(&_stViConfig);
    if (SAMPLE_SNS_TYPE_BUTT == _stViConfig.astViInfo[0].stSnsInfo.enSnsType) {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n", 0);
        return HI_FAILURE;
    }

    s32Ret = LOTO_VENC_CheckSensor(_stViConfig.astViInfo[0].stSnsInfo.enSnsType, _stSize[0]);
    if (s32Ret != HI_SUCCESS) {
        s32Ret = LOTO_VENC_ModifyResolution(_stViConfig.astViInfo[0].stSnsInfo.enSnsType, &_enSize[0], &_stSize[0]);
        if (s32Ret != HI_SUCCESS) {
            return HI_FAILURE;
        }
    }

    _stViConfig.s32WorkingViNum                       = 1;
    _stViConfig.astViInfo[0].stDevInfo.ViDev          = ViDev;
    _stViConfig.astViInfo[0].stPipeInfo.aPipe[0]      = ViPipe;
    _stViConfig.astViInfo[0].stChnInfo.ViChn          = ViChn;
    _stViConfig.astViInfo[0].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    _stViConfig.astViInfo[0].stChnInfo.enPixFormat    = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    s32Ret = LOTO_VENC_VI_Init(&_stViConfig, HI_FALSE, HI_FALSE);
    if (s32Ret != HI_SUCCESS) {
        SAMPLE_PRT("Init VI err for %#x!\n", s32Ret);
        // LOGD("[%s] Init VI err for %#x! \n", log_Time(), s32Ret);
        return HI_FAILURE;
    }

    pthread_create(&vid, NULL, LOTO_VENC_CLASSIC, NULL);
    // pthread_create(&aid, NULL, LOTO_AENC_AAC_CLASSIC, NULL);
    pthread_create(&aid, NULL, LOTO_OPUS_AudioEncode, NULL);

    printf("LOTO_RTMP_VA_CLASSIC: vid=%d, aid = %d\n", vid, aid);

    return s32Ret;
}

/**
 * @brief Determine whether the URL starts with 'http://', 'ws://', 'wss://' or 'webrtc://'
 *
 * @param url push_url
 * @retval -1 error url
 * @retval 1 http://host:port/path, ws://host:port/path, wss://host:port/path
 * @retval 2 webrtc://host[:port]/app/stream
 */
int32_t parse_url(char *url)
{
    char *delimiter       = strchr(url, ':');
    char  url_protocal[8] = {0};

    if (delimiter == NULL) {
        printf("error url\n");
        return -1;
    } else {
        strncpy(url_protocal, url, delimiter - url);
        printf("use %s\n", url_protocal);
    }

    if (strcmp(url_protocal, "webrtc") == 0) {
        return 2;
    } else {
        return 1;
    }
}

static int publish(char *url)
{

    int err = Yang_Ok;
    memset(&urlData, 0, sizeof(urlData));

    int url_pro = parse_url(url);
    if (url_pro == 1) {
        if (yang_ws_url_parse(g_context->avinfo.sys.familyType, url, &urlData))
            return 1;
    } else if (url_pro == 2) {
        if (yang_url_parse(g_context->avinfo.sys.familyType, url, &urlData))
            return 1;
    }

    g_context->avinfo.sys.transType          = urlData.netType;
    g_context->avinfo.audio.audioEncoderType = Yang_AED_OPUS;
    g_context->avinfo.audio.sample           = 48000;

    yang_trace("netType==%d\n", urlData.netType);
    yang_trace("server=%s, port=%d\n", urlData.server, urlData.port);
    yang_trace("app=%s, stream=%s\n", urlData.app, urlData.stream);

    if (g_rtcPub == NULL) {
        g_rtcPub = new YangRtcPublish(g_context);
    }
    // if(hasAudio) {
    //     hasAudio=bool(p_cap->startAudioCapture()==Yang_Ok);
    // }
    // if (hasAudio) {
    //     p_cap->initAudioEncoding();
    // }

    // p_cap->initVideoEncoding();
    // p_cap->setRtcNetBuffer(g_rtcPub);

    yang_reindex(p_videoBuffer);
    yang_reindex(p_audioBuffer);
    g_rtcPub->setInAudioList(p_audioBuffer);
    g_rtcPub->setInVideoList(p_videoBuffer);

    // if (hasAudio)
    //     p_cap->startAudioEncoding();
    // p_cap->startVideoEncoding();

    if ((err = g_rtcPub->init(urlData.netType, urlData.server, urlData.port, urlData.app, urlData.stream)) != Yang_Ok) {
        return yang_error_wrap(err, " connect server failure!");
    }

    basesatmp = 0;
    g_rtcPub->start();
    // if (hasAudio)
    // 	p_cap->startAudioCaptureState();
    // p_cap->startVideoCaptureState();

    return err;
}

static void init_context()
{
    g_context = new YangContext();
    g_context->init((char *)"yang_config.ini");
    g_context->avinfo.video.videoEncoderFormat = YangI420;
    g_context->avinfo.enc.createMeta           = 0;

    // #if Yang_Enable_Openh264
    //     g_context->avinfo.enc.createMeta=0;
    // #else
    //     g_context->avinfo.enc.createMeta=1;
    // #endif

#if Yang_Enable_GPU_Encoding
    // using gpu encode
    g_context->avinfo.video.videoEncHwType     = YangV_Hw_Nvdia; // YangV_Hw_Intel,  YangV_Hw_Nvdia,
    g_context->avinfo.video.videoEncoderFormat = YangI420;       // YangI420;//YangArgb;
    g_context->avinfo.enc.createMeta           = 0;
#endif

#if Yang_Enable_Vr
    // using vr bg file name
    memset(g_context->avinfo.bgFilename, 0, sizeof(m_ini->bgFilename));
    QSettings settingsread((char *)"yang_config.ini", QSettings::IniFormat);
    strcpy(g_context->avinfo.bgFilename, settingsread.value("sys/bgFilename", QVariant("d:\\bg.jpeg")).toString().toStdString().c_str());
#endif

    g_context->avinfo.audio.enableMono        = yangfalse;
    g_context->avinfo.audio.sample            = 48000;
    g_context->avinfo.audio.channel           = 2;
    g_context->avinfo.audio.enableAec         = yangfalse;
    g_context->avinfo.audio.audioCacheNum     = 8;
    g_context->avinfo.audio.audioCacheSize    = 8;
    g_context->avinfo.audio.audioPlayCacheNum = 8;

    g_context->avinfo.video.videoCacheNum     = 10;
    g_context->avinfo.video.evideoCacheNum    = 10;
    g_context->avinfo.video.videoPlayCacheNum = 10;

    g_context->avinfo.audio.audioEncoderType = Yang_AED_OPUS;
    g_context->avinfo.sys.rtcLocalPort       = 17000;
    g_context->avinfo.enc.enc_threads        = 4;

    yang_setLogLevel(g_context->avinfo.sys.logLevel);
    yang_setLogFile(g_context->avinfo.sys.enableLogFile);

    g_context->avinfo.sys.mediaServer  = Yang_Server_Srs; // Yang_Server_Srs/Yang_Server_Zlm
    g_context->avinfo.sys.rtcLocalPort = 10000 + yang_random() % 15000;

    hasAudio = false;

    g_context->avinfo.rtc.enableDatachannel = yangfalse;

    // using h264 h265
    g_context->avinfo.video.videoEncoderType = Yang_VED_264; // Yang_VED_265;
    if (g_context->avinfo.video.videoEncoderType == Yang_VED_265) {
        g_context->avinfo.enc.createMeta        = 1;
        g_context->avinfo.rtc.enableDatachannel = yangfalse;
    }
    g_context->avinfo.rtc.iceCandidateType = YangIceHost;

    // srs do not use audio fec
    g_context->avinfo.audio.enableAudioFec = yangfalse;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: loto_metartc 'push_url'\n");
        exit(1);
    } else {
        memcpy(push_url, argv[1], strlen(argv[1]));
    }

    printf("push_url = %s\n", push_url);

    struct sigaction sigIntHandler;
    struct sigaction sigHupHandler;
    pthread_mutex_init(&g_lock, NULL);
    pthread_cond_init(&g_cond, NULL);

    // ctrl + c to exit
    sigIntHandler.sa_handler = ctrl_c_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, 0);

    // hup to reload
    sigHupHandler.sa_handler = reload_handler;
    sigemptyset(&sigHupHandler.sa_mask);
    sigHupHandler.sa_flags = 0;
    sigaction(SIGHUP, &sigHupHandler, 0);

    init_context();

    if (p_videoBuffer == NULL)
        p_videoBuffer = new YangVideoEncoderBuffer(g_context->avinfo.video.videoCacheNum);

    if (p_audioBuffer == NULL) {
        if (g_context->avinfo.audio.enableMono)
            p_audioBuffer = new YangAudioEncoderBuffer(g_context->avinfo.audio.audioCacheNum);
        else
            p_audioBuffer = new YangAudioEncoderBuffer(g_context->avinfo.audio.audioCacheNum);
    }

    LOTO_RTMP_VA_CLASSIC();

    publish(push_url);

    while (!b_exit) {
        g_waitState = 1;
        pthread_cond_wait(&g_cond, &g_lock);
        g_waitState = 0;
        if (b_reload) {
            // reload
            b_reload = 0;
        }
    }

    return 0;
}
