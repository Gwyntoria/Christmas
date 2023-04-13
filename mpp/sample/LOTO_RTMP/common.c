/*
 * Common.c:
 *
 * By Jessica Mao 2020/05/18
 *
 * Copyright (c) 2012-2020 Lotogram Inc. <lotogram.com, zhuagewawa.com>

 * Version 1.0.0.73	Details in update.log
 ***********************************************************************
 */
#include "common.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

#define LOG_FOLDER "/root/log"
#define LOG_AVCTL_FILE "avctl_log"
#define LOG_RTMP_FILE "rtmp_log"
#define MAX_FILE_SIZE (5 * 1024 * 1024)
#define MAX_FILE_COUNT 5
// #define NTP_TIMESTAMP_DELTA 2208988800ull // NTP 时间戳的起始时间（1900 年 1 月 1 日）

static FILE *log_handle = NULL;
static pthread_mutex_t _vLogMutex;

char *GetLocalTime(void)
{
    static char time[32] = {0};

    // 获取当前系统时间
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // 将时间转换为本地时间
    struct tm *lt = localtime(&tv.tv_sec);

    // 格式化输出本地时间（精确到毫秒）
    snprintf(time, sizeof(time), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
             lt->tm_hour, lt->tm_min, lt->tm_sec, tv.tv_usec / 1000);
    return time;
}

long long string2int(const char *str)
{
    char flag = '+'; //指示结果是否带符号
    long long res = 0;

    if (*str == '-') //字符串带负号
    {
        ++str;      //指向下一个字符
        flag = '-'; //将标志设为负号
    }
    //逐个字符转换，并累加到结果res
    while (*str >= 48 && *str <= 57) //如果是数字才进行转换，数字0~9的ASCII码：48~57
    {
        res = 10 * res + *str++ - 48; //字符'0'的ASCII码为48,48-48=0刚好转化为数字0
    }

    if (flag == '-') //处理是负数的情况
    {
        res = -res;
    }

    return res;
}

int string_reverse(char *strSrc)
{
    int len = 0;
    int i = 0;
    char *output = NULL;
    char *pstr = strSrc;
    while (*pstr)
    {
        pstr++;
        len++;
    }
    output = (char *)malloc(len);
    if (output == NULL)
    {
        LOGE("malloc");
        return -1;
    }
    for (i = 0; i < len; i++)
    {
        output[i] = strSrc[len - i - 1];
        // LOGE("output[%d] = %c\n",len - i -1,strSrc[len - i - 1]);
    }
    output[len] = '\0';
    strcpy(strSrc, output);
    free(output);
    return 0;
}

int int2string(long long value, char *output)
{
    int index = 0;
    if (value == 0)
    {
        output[0] = value + '0';
        return 1;
    }
    else
    {
        while (value)
        {
            output[index] = value % 10 + '0';
            index++;
            value /= 10;
        }
        string_reverse(output);
        return 1;
    }
}

uint64_t GetTimestamp(char *pszTS, int isMSec)
{
    uint64_t timestamp;
    char szT[64] = "";
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (isMSec > 0)
        timestamp = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    else
        timestamp = (uint64_t)tv.tv_sec;

    if (pszTS != NULL)
    {
        int2string((int64_t)timestamp, szT);
        strcpy(pszTS, szT);
    }

    return timestamp;
}

