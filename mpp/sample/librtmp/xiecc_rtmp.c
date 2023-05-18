/***************************************************************
 Author: xiecc
 Date: 2014-04-03
 E-mail: xiechc@gmail.com
 Notice:
 *  you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation;
 *  flvmuxer is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY;
 *************************************************************/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtmp.h"
#include "log.h"
#include "xiecc_rtmp.h"
#include "flv.h"

#define AAC_ADTS_HEADER_SIZE 7
#define RTMP_MESSAGE_HEADER_LEN 11
#define FLV_PRE_TAG_LEN 4
#define OPUS_HEADER_SIZE 21

typedef struct OpusHeader
{
    uint8_t version;
    uint8_t output_channel_count;
    uint16_t pre_skip;
    uint32_t input_sample_rate;
    uint16_t output_gain;
    uint8_t channel_mapping_family;
    uint16_t frame_size;
} OpusHeader;

typedef struct AudioSpecificConfig
{
    uint8_t audio_object_type;      /* audio type */
    uint8_t sample_frequency_index; /* sampleing rate */
    uint8_t channel_configuration;  /* mono or stereo */
} AudioSpecificConfig;

typedef struct RTMP_XIECC
{
    RTMP *rtmp;
    AudioSpecificConfig config;
    uint32_t audio_config_ok;
    uint32_t video_config_ok;
} RTMP_XIECC;

static void write_data_to_flv_file(char **value, uint32_t length)
{
    int ret;
    FILE *flv_file = fopen("rtmp_file.flv", "ab");
    if (flv_file == NULL)
    {
        printf("open flv_file fail!\n");
    }

    ret = fwrite(*value, length, 1, flv_file);
    if (ret < 0)
    {
        printf("write data failed!\n");
    }
    fclose(flv_file);
}

// @brief alloc function
// @param [in] url     : RTMP URL, rtmp://127.0.0.1/live/xxx
// @return             : rtmp_sender handler
void *rtmp_sender_alloc(const char *url) // return handle
{
    RTMP_Log(RTMP_LOGINFO, "=== rtmp_sender_alloc ===");

    RTMP_XIECC *rtmp_xiecc;
    RTMP *rtmp;

    if (url == NULL)
    {
        return NULL;
    }
    RTMP_LogSetLevel(RTMP_LOGDEBUG);
    rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    rtmp->Link.timeout = 5; // It takes a few secs to wait the connection of network, before return timeout.
    rtmp->Link.lFlags |= RTMP_LF_LIVE;

    if (!RTMP_SetupURL(rtmp, (char *)url))
    {
        RTMP_Log(RTMP_LOGWARNING, "Couldn't set the specified url (%s)!", url);
        RTMP_Free(rtmp);
        return NULL;
    }

    RTMP_EnableWrite(rtmp);
    rtmp_xiecc = calloc(1, sizeof(RTMP_XIECC));
    rtmp_xiecc->rtmp = rtmp;
    return (void *)rtmp_xiecc;
}

// @brief free rtmp_sender handler
// @param [in] rtmp_sender handler
void rtmp_sender_free(void *handle)
{
    RTMP_Log(RTMP_LOGINFO, "=== rtmp_sender_free ===");

    RTMP_XIECC *rtmp_xiecc;
    RTMP *rtmp;

    if (handle == NULL)
    {
        return;
    }

    rtmp_xiecc = (RTMP_XIECC *)handle;
    rtmp = rtmp_xiecc->rtmp;
    if (rtmp != NULL)
    {
        RTMP_Free(rtmp);
    }
    free(rtmp_xiecc);
}

// @brief start publish
// @param [in] rtmp_sender handler
// @param [in] flag        stream falg
// @param [in] ts_us       timestamp in us
// @return             : 0: OK; others: FAILED
int rtmp_sender_start_publish(void *handle, uint32_t flag, int64_t ts_us)
{
    RTMP_Log(RTMP_LOGINFO, "=== rtmp_sender_start_publish ===");

    RTMP_XIECC *rtmp_xiecc = (RTMP_XIECC *)handle;
    RTMP *rtmp;

    if (rtmp_xiecc == NULL)
    {
        return 1;
    }
    rtmp = rtmp_xiecc->rtmp;
    if (!RTMP_Connect(rtmp, NULL) || !RTMP_ConnectStream(rtmp, 0))
    {
        return 1;
    }
    return 0;
}

