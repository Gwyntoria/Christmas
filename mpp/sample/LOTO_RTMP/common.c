/**
 * @file common.c
 * @author Karl Meng (karlmfloating@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-05-18
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define LOG_FOLDER     "/root/log"
#define LOG_AVCTL_FILE "avctl_log"
#define LOG_RTMP_FILE  "rtmp_log"
#define MAX_FILE_SIZE  (5 * 1024 * 1024)
#define MAX_FILE_COUNT 5

static FILE*           log_handle = NULL;
static pthread_mutex_t _vLogMutex;

char* GetTimestampString(void) {
    struct tm*   ptm;
    struct timeb stTimeb;
    static char  szTime[32] = {0};

    ftime(&stTimeb);
    ptm = localtime(&stTimeb.time);
    sprintf(szTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec, stTimeb.millitm);
    // szTime[23] = 0;
    return szTime;
}

uint64_t GetTimestampU64(char* pszTS, int isMSec) {
    uint64_t       timestamp;
    char           szT[64] = "";
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (isMSec)
        timestamp = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    else
        timestamp = (uint64_t)tv.tv_sec;

    if (pszTS != NULL) {
        int2string((int64_t)timestamp, szT);
        strcpy(pszTS, szT);
    }

    return timestamp;
}

uint64_t get_us_timestamp() {
    uint64_t       timestamp;
    char           szT[64] = "";
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timestamp = (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;

    return timestamp;
}

int get_mac(char* mac) {
    int          sockfd;
    struct ifreq tmp;
    char         mac_addr[30];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("create socket fail\n");
        return -1;
    }

    memset(&tmp, 0, sizeof(struct ifreq));
    strncpy(tmp.ifr_name, "eth0", sizeof(tmp.ifr_name) - 1);
    if ((ioctl(sockfd, SIOCGIFHWADDR, &tmp)) < 0) {
        printf("mac ioctl error\n");
        return -1;
    }

    sprintf(mac_addr, "%02x%02x%02x%02x%02x%02x", (unsigned char)tmp.ifr_hwaddr.sa_data[0],
            (unsigned char)tmp.ifr_hwaddr.sa_data[1], (unsigned char)tmp.ifr_hwaddr.sa_data[2],
            (unsigned char)tmp.ifr_hwaddr.sa_data[3], (unsigned char)tmp.ifr_hwaddr.sa_data[4],
            (unsigned char)tmp.ifr_hwaddr.sa_data[5]);
    printf("MAC: %s\n", mac_addr);
    close(sockfd);
    memcpy(mac, mac_addr, strlen(mac_addr));

    return 0;
}

long long string2int(const char* str) {
    char      flag = '+'; // 指示结果是否带符号
    long long res  = 0;

    if (*str == '-') // 字符串带负号
    {
        ++str;      // 指向下一个字符
        flag = '-'; // 将标志设为负号
    }
    // 逐个字符转换，并累加到结果res
    while (*str >= 48 && *str <= 57) // 如果是数字才进行转换，数字0~9的ASCII码：48~57
    {
        res = 10 * res + *str++ - 48; // 字符'0'的ASCII码为48,48-48=0刚好转化为数字0
    }

    if (flag == '-') // 处理是负数的情况
    {
        res = -res;
    }

    return res;
}

int string_reverse(char* strSrc) {
    int   len    = 0;
    int   i      = 0;
    char* output = NULL;
    char* pstr   = strSrc;
    while (*pstr) {
        pstr++;
        len++;
    }
    output = (char*)malloc(len);
    if (output == NULL) {
        perror("malloc");
        return -1;
    }
    for (i = 0; i < len; i++) {
        output[i] = strSrc[len - i - 1];
        // printf("output[%d] = %c\n",len - i -1,strSrc[len - i - 1]);
    }
    output[len] = '\0';
    strcpy(strSrc, output);
    free(output);
    return 0;
}

int int2string(long long value, char* output) {
    int index = 0;
    if (value == 0) {
        output[0] = value + '0';
        return 1;
    } else {
        while (value) {
            output[index] = value % 10 + '0';
            index++;
            value /= 10;
        }
        string_reverse(output);
        return 1;
    }
}

int InitAvctlLogFile() {
    char log[256];

    mkdir(LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", LOG_FOLDER, LOG_AVCTL_FILE);
    log_handle = fopen((char*)log, "a+");
    if (log_handle) {
        return 0;
    } else {
        return -1;
    }
}

int InitRtmpLogFile(FILE** log_handle) {
    char log[256];

    mkdir(LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", LOG_FOLDER, LOG_RTMP_FILE);
    *log_handle = fopen((char*)log, "a+");
    if (*log_handle) {
        return 0;
    } else {
        return -1;
    }
}

static long _getfilesize(FILE* stream) {
    long curpos, length;
    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);
    fseek(stream, curpos, SEEK_SET);
    return length;
}

static int _rebuildLogFiles() {
    char tmp[256];
    char tmp2[256];

    if (log_handle) {
        fclose(log_handle);

        for (int i = (MAX_FILE_COUNT - 1); i > 0; i--) {
            snprintf(tmp, sizeof(tmp), "%s/%s.%d", LOG_FOLDER, LOG_AVCTL_FILE, i);
            snprintf(tmp2, sizeof(tmp), "%s/%s.%d", LOG_FOLDER, LOG_AVCTL_FILE, i + 1);
            if ((access(tmp, F_OK)) != -1) {
                remove(tmp2);
                rename(tmp, tmp2);
            }
        }

        snprintf(tmp, sizeof(tmp), "%s/%s", LOG_FOLDER, LOG_AVCTL_FILE);
        snprintf(tmp2, sizeof(tmp), "%s/%s.1", LOG_FOLDER, LOG_AVCTL_FILE);

        remove(tmp2);
        rename(tmp, tmp2);

        log_handle = fopen((char*)tmp, "a");
    }

    return 1;
}

void WriteLogFile(char* p_fmt, ...) {
    va_list ap;

    if (!log_handle) {
        return;
    }

    pthread_mutex_lock(&_vLogMutex);
    va_start(ap, p_fmt);
    vfprintf(log_handle, p_fmt, ap);
    va_end(ap);

    fflush(log_handle);

    long file_size = _getfilesize(log_handle);
    if (file_size >= MAX_FILE_SIZE) {
        _rebuildLogFiles();
    }

    pthread_mutex_unlock(&_vLogMutex);
}

int get_hash_code_24(char* psz_combined_string) {
    int hash_code = 0;
    if (psz_combined_string == NULL || strlen(psz_combined_string) <= 0)
        return 0;

    uint i = 0;
    for (i = 0; i < strlen(psz_combined_string); i++) {
        hash_code = ((hash_code << 5) - hash_code) + (int)psz_combined_string[i];
        hash_code = hash_code & 0x00FFFFFF;
    }
    return hash_code;
}

const char* base64char   = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const char  padding_char = '=';

static int base64_encode(const unsigned char* sourcedata, int datalength, char* base64) {
    int           i = 0, j = 0;
    unsigned char trans_index = 0; // 索引是8位，但是高两位都为0
    // const int datalength = strlen((const char*)sourcedata);
    for (; i < datalength; i += 3) {
        // 每三个一组，进行编码
        // 要编码的数字的第一个
        trans_index = ((sourcedata[i] >> 2) & 0x3f);
        base64[j++] = base64char[(int)trans_index];
        // 第二个
        trans_index = ((sourcedata[i] << 4) & 0x30);
        if (i + 1 < datalength) {
            trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
            base64[j++] = base64char[(int)trans_index];
        } else {
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            base64[j++] = padding_char;

            break; // 超出总长度，可以直接break
        }
        // 第三个
        trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
        if (i + 2 < datalength) { // 有的话需要编码2个
            trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
            base64[j++] = base64char[(int)trans_index];

            trans_index = sourcedata[i + 2] & 0x3f;
            base64[j++] = base64char[(int)trans_index];
        } else {
            base64[j++] = base64char[(int)trans_index];

            base64[j++] = padding_char;

            break;
        }
    }
    base64[j] = '\0';
    return 0;
}

/** 在字符串中查询特定字符位置索引
 * const char *str ，字符串
 * char c，要查找的字符
 */