int InitAvctlLogFile()
{
    char log[256];

    mkdir(LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", LOG_FOLDER, LOG_AVCTL_FILE);
    log_handle = fopen((char *)log, "a+");
    if (log_handle)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int InitRtmpLogFile(FILE **log_handle)
{
    char log[256];

    mkdir(LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", LOG_FOLDER, LOG_RTMP_FILE);
    *log_handle = fopen((char *)log, "a+");
    if (*log_handle)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

static long _getfilesize(FILE *stream)
{
    long curpos, length;
    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);
    fseek(stream, curpos, SEEK_SET);
    return length;
}

static int _rebuildLogFiles()
{
    char tmp[256];
    char tmp2[256];

    if (log_handle)
    {
        fclose(log_handle);

        for (int i = (MAX_FILE_COUNT - 1); i > 0; i--)
        {
            snprintf(tmp, sizeof(tmp), "%s/%s.%d", LOG_FOLDER, LOG_AVCTL_FILE, i);
            snprintf(tmp2, sizeof(tmp), "%s/%s.%d", LOG_FOLDER, LOG_AVCTL_FILE, i + 1);
            if ((access(tmp, F_OK)) != -1)
            {
                remove(tmp2);
                rename(tmp, tmp2);
            }
        }

        snprintf(tmp, sizeof(tmp), "%s/%s", LOG_FOLDER, LOG_AVCTL_FILE);
        snprintf(tmp2, sizeof(tmp), "%s/%s.1", LOG_FOLDER, LOG_AVCTL_FILE);

        remove(tmp2);
        rename(tmp, tmp2);

        log_handle = fopen((char *)tmp, "a");
    }

    return 1;
}

void WriteLogFile(char *p_fmt, ...)
{
    va_list ap;

    if (!log_handle)
    {
        return;
    }

    pthread_mutex_lock(&_vLogMutex);
    va_start(ap, p_fmt);
    vfprintf(log_handle, p_fmt, ap);
    va_end(ap);

    fflush(log_handle);

    long file_size = _getfilesize(log_handle);
    if (file_size >= MAX_FILE_SIZE)
    {
        _rebuildLogFiles();
    }

    pthread_mutex_unlock(&_vLogMutex);
}


#define NTP_PORT 123
#define NTP_PACKET_SIZE 48
#define NTP_UNIX_OFFSET 2208988800
#define RESEND_INTERVAL 3
#define MAX_RETRIES 5
#define TIMEOUT_SEC 5

int time_sync() {
    LOGI("=== time_sync ===\n");
    
    char *ntp_server = "ntp1.aliyun.com";

    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == -1) {
        LOGE("create socket failed\n");
        return -1;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    // LOGI("create udp_socket success!\n");

    struct hostent *server = gethostbyname(ntp_server);
    if (server == NULL) {
        LOGE("could not resolve %s\n", ntp_server);
        return -1;
    }

    // LOGI("gethostbyname success!\n");

    // Populate the server information
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(NTP_PORT);

    // Construct NTP packets
    uint8_t ntp_packet[NTP_PACKET_SIZE];
    memset(ntp_packet, 0, sizeof(ntp_packet));
    ntp_packet[0] = 0b11100011; // NTP version 4, client mode, no leap indicator
    ntp_packet[1] = 0; // stratum, or how far away the server is from a reliable time source
    ntp_packet[2] = 6; // poll interval, or how often the client will request time
    ntp_packet[3] = 0xEC; // precision, or how accurate the client's clock is
    // the rest of the packet is all zeros

    int retries = 0;
    uint8_t ntp_response[NTP_PACKET_SIZE];
    socklen_t addr_len = sizeof(server_addr); 

    while (retries < MAX_RETRIES) {
        ssize_t bytes_sent = sendto(sock_fd, ntp_packet, sizeof(ntp_packet), 0,
                                    (struct sockaddr *) &server_addr, addr_len);
        if (bytes_sent < 0) {
            LOGE("sendto failed\n");
            break;
        }

        // LOGI("send ntp_packet success!\n");

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        // Waiting for data return or timeout
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int ret = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret == -1) {
            LOGE("select error");
            exit(1);
        } else if (ret == 0) {
            LOGE("timeout, retry.....\n");
            retries++;
            continue;
        }

        memset(ntp_response, 0, sizeof(ntp_response));
        ssize_t bytes_received = recvfrom(sock_fd, ntp_response, sizeof(ntp_response), 0, 
                                            (struct sockaddr *) &server_addr, &addr_len);
        if (bytes_received < 0) {
            if (errno == EINTR) {
                LOGE("No data available; retrying...\n");
                retries++;
                continue;
            } else {
                LOGE("recv error; retrying...\n");
                retries++;
                continue;
            }
        }

        // LOGI("receive ntp_packet success!\n");
        break;
        
    }

    close(sock_fd);

    if (retries == MAX_RETRIES) {
        LOGI("Maximum number of retries reached; giving up.\n");
        return -1;
    }

    uint32_t ntp_time = ntohl(*(uint32_t *) (ntp_response + 40));
    time_t unix_time = ntp_time - NTP_UNIX_OFFSET;
    LOGI("Time from %s: %s", ntp_server, ctime(&unix_time));

    if (settimeofday(&(struct timeval){.tv_sec = unix_time}, NULL) == -1) {
        LOGE("settimeofday failed\n");
        return -1;
    }

    // LOGI("set time success!\n");

    return 0;
}