// @brief stop publish
// @param [in] rtmp_sender handler
// @return             : 0: OK; others: FAILED
int rtmp_sender_stop_publish(void *handle)
{
    RTMP_Log(RTMP_LOGINFO, "=== rtmp_sender_stop_publish ===");

    RTMP_XIECC *rtmp_xiecc = (RTMP_XIECC *)handle;
    RTMP *rtmp;

    if (rtmp_xiecc == NULL)
    {
        return 1;
    }

    rtmp = rtmp_xiecc->rtmp;
    RTMP_Close(rtmp);
    return 0;
}

static AudioSpecificConfig gen_config(uint8_t *frame)
{
    AudioSpecificConfig config = {0, 0, 0};

    if (frame == NULL)
    {
        return config;
    }
    config.audio_object_type = (frame[2] & 0xc0) >> 6;
    // config.sample_frequency_index = (frame[2] & 0x3c) >> 2;
    config.sample_frequency_index = 4;
    config.channel_configuration = (frame[3] & 0xc0) >> 6;
    // config.channel_configuration = 1;
    return config;
}

static uint8_t gen_audio_tag_header(AudioSpecificConfig config)
{
    uint8_t soundType = config.channel_configuration - 1; // 0 mono, 1 stereo
    uint8_t soundRate = 0;
    uint8_t val = 0;

    switch (config.sample_frequency_index)
    {
    case 10:
    { // 11.025k
        soundRate = 1;
        break;
    }
    case 7:
    { // 22k
        soundRate = 2;
        break;
    }
    case 4:
    { // 44k
        soundRate = 3;
        break;
    }
    case 11:
    { // 8k, 5.5k
        soundRate = 0;
        break;
    }
    default:
    {
        // return val;
        soundRate = 2;
        break;
    }
    }
    // val = 0xA0 | (soundRate << 2) | 0x02 | soundType;
    // val = 0xAE; /* aac 44k mono */
    val = 0xAF; /* aac 44k stereo */
    // val = 0x77;     /* g711 44k mono */
    // val = 0x9E;     /* opus 44k stereo */
    return val;
}

/**
 * @brief Get the Audio Data Transport Stream(adts)
 *
 * @param len [out] aac frame length
 * @param offset [out] opus raw data
 * @param start [in] pointer to the audio data
 * @param total [in] audio data size
 * @return uint8_t* pointer to aac audio frame
 */
static uint8_t *get_adts(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total)
{
    uint8_t *p = *offset;
    uint32_t frame_len_1;
    uint32_t frame_len_2;
    uint32_t frame_len_3;
    uint32_t frame_length;

    if (total < AAC_ADTS_HEADER_SIZE)
    {
        return NULL;
    }
    if ((p - start) >= total)
    {
        return NULL;
    }

    if (p[0] != 0xff)
    {
        return NULL;
    }
    if ((p[1] & 0xf0) != 0xf0)
    {
        return NULL;
    }
    frame_len_1 = p[3] & 0x03;
    frame_len_2 = p[4];
    frame_len_3 = (p[5] & 0xe0) >> 5;
    frame_length = (frame_len_1 << 11) | (frame_len_2 << 3) | frame_len_3;
    *offset = p + frame_length;
    *len = frame_length;
    return p;
}