static int num_strchr(const char* str, char c) //
{
    const char* pindex = strchr(str, c);
    if (NULL == pindex) {
        return -1;
    }
    return pindex - str;
}

/* 解码
 * const char * base64 码字
 * unsigned char * dedata， 解码恢复的数据
 */
static int base64_decode(const char* base64, unsigned char* dedata) {
    int i = 0, j = 0;
    int trans[4] = {0, 0, 0, 0};
    for (; base64[i] != '\0'; i += 4) {
        // 每四个一组，译码成三个字符
        trans[0] = num_strchr(base64char, base64[i]);
        trans[1] = num_strchr(base64char, base64[i + 1]);
        // 1/3
        dedata[j++] = ((trans[0] << 2) & 0xfc) | ((trans[1] >> 4) & 0x03);

        if (base64[i + 2] == '=') {
            continue;
        } else {
            trans[2] = num_strchr(base64char, base64[i + 2]);
        }
        // 2/3
        dedata[j++] = ((trans[1] << 4) & 0xf0) | ((trans[2] >> 2) & 0x0f);

        if (base64[i + 3] == '=') {
            continue;
        } else {
            trans[3] = num_strchr(base64char, base64[i + 3]);
        }

        // 3/3
        dedata[j++] = ((trans[2] << 6) & 0xc0) | (trans[3] & 0x3f);
    }

    dedata[j] = '\0';

    return j;
}

