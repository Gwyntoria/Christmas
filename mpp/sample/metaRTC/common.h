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

// LOG system
#define LOG_LEVEL 0x0F

#define LOG_LVL_ERROR 0x01
#define LOG_LVL_WARN  0x02
#define LOG_LVL_INFO  0x04
#define LOG_LVL_DEBUG 0x08

void LogTcpRtp(char *p_fmt, ...);

#if (LOG_LEVEL & LOG_LVL_INFO)
#define LOGI(format, ...) \
    do { \
        fprintf(stderr, "INFO--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        LogTcpRtp("INFO--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGI(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_ERROR)
#define LOGE(format, ...) \
    do { \
        fprintf(stderr, "ERROR--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        LogTcpRtp("ERROR--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGE(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_WARN)
#define LOGW(format, ...) \
    do { \
        fprintf(stderr, "WARNING--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        LogTcpRtp("WARNING--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGW(format, ...) \
    do { } while (0)
#endif

#if (LOG_LEVEL & LOG_LVL_DEBUG)
#define LOGD(format, ...) \
    do { \
        fprintf(stderr, "DEBUG--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__ ); \
        LogTcpRtp("DEBUG--%s -%4d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define LOGD(format, ...) \
    do { } while (0)
#endif

#define WORK_FOLDER     "/root/WaController"

typedef struct LOTO_ROOM_INFO
{
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

}loto_room_info;

typedef struct Msg {
	int flag;//0为读，1为写
	char pushurl[256];
    int  isopen;
}Loto_Msg;


char*   log_Time(void);
int     int2string(long long value, char * output);
long long string2int(const char *str);
long long get_timestamp(char* pszTS, int isMSec);

int InitTCpRtpLog();


#ifdef __cplusplus
}
#endif

#endif