// @brief send aac frame
// @param [in] rtmp_sender handler
// @param [in] data       : aac audio data
// @param [in] size       : aac audio data size
// @param [in] dts_us     : decode timestamp of frame
int rtmp_sender_write_aac_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  uint32_t start_time)
{
    int ret;
    RTMP_XIECC *rtmp_xiecc = (RTMP_XIECC *)handle;
    RTMP *rtmp;
    uint32_t audio_ts = (uint32_t)dts_us;
    uint8_t *audio_buf = data;
    uint32_t audio_buf_len = size;
    uint8_t *audio_buf_offset = audio_buf; // aac header length
    uint8_t *audio_frame;
    uint32_t adts_len; // aac data length
    uint32_t offset;
    uint32_t body_len;
    uint32_t output_len;
    char *output;
    // audio_ts = RTMP_GetTime() - start_time;
    if ((data == NULL) || (rtmp_xiecc == NULL))
    {
        return 1;
    }
    rtmp = rtmp_xiecc->rtmp;
    while (1)
    {
        if (!RTMP_IsConnected(rtmp))
        {
            RTMP_Log(RTMP_LOGERROR, "aac: connect failed");
            return -1;
        }
        // Audio OUTPUT
        offset = 0;
        audio_frame = get_adts(&adts_len, &audio_buf_offset, audio_buf, audio_buf_len);
        if (audio_frame == NULL)
            break;
        if (rtmp_xiecc->audio_config_ok == 0)
        {
            rtmp_xiecc->config = gen_config(audio_frame);
            body_len = 2 + 2; // AudioTagHeader + AudioSpecificConfig
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = 0x08;                      // TagType
            output[offset++] = (uint8_t)(body_len >> 16); // DataSize
            output[offset++] = (uint8_t)(body_len >> 8);  // DataSize
            output[offset++] = (uint8_t)(body_len);       // DataSize
            output[offset++] = (uint8_t)(audio_ts >> 16); // Timestamp
            output[offset++] = (uint8_t)(audio_ts >> 8);  // Timestamp
            output[offset++] = (uint8_t)(audio_ts);       // Timestamp
            output[offset++] = (uint8_t)(audio_ts >> 24); // TimestampExtended
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)

            // flv AudioTagHeader

            // uint8_t header = gen_audio_tag_header(rtmp_xiecc->config);
            // RTMP_Log(RTMP_LOGINFO, "rtmp_sender_write_aac_audio_frame--config = 0: header=%X\n", header);

            output[offset++] = gen_audio_tag_header(rtmp_xiecc->config); // sound format aac
            output[offset++] = 0x00;                                     // aac sequence header

            // flv AudioTagBody --AudioSpecificConfig
            uint8_t audio_object_type = rtmp_xiecc->config.audio_object_type + 1;
            output[offset++] = (audio_object_type << 3) | (rtmp_xiecc->config.sample_frequency_index >> 1);
            output[offset++] = ((rtmp_xiecc->config.sample_frequency_index & 0x01) << 7) | (rtmp_xiecc->config.channel_configuration << 3);
            // no need to set pre_tag_size
            /*
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */

            // RTMP_Log(RTMP_LOGINFO, "rtmp_sender_write_aac_audio_frame--config = 0: audio_ts=%u\n", audio_ts);
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            free(output);
            rtmp_xiecc->audio_config_ok = 1;
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
        }
        else
        {
            body_len = 2 + adts_len - AAC_ADTS_HEADER_SIZE; // remove adts header + AudioTagHeader
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = 0x08;                      // tagtype audio
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(audio_ts >> 16); // time stamp
            output[offset++] = (uint8_t)(audio_ts >> 8);  // time stamp
            output[offset++] = (uint8_t)(audio_ts);       // time stamp
            output[offset++] = (uint8_t)(audio_ts >> 24); // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv AudioTagHeader
            //  uint8_t header = gen_audio_tag_header(rtmp_xiecc->config);
            //  printf("rtmp_sender_write_aac_audio_frame--config = 1: header=%X\n", header);

            output[offset++] = gen_audio_tag_header(rtmp_xiecc->config); // sound format aac
            output[offset++] = 0x01;                                     // aac raw data

            // flv AudioTagBody --raw aac data
            memcpy(output + offset, audio_frame + AAC_ADTS_HEADER_SIZE,
                   (adts_len - AAC_ADTS_HEADER_SIZE)); // H264 sequence parameter set

            // RTMP_Log(RTMP_LOGINFO, "rtmp_sender_write_aac_audio_frame--config != 0: audio_ts=%u\n", audio_ts);
            /*
            //previous tag size
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            offset += (adts_len - AAC_ADTS_HEADER_SIZE);
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            free(output);
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
        }
    } // end while 1
    return 0;
}

static uint32_t find_start_code(uint8_t *buf, uint32_t zeros_in_startcode)
{
    uint32_t info;
    uint32_t i;

    info = 1;
    if ((info = (buf[zeros_in_startcode] != 1) ? 0 : 1) == 0)
        return 0;

    for (i = 0; i < zeros_in_startcode; i++)
        if (buf[i] != 0)
        {
            info = 0;
            break;
        };

    return info;
}

/**
 * @brief Get the nal object (NAL, Network Abstraction Layer)
 * 
 * @param[out] len length between two start_codet
 * @param[out] offset position of the second start_code
 * @param[in] start buffer start
 * @param[in] total buffer length
 * @return uint8_t* position of the first start_code
 */
static uint8_t *get_nal(uint32_t *len, uint8_t **offset, uint8_t *start, uint32_t total)
{
    uint32_t info;
    uint8_t *q;
    uint8_t *p = *offset;
    *len = 0;

    if ((p - start) >= total)
        return NULL;

    while (1)
    {
        info = find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total)
            return NULL;
    }
    q = p + 4;
    p = q;
    while (1)
    {
        info = find_start_code(p, 3);
        if (info == 1)
            break;
        p++;
        if ((p - start) >= total)
            break;
    }

    *len = (p - q);
    *offset = p;
    return q;
}

int rtmp_sender_write_avc_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  int key)
{
    int ret;
    uint8_t *buf; // video data
    uint8_t *buf_offset;
    int total;
    uint32_t ts;
    uint32_t nal_len;
    uint32_t nal_len_pps;
    uint8_t *nal;
    uint8_t *nal_pps;
    char *output;
    uint32_t offset = 0;
    uint32_t body_len;
    uint32_t output_len;
    RTMP_XIECC *rtmp_xiecc;
    RTMP *rtmp;

    buf = data;
    buf_offset = data;
    total = size;
    ts = (uint32_t)dts_us;
    rtmp_xiecc = (RTMP_XIECC *)handle;
    if ((data == NULL) || (rtmp_xiecc == NULL))
    {
        return 1;
    }
    rtmp = rtmp_xiecc->rtmp;
    // printf("ts is %d, dts_us = %lu\n",ts, dts_us);

    while (1)
    {
        if (!RTMP_IsConnected(rtmp))
        {
            RTMP_Log(RTMP_LOGERROR, "video: connect failed");
            return -1;
        }
        // ts = RTMP_GetTime() - start_time;  //
        // by ssy
        offset = 0;
        nal = get_nal(&nal_len, &buf_offset, buf, total);
        if (nal == NULL)
            break;
        
        // SPS(Sequence Parameter Set) and PPS(Picture Parameter Set)
        if ((nal[0] & 0x1f) == 0x07)
        {
            if (rtmp_xiecc->video_config_ok > 0) {
                continue; // only send video sequence set one time
            }
            
            /* get pps */
            nal_pps = get_nal(&nal_len_pps, &buf_offset, buf, total);
            if (nal_pps == NULL) {
                RTMP_Log(RTMP_LOGERROR, "No Nal after PPS");
                break;
            }

            body_len = nal_len + nal_len_pps + 16;
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_KEY & 0xf0) | (FLV_CODECID_H264 & 0x0f); // key frame
            output[offset++] = 0x00; // avc sequence header
            output[offset++] = 0x00; // composite time
            output[offset++] = 0x00; // composite time
            output[offset++] = 0x00; // composite time

            // flv VideoTagBody -- AVCDecoderConfigurationRecord
            output[offset++] = 0x01;                    // configuration version
            output[offset++] = nal[1];                  // avc profile indication
            output[offset++] = nal[2];                  // profile compatibility
            output[offset++] = nal[3];                  // avc level indication
            output[offset++] = 0xff;                    // reserved + length size minus one, (0xFC | length size minus one)
            output[offset++] = 0xe1;                    // 0xE0 | num of SPS NALUs (usually 1)
            output[offset++] = (uint8_t)(nal_len >> 8); // sequence parameter set length high 8 bits
            output[offset++] = (uint8_t)(nal_len);      // sequence parameter set  length low 8 bits
            memcpy(output + offset, nal, nal_len);      // H264 sequence parameter set
            offset += nal_len;
            output[offset++] = 0x01;                            // num of PPS NALUs (usually 1)
            output[offset++] = (uint8_t)(nal_len_pps >> 8);     // picture parameter set length high 8 bits
            output[offset++] = (uint8_t)(nal_len_pps);          // picture parameter set length low 8 bits
            memcpy(output + offset, nal_pps, nal_len_pps);      // H264 picture parameter set

            // no need set pre_tag_size ,RTMP NO NEED
            //  flv test
            /*
            offset += nal_len_pps;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24);    //data len
            output[offset++] = (uint8_t)(fff >> 16);    //data len
            output[offset++] = (uint8_t)(fff >> 8);     //data len
            output[offset++] = (uint8_t)(fff);          //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);

            rtmp_xiecc->video_config_ok = 1;

            if (-1 == ret) {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }

        // I Frame, Instantaneous Decoder Refresh Frame
        if ((nal[0] & 0x1f) == 0x05)
        {
            body_len = nal_len + 5 + 4; // flv VideoTagHeader +  NALU length
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_KEY & 0xf0) | (FLV_CODECID_H264 & 0x0f); // key frame
            output[offset++] = 0x01; // avc NALU unit
            output[offset++] = 0x00; // composit time ??????????
            output[offset++] = 0x00; // composit time
            output[offset++] = 0x00; // composit time

            output[offset++] = (uint8_t)(nal_len >> 24); // nal length
            output[offset++] = (uint8_t)(nal_len >> 16); // nal length
            output[offset++] = (uint8_t)(nal_len >> 8);  // nal length
            output[offset++] = (uint8_t)(nal_len);       // nal length
            memcpy(output + offset, nal, nal_len);

            // no need set pre_tag_size ,RTMP NO NEED
            /*
            offset += nal_len;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */

            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }

        // P frame, Predictive Frame
        if ((nal[0] & 0x1f) == 0x01)
        {
            body_len = nal_len + 5 + 4; // flv VideoTagHeader +  NALU length
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_INTER & 0xf0) | (FLV_CODECID_H264 & 0x0f); // key frame
            output[offset++] = 0x01; // avc NALU unit
            output[offset++] = 0x00; // composit time ??????????
            output[offset++] = 0x00; // composit time
            output[offset++] = 0x00; // composit time

            output[offset++] = (uint8_t)(nal_len >> 24); // nal length
            output[offset++] = (uint8_t)(nal_len >> 16); // nal length
            output[offset++] = (uint8_t)(nal_len >> 8);  // nal length
            output[offset++] = (uint8_t)(nal_len);       // nal length
            memcpy(output + offset, nal, nal_len);

            // no need set pre_tag_size ,RTMP NO NEED
            /*
            offset += nal_len;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }
    }
    return 0;
}

/**
 * @brief Check the connection status of the rtmp_sender
 *
 * @param handle RTMP_XIECC handle
 * @return int status value, 1: connected, 0: not connected
 */
int rtmp_sender_isOK(void *handle)
{
    RTMP_XIECC *rtmp_xiecc;
    RTMP *rtmp;

    if (handle == NULL)
    {
        return 0;
    }
    rtmp_xiecc = (RTMP_XIECC *)handle;
    rtmp = rtmp_xiecc->rtmp;

    if (RTMP_IsConnected(rtmp))
    {
        return 1;
    }
    return 0;
}

static void bytestream_put(char *const stream, const uint32_t value, uint8_t length, uint32_t *offset)
{
    /* big-endian */
    for (int i = length - 1; i >= 0; i--) {
        *(stream + *offset) = (uint8_t)(value >> (8 * i));
        (*offset) += 1;
    }
}

static void libopus_write_header(char *const header_box, uint32_t *offset)
{
    OpusHeader opus_header;

    opus_header.version = 0x01;
    opus_header.output_channel_count = 0x02;
    opus_header.pre_skip = 0x0138;
    opus_header.input_sample_rate = 0x0000BB80;
    opus_header.output_gain = 0x0000;
    opus_header.channel_mapping_family = 0x00;
    // opus_header.frame_size = 0x01e0; // 480
    opus_header.frame_size = 0x03C0; // 960

    memcpy(header_box + *offset, "OpusHead", 8);
    *offset += 8;

    bytestream_put(header_box, opus_header.version, 1, offset);
    bytestream_put(header_box, opus_header.output_channel_count, 1, offset);
    bytestream_put(header_box, opus_header.pre_skip, 2, offset);
    bytestream_put(header_box, opus_header.input_sample_rate, 4, offset);
    bytestream_put(header_box, opus_header.output_gain, 2, offset);
    bytestream_put(header_box, opus_header.channel_mapping_family, 1, offset);
    bytestream_put(header_box, opus_header.frame_size, 2, offset);
}

/**
 * @brief send opus data to rtmp container
 *
 * @param [in] handle rtmp handle
 * @param [in] data opus frame data
 * @param [in] size length of opus frame data
 * @param [in] dts_us decode timestamp (us)
 * @param [in] start_time
 * @return int
 */
int rtmp_sender_write_opus_frame(void *handle,
                                 uint8_t *data,
                                 int size,
                                 uint64_t dts_us,
                                 uint32_t start_time)
{
    int ret;
    RTMP_XIECC *rtmp_xiecc = (RTMP_XIECC *)handle;
    RTMP *rtmp;
    uint32_t opus_ts = (uint32_t)dts_us;
    uint8_t *opus_frame = data;
    uint32_t opus_frame_len = size;
    uint32_t offset;
    uint32_t body_len;
    char *output;
    uint32_t output_len;
    int send_state = 0;
    // opus_ts = RTMP_GetTime() - start_time;
    if ((data == NULL) || (rtmp_xiecc == NULL))
    {
        return 1;
    }
    rtmp = rtmp_xiecc->rtmp;

    while (1)
    {
        if (!RTMP_IsConnected(rtmp))
        {
            RTMP_Log(RTMP_LOGERROR, "opus: connect failed");
            return -1;
        }
        // Audio OUTPUT
        offset = 0;

        if (rtmp_xiecc->audio_config_ok == 0)
        {
            body_len = 2 + OPUS_HEADER_SIZE; // FlvAudioTagHeader + OpusHeaderSize
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_AUDIO;    // TagType
            output[offset++] = (uint8_t)(body_len >> 16); // DataSize
            output[offset++] = (uint8_t)(body_len >> 8);  // DataSize
            output[offset++] = (uint8_t)(body_len);       // DataSize
            output[offset++] = (uint8_t)(opus_ts >> 16);  // Timestamp
            output[offset++] = (uint8_t)(opus_ts >> 8);   // Timestamp
            output[offset++] = (uint8_t)(opus_ts);        // Timestamp
            output[offset++] = (uint8_t)(opus_ts >> 24);  // TimestampExtended
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)

            // flv AudioTagHeader
            // output[offset++] = 0xDF;
            output[offset++] = FLV_CODECID_OPUS | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
            output[offset++] = 0x00; // opus sequence header

            // flv AudioTagBody -- OpusHeader
            libopus_write_header(output, &offset);

            // for (int i = 0; i < offset; i++)
            // {
            //     RTMP_Log(RTMP_LOGDEBUG, "output = %d\n", output[i]);
            // }

            // RTMP_Log(RTMP_LOGDEBUG, "write opus header-- opus_ts=%u\n", opus_ts);
            // no need to set pre_tag_size
            /*
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);
            free(output);
            rtmp_xiecc->audio_config_ok = 1;
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
        }
        else
        {
            body_len = 2 + opus_frame_len; // remove adts header + AudioTagHeader
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_AUDIO;    // TagType
            output[offset++] = (uint8_t)(body_len >> 16); // DataSize
            output[offset++] = (uint8_t)(body_len >> 8);  // DataSize
            output[offset++] = (uint8_t)(body_len);       // DataSize
            output[offset++] = (uint8_t)(opus_ts >> 16);  // Timestamp
            output[offset++] = (uint8_t)(opus_ts >> 8);   // Timestamp
            output[offset++] = (uint8_t)(opus_ts);        // Timestamp
            output[offset++] = (uint8_t)(opus_ts >> 24);  // TimestampExtended
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)
            output[offset++] = 0x00;                      // StreamID (Always 0)

            // flv AudioTagHeader
            // output[offset++] = 0xDF;
            output[offset++] = FLV_CODECID_OPUS | FLV_SAMPLERATE_44100HZ | FLV_SAMPLESSIZE_16BIT | FLV_STEREO;
            output[offset++] = 0x01; // opus raw data

            // flv AudioTagBody --raw opus data
            memcpy(output + offset, opus_frame, opus_frame_len);
            offset += opus_frame_len;

            // RTMP_Log(RTMP_LOGDEBUG, "write raw opus data-- opus_ts=%u\n", opus_ts);
            /* //previous tag size
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            offset += (opus_box_len - AAC_ADTS_HEADER_SIZE);
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len */

            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            free(output);
            send_state = 1;
            if (-1 == ret)
            {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
        }

        // if the raw opus data has been completely sent, exit
        if (send_state == 1)
            break;
    } // end while 1
    return 0;
}

int rtmp_sender_write_hevc_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  int key)
{
    int ret;
    uint8_t *buf; // video data
    uint8_t *buf_offset;
    int total;
    uint32_t ts;
    uint32_t nal_len;
    uint32_t nal_len_sps;
    uint32_t nal_len_pps;
    uint8_t *nal;
    uint8_t *nal_sps;
    uint8_t *nal_pps;
    char *output;
    uint32_t offset = 0;
    uint32_t body_len;
    uint32_t output_len;
    RTMP_XIECC *rtmp_xiecc;
    RTMP *rtmp;

    buf = data;
    buf_offset = data;
    total = size;
    ts = (uint32_t)dts_us;
    rtmp_xiecc = (RTMP_XIECC *)handle;
    if ((data == NULL) || (rtmp_xiecc == NULL)) {
        return 1;
    }
    rtmp = rtmp_xiecc->rtmp;
    // printf("ts is %d, dts_us = %lu\n",ts, dts_us);

    while (1) {
        if (!RTMP_IsConnected(rtmp)) {
            RTMP_Log(RTMP_LOGERROR, "video: connect failed");
            return -1;
        }
        // ts = RTMP_GetTime() - start_time;  //
        // by ssy
        offset = 0;
        nal = get_nal(&nal_len, &buf_offset, buf, total);
        if (nal == NULL)
            break;
        
        // VPS(Video parameter set), SPS(Sequence Parameter Set) and PPS(Picture Parameter Set)
        if (((nal[0] >> 1) & 0x3f) == 0x20) {
            if (rtmp_xiecc->video_config_ok > 0) {
                continue; // only send video sequence set one time
            }

            /* Get SPS */
            nal_sps = get_nal(&nal_len_sps, &buf_offset, buf, total);
            if (nal_sps == NULL) {
                RTMP_Log(RTMP_LOGERROR, "No Nal after SPS");
                break;
            }
            
            /* Get PPS */
            nal_pps = get_nal(&nal_len_pps, &buf_offset, buf, total);
            if (nal_pps == NULL) {
                RTMP_Log(RTMP_LOGERROR, "No Nal after PPS");
                break;
            }

            body_len = nal_len + nal_len_sps + nal_len_pps + 5 + 23 + 15;
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            memset(output, 0, output_len);

            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_KEY & 0xf0) | (FLV_CODECID_HEVC & 0x0f); // key frame
            output[offset++] = 0x00; // avc sequence header
            output[offset++] = 0x00; // composite time
            output[offset++] = 0x00; // composite time
            output[offset++] = 0x00; // composite time

            // flv VideoTagBody --HEVCDecoderConfigurationRecord
            output[offset++] = 0x01;                        // configuration version, (always 0x01)
            output[offset++] = 0x01;                        // hevc profile indication

            output[offset++] = 0x60;
            output[offset++] = 0x00;
            output[offset++] = 0x00;
            output[offset++] = 0x00;

            output[offset++] = 0x90;
            output[offset++] = 0x00;
            output[offset++] = 0x00;
            output[offset++] = 0x00;
            output[offset++] = 0x00;
            output[offset++] = 0x00;

            output[offset++] = 0x5A;

            output[offset++] = 0xF0;
            output[offset++] = 0x00;
            output[offset++] = 0xFC;
            output[offset++] = 0xFD;
            output[offset++] = 0xF8;
            output[offset++] = 0xF8;

            output[offset++] = 0x00;
            output[offset++] = 0x00;

            output[offset++] = 0x0F;

            output[offset++] = 0x03;

            output[offset++] = 0x20;
            output[offset++] = (0x01 >> 8) & 0xff;
            output[offset++] = 0x01 & 0xff;
            output[offset++] = (uint8_t)(nal_len >> 8);
            output[offset++] = (uint8_t)(nal_len);
            memcpy(output + offset, nal, nal_len);
            offset += nal_len;

            output[offset++] = 0x21;
            output[offset++] = (0x01 >> 8) & 0xff;
            output[offset++] = 0x01 & 0xff;
            output[offset++] = (uint8_t)(nal_len_sps >> 8);
            output[offset++] = (uint8_t)(nal_len_sps);
            memcpy(output + offset, nal_sps, nal_len_sps);
            offset += nal_len_sps;


            output[offset++] = 0x22;
            output[offset++] = (0x01 >> 8) & 0xff;
            output[offset++] = 0x01 & 0xff;
            output[offset++] = (uint8_t)(nal_len_pps >> 8);
            output[offset++] = (uint8_t)(nal_len_pps);
            memcpy(output + offset, nal_pps, nal_len_pps);
            offset += nal_len_pps;

            // no need set pre_tag_size ,RTMP NO NEED
            //  flv test
            /*
            offset += nal_len_pps;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24);    //data len
            output[offset++] = (uint8_t)(fff >> 16);    //data len
            output[offset++] = (uint8_t)(fff >> 8);     //data len
            output[offset++] = (uint8_t)(fff);          //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);
            rtmp_xiecc->video_config_ok = 1;
            if (-1 == ret) {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }

        // I Frame, Instantaneous Decoder Refresh Frame
        if (((nal[0] >> 1) & 0x3f) == 0x13) {
            body_len = nal_len + 5 + 4; // flv VideoTagHeader +  NALU length
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_KEY & 0xf0) | (FLV_CODECID_HEVC & 0x0f); // key frame
            output[offset++] = 0x01; // avc NALU unit
            output[offset++] = 0x00; // composit time ??????????
            output[offset++] = 0x00; // composit time
            output[offset++] = 0x00; // composit time

            output[offset++] = (uint8_t)(nal_len >> 24); // nal length
            output[offset++] = (uint8_t)(nal_len >> 16); // nal length
            output[offset++] = (uint8_t)(nal_len >> 8);  // nal length
            output[offset++] = (uint8_t)(nal_len);       // nal length
            memcpy(output + offset, nal, nal_len);

            // no need set pre_tag_size ,RTMP NO NEED
            /*
            offset += nal_len;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */

            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);
            if (-1 == ret) {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }

        // P frame, Predictive Frame
        if (((nal[0] >> 1) & 0x3f) == 0x01) {
            body_len = nal_len + 5 + 4; // flv VideoTagHeader +  NALU length
            output_len = body_len + RTMP_MESSAGE_HEADER_LEN + FLV_PRE_TAG_LEN;
            output = malloc(output_len);
            // Message Header
            output[offset++] = RTMP_PACKET_TYPE_VIDEO;    // tagtype video
            output[offset++] = (uint8_t)(body_len >> 16); // data len
            output[offset++] = (uint8_t)(body_len >> 8);  // data len
            output[offset++] = (uint8_t)(body_len);       // data len
            output[offset++] = (uint8_t)(ts >> 16);       // time stamp
            output[offset++] = (uint8_t)(ts >> 8);        // time stamp
            output[offset++] = (uint8_t)(ts);             // time stamp
            output[offset++] = (uint8_t)(ts >> 24);       // time stamp
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0
            output[offset++] = 0x00;                      // stream id 0

            // flv VideoTagHeader
            output[offset++] = (FLV_FRAME_INTER & 0xf0) | (FLV_CODECID_HEVC & 0x0f); // key frame
            output[offset++] = 0x01; // avc NALU unit
            output[offset++] = 0x00; // composit time ??????????
            output[offset++] = 0x00; // composit time
            output[offset++] = 0x00; // composit time

            output[offset++] = (uint8_t)(nal_len >> 24); // nal length
            output[offset++] = (uint8_t)(nal_len >> 16); // nal length
            output[offset++] = (uint8_t)(nal_len >> 8);  // nal length
            output[offset++] = (uint8_t)(nal_len);       // nal length
            memcpy(output + offset, nal, nal_len);

            // no need set pre_tag_size ,RTMP NO NEED
            /*
            offset += nal_len;
            uint32_t fff = body_len + RTMP_MESSAGE_HEADER_LEN;
            output[offset++] = (uint8_t)(fff >> 24); //data len
            output[offset++] = (uint8_t)(fff >> 16); //data len
            output[offset++] = (uint8_t)(fff >> 8); //data len
            output[offset++] = (uint8_t)(fff); //data len
            */
            ret = RTMP_Write(rtmp, output, output_len);
            // write_data_to_flv_file(&output, output_len);

            // RTMP Send out
            free(output);
            if (-1 == ret) {
                RTMP_Log(RTMP_LOGERROR, "%s: RTMP_Write failed!", __FUNCTION__);
                return -1;
            }
            continue;
        }
    }
    return 0;
}