char* encode(char* message, const char* codeckey) {
    int src_length = strlen(message);
    int keyLength  = strlen(codeckey);

    char* des_buf = (char*)malloc(sizeof(char) * (src_length + 1));
    memset(des_buf, 0, sizeof(char) * (src_length + 1));

    for (int i = 0; i < src_length; i++) {
        int a      = message[i];
        int b      = codeckey[i % keyLength];
        des_buf[i] = (a ^ b) ^ i;
    }
    int base64_length = 0;
    if (src_length % 3 == 0)
        base64_length = src_length / 3 * 4;
    else
        base64_length = (src_length / 3 + 1) * 4;

    char* base64_enc = (char*)malloc(sizeof(char) * (base64_length + 1));
    base64_encode((const unsigned char*)des_buf, src_length, base64_enc);
    free(des_buf);
    return base64_enc;
}

char* decode(char* message, const char* codeckey) {
    int   base64_length = strlen(message);
    int   dec_length    = base64_length / 4 * 3;
    char* base64_dec    = (char*)malloc(sizeof(char) * (dec_length + 1));
    base64_decode(message, (unsigned char*)base64_dec);
    char* des_buf = (char*)malloc(sizeof(char) * (dec_length + 1));
    memset(des_buf, 0, sizeof(char) * (dec_length + 1));

    int keyLength = strlen(codeckey);
    for (int i = 0; i < dec_length; i++) {
        int a      = base64_dec[i];
        int b      = codeckey[i % keyLength];
        des_buf[i] = (i ^ a) ^ b;
    }
    free(base64_dec);

    return des_buf;
}

#define NTP_PORT        123
#define NTP_PACKET_SIZE 48
#define NTP_UNIX_OFFSET 2208988800
#define RESEND_INTERVAL 3
#define MAX_RETRIES     5
#define TIMEOUT_SEC     5

int get_net_time() {
    char* ntp_server = "ntp1.aliyun.com";

    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd == -1) {
        LOGE("create socket failed\n");
        return -1;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

    // LOGI("create udp_socket success!\n");

    struct hostent* server = gethostbyname(ntp_server);
    if (server == NULL) {
        LOGE("could not resolve %s\n", ntp_server);
        return -1;
    }

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
    ntp_packet[1] = 0;          // stratum, or how far away the server is from a reliable time source
    ntp_packet[2] = 6;          // poll interval, or how often the client will request time
    ntp_packet[3] = 0xEC;       // precision, or how accurate the client's clock is
    // the rest of the packet is all zeros

    int       retries = 0;
    uint8_t   ntp_response[NTP_PACKET_SIZE];
    socklen_t addr_len = sizeof(server_addr);

    while (retries < MAX_RETRIES) {
        ssize_t bytes_sent
            = sendto(sock_fd, ntp_packet, sizeof(ntp_packet), 0, (struct sockaddr*)&server_addr, addr_len);
        if (bytes_sent < 0) {
            LOGE("sendto failed\n");
            break;
        }

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);

        // Waiting for data return or timeout
        struct timeval timeout;
        timeout.tv_sec  = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int ret = select(sock_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret == -1) {
            LOGE("select error");
            return -1;
        } else if (ret == 0) {
            LOGE("timeout, retry.....\n");
            retries++;
            continue;
        }

        memset(ntp_response, 0, sizeof(ntp_response));
        ssize_t bytes_received
            = recvfrom(sock_fd, ntp_response, sizeof(ntp_response), 0, (struct sockaddr*)&server_addr, &addr_len);
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

    uint32_t ntp_time  = ntohl(*(uint32_t*)(ntp_response + 40));
    time_t   unix_time = ntp_time - NTP_UNIX_OFFSET;

    if (settimeofday(&(struct timeval){.tv_sec = unix_time}, NULL) == -1) {
        LOGE("settimeofday failed\n");
        return -1;
    }

    // LOGI("Time from %s: %s", ntp_server, ctime(&unix_time));

    return 0;
}

