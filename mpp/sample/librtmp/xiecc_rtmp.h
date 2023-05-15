#ifndef _XIECC_RTMP_H_
#define _XIECC_RTMP_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// @brief alloc function
// @param [in] url     : RTMP URL, rtmp://127.0.0.1/live/xxx
// @return             : rtmp_sender handler
void *rtmp_sender_alloc(const char *url); // return handle

// @brief free rtmp_sender handler
// @param [in] rtmp_sender handler
void rtmp_sender_free(void *handle);

// @brief start publish
// @param [in] rtmp_sender handler
// @param [in] flag        stream falg
// @param [in] ts_us       timestamp in us
// @return             : 0: OK; others: FAILED
int rtmp_sender_start_publish(void *handle, uint32_t flag, int64_t ts_us);

// @brief stop publish
// @param [in] rtmp_sender handler
// @return             : 0: OK; others: FAILED
int rtmp_sender_stop_publish(void *handle);

// @brief send aac frame
// @param [in] rtmp_sender handler
// @param [in] data       : aac audio data
// @param [in] size       : aac audio data size
// @param [in] dts_us     : decode timestamp of frame
int rtmp_sender_write_aac_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  uint32_t start_time);

/**
 * @brief send avc(h.264) frame
 * 
 * @param handle rtmp_sender handler
 * @param data video data, (Full frames are required)
 * @param size video data size
 * @param dts_us timestamp
 * @param key 
 * @return int 
 */
int rtmp_sender_write_avc_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  int key);


/**
 * @brief Check the connection status of the rtmp_sender
 *
 * @param handle RTMP_XIECC handle
 * @return int status value, 1: connected, 0: not connected
 */
int rtmp_sender_isOK(void *handle);

/**
 * @brief 
 * 
 * @param handle 
 * @param data opus frame data
 * @param size length of opus frame data
 * @param dts_us decode timestamp (us)
 * @param start_time 
 * @return int 
 */
int rtmp_sender_write_opus_frame(void *handle,
                                 uint8_t *data,
                                 int size,
                                 uint64_t dts_us,
                                 uint32_t start_time);

/**
 * @brief send hevc(h.265) frame
 * 
 * @param handle rtmp_sender
 * @param data frame
 * @param size frame size
 * @param dts_us timestamp
 * @param key 
 * @return int 
 */
int rtmp_sender_write_hevc_frame(void *handle,
                                  uint8_t *data,
                                  int size,
                                  uint64_t dts_us,
                                  int key);

#ifdef __cplusplus
}
#endif
#endif
