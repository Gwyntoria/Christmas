/*
 * Common.h:
 *
 ***********************************************************************
 * by Jessica Mao
 * Lotogram Inc,. 2020/05/18
 *
 ***********************************************************************
 */

#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>

/* LOG system */
#define LOG_LEVEL 0x0f

#define LOG_LVL_ERROR   0x01
#define LOG_LVL_WARN    0x02
#define LOG_LVL_INFO    0x04
#define LOG_LVL_DEBUG   0x08

/**
 * @brief Get the string typed time 
 * 
 * @return char* the formated time string
 */
char*   GetLocalTime(void);

void WriteLogFile(char *p_fmt, ...);

#if (LOG_LEVEL & LOG_LVL_INFO)
#define LOGI(format, ...) \
    do { \
        fprintf(stderr, "[INF]-[%s]-[%d]: " format "", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        WriteLogFile("[%s]-[INF]-[%s]-[%d]: " format "", GetLocalTime(), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGI(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_ERROR)
#define LOGE(format, ...) \
    do { \
        fprintf(stderr, "[ERR]-[%s]-[%d]: " format "", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        WriteLogFile("[%s]-[ERR]-[%s]-[%d]: " format "", GetLocalTime(), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGE(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_WARN)
#define LOGW(format, ...) \
    do { \
        fprintf(stderr, "[WAR]-[%s]-[%d]: " format "", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        WriteLogFile("[%s]-[WAR]-[%s]-[%d]: " format "", GetLocalTime(), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGW(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_DEBUG)
#define LOGD(format, ...) \
    do { \
        fprintf(stderr, "[DEB]-[%s]-[%d]: " format "", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        WriteLogFile("[%s]-[DEB]-[%s]-[%d]: " format "", GetLocalTime(), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGD(format, ...) \
    do { } while (0)
#endif

#define WORK_FOLDER     "/root"

typedef struct RoomInfo {
    int 	iCode;

    int		iRoomType;
    int 	iWSType;
    long 	lServerTS;
    long    lBoardTS;

    char	szRoomId[64];
    char    szCId[32];
    char	szWebsocket[128];
    char	szNonce[128];
    char	szPushURL[512];
    char	szMac[32];
    char    szCodeckey[128];

}RoomInfo;

typedef struct LOTO_ROOM_INFO
{
    char	szStatus[32];
    int 	iCode;

    int		iRoomType;
    int 	iSubType;
    int		iVip;
    int		iJP;
    int		iMatchType;
    int		iEncodeType;
    int 	iPlayerCount;
    int     iSupportMonster;

    char	szRoomId[64];
    char	szDomain[32];
    char	szName[32];
    char	szHostName[32];
    char	szRouterIp[32];
    char	szWebsocket[64];
    char	szIp[32];
    char	szRouterDNS[128];
    char	szFrpPort[32];
    char	szPushURL[256];
    char    szScreenShot[256];
} loto_room_info;

typedef struct Msg {
    int flag;//0为读，1为写
    char pushurl[256];
    int  is_open;
}Loto_Msg;

typedef struct LotoLogHandle {
    FILE *rtmp_handle;
    FILE *avctl_handle;
} LogHandle;


/**
 * @brief Initialize avctl_log file. 
 * 
 * @return int status value, 0: success, -1: failure
 */
int InitAvctlLogFile();

/**
 * @brief Initialize the rtmp_log file. 
 * 
 * @param log_handle rtmp_log file handle. 
 * @return int status value, 0: success, -1: failure
 */
int InitRtmpLogFile(FILE **log_handle);

int int2string(long long value, char * output);

long long string2int(const char *str);

/**
 * @brief Get the Timestamp object
 * 
 * @param pszTS [out] the pointer to the formated string of time
 * @param isMSec [in] [1: ms], [0: s]
 * @return uint64_t time 
 */
uint64_t GetTimestamp(char* pszTS, int isMSec);

int get_hash_code_24(char* psz_combined_string);

char* encode(char* message, const char* codeckey);
char* decode(char* message, const char* codeckey);

static int base64_encode(const unsigned char *sourcedata, int datalength, char *base64);

int get_mac(char *mac);

#ifdef __cplusplus
}
#endif

#endif