void* sync_time(void* arg) {
    while (1) {
        sleep(60);

        if (get_net_time() != 0) {
            usleep(1000 * 10);
            continue;
        }
    }

    return NULL;
}

#define BP_OFFSET 9
#define BP_GRAPH  60
#define BP_LEN    80

void print_data_stream_hex(const uint8_t* data, unsigned long len) {
    char              line[BP_LEN];
    unsigned long     i;
    static const char hexdig[] = "0123456789abcdef";

    if (!data)
        return;

    /* in case len is zero */
    line[0] = '\0';

    for (i = 0; i < len; i++) {
        int      n = i % 16;
        unsigned off;

        if (!n) {
            if (i)
                printf("%s\n", line);
            memset(line, ' ', sizeof(line) - 2);
            line[sizeof(line) - 2] = '\0';

            off = i % 0x0ffffU;

            line[2] = hexdig[0x0f & (off >> 12)];
            line[3] = hexdig[0x0f & (off >> 8)];
            line[4] = hexdig[0x0f & (off >> 4)];
            line[5] = hexdig[0x0f & off];
            line[6] = ':';
        }

        off           = BP_OFFSET + n * 3 + ((n >= 8) ? 1 : 0);
        line[off]     = hexdig[0x0f & (data[i] >> 4)];
        line[off + 1] = hexdig[0x0f & data[i]];

        off = BP_GRAPH + n + ((n >= 8) ? 1 : 0);

        if (isprint(data[i])) {
            line[BP_GRAPH + n] = data[i];
        } else {
            line[BP_GRAPH + n] = '.';
        }
    }

    printf("%s\n", line);
}

uint8_t* put_byte_stream(uint8_t* stream, uint64_t srcValue, size_t numBytes, uint32_t* offset) {
    for (int i = numBytes - 1; i >= 0; i--) {
        *(stream + *offset) = (uint8_t)(srcValue >> (8 * i));
        (*offset) += 1;
    }

    return stream;
}

uint64_t get_byte_stream(const uint8_t* stream, size_t numBytes, uint32_t* offset) {
    uint64_t result = 0;

    if (stream == NULL || numBytes > 8)
        return -1;

    for (size_t i = 0; i < numBytes; i++) {
        result = (result << 8) | stream[*offset];
        (*offset) += 1;
    }

    return result;
}

uint8_t* save_in_big_endian(uint8_t* array, uint64_t value, size_t numBytes) {
    for (int i = 0; i < numBytes; i++) {
        array[numBytes - 1 - i] = (value >> (8 * i)) & 0xFF;
    }

    return array;
}

uint64_t extract_from_big_endian(uint8_t* array, size_t numBytes) {
    uint64_t result = 0;

    for (size_t i = 0; i < numBytes; i++) {
        result = (result << 8) | array[i];
    }

    return result;
}

void reboot_system() {
    LOGI("Rebooting the system...\n");
    sleep(1); // 等待一段时间确保打印信息输出
    // 调用系统命令进行重启
    system("reboot");
}

int get_local_ip_address(char* ipAddress) {
    int          fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ); // 你可以根据实际情况更改接口名字

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    close(fd);

    strcpy(ipAddress, inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr));
    return 0;
}

int get_local_mac_address(char* macAddress) {
    int          fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ); // 你可以根据实际情况更改接口名字

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }

    close(fd);

    sprintf(macAddress, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char)ifr.ifr_hwaddr.sa_data[0],
            (unsigned char)ifr.ifr_hwaddr.sa_data[1], (unsigned char)ifr.ifr_hwaddr.sa_data[2],
            (unsigned char)ifr.ifr_hwaddr.sa_data[3], (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    return 0;
}

void format_time(time_t time, char* formattedTime) {
    sprintf(formattedTime, "%02dh-%02dm-%02ds", time / 3600, (time % 3600) / 60, time % 60);
}

int get_sys_load(int* used_ram, int* free_ram) {
    struct sysinfo info = {0};

    if (sysinfo(&info) == -1) {
        perror("sysinfo");
        return -1;
    }

    *used_ram = (info.totalram - info.freeram) / 1024;
    *free_ram = info.freeram / 1024;

    return 0;
}
