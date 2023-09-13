/*
 * Common.c:
 *
 * By Jessica Mao 2020/05/18
 *
 * Copyright (c) 2012-2020 Lotogram Inc. <lotogram.com, zhuagewawa.com>

 * Version 1.0.0.73	Details in update.log
 ***********************************************************************
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <time.h>
#include "common.h"
#include <pthread.h>
#include <stdarg.h>

 /*
 * log_Time:
 *  Return the formated time
 *********************************************************************************
 */
char *log_Time(void)
{
    struct  tm      *ptm;
    struct  timeb   stTimeb;
    static  char    szTime[256] = {0};

    ftime(&stTimeb);
    ptm = localtime(&stTimeb.time);
    sprintf(szTime, "%04d-%02d-%02d %02d:%02d:%02d.%03d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, stTimeb.millitm);
    //szTime[23] = 0;
    return szTime;
}

long long string2int(const char *str)
{
	char flag = '+';//指示结果是否带符号
	long long  res = 0;

	if(*str=='-')//字符串带负号
	{
		++str;//指向下一个字符
		flag = '-';//将标志设为负号
	}
	//逐个字符转换，并累加到结果res
	while(*str>=48 && *str<=57)//如果是数字才进行转换，数字0~9的ASCII码：48~57
	{
		res = 10*res+  *str++-48;//字符'0'的ASCII码为48,48-48=0刚好转化为数字0
	}

    if(flag == '-')//处理是负数的情况
	{
		res = -res;
	}

	return res;
}

int string_reverse(char * strSrc)
{
    int len = 0;
    int i = 0;
    char * output = NULL;
    char * pstr = strSrc;
    while(* pstr)
    {
        pstr++;
        len++;
    }
    output = (char *)malloc(len);
    if(output == NULL)
    {
        perror("malloc");
        return -1;
    }
    for(i = 0; i < len ;i++)
    {
        output[i] = strSrc[len - i -1];
        //printf("output[%d] = %c\n",len - i -1,strSrc[len - i - 1]);
    }
    output[len] = '\0';
    strcpy(strSrc, output);
    free(output);
    return 0;
}

int  int2string(long long value, char * output)
{
    int index = 0;
    if(value == 0)
    {
        output[0] = value + '0';
        return 1;
    }
    else
    {
        while(value)
        {
            output[index] = value % 10 + '0';
            index ++;
            value /= 10;
        }
        string_reverse(output);
        return 1;
    }
}

long long get_timestamp(char* pszTS, int isMSec)
{
    long long 	timestamp;
    char        szT[64] = "";
    struct 	timeval tv ;
    gettimeofday(&tv,NULL);
    if (isMSec > 0)
        timestamp = (long long)tv.tv_sec*1000 + (long long)tv.tv_usec/1000;
    else
        timestamp = tv.tv_sec;

    if (pszTS != NULL)
    {
        int2string(timestamp, szT);
        strcpy(pszTS, szT);
    }

    return timestamp;
}


#define WA_LOG_FOLDER   "/root/WaController/LOG"
#define WA_LOG_FILE     "LotoRTMP.log"
#define LIBRTMP_LOG_FILE     "libRTMP.log"
#define MAX_FILE_SIZE   5*1024*1024
#define MAX_FILE_COUNT  10
static  FILE* vLogHandle = NULL;
static  pthread_mutex_t _vLogMutex;

FILE* InitRtmpLogFile()
{
    char log[256];

    mkdir(WA_LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", WA_LOG_FOLDER, LIBRTMP_LOG_FILE);
    FILE* logHandle = fopen((char*)log, "a");
    if (logHandle){

        return logHandle;
    }
    else{
        return  NULL;
    }
}

void CloseRtmpLogFiel(FILE* logHandle)
{
    if (logHandle != NULL)
        fclose(logHandle);
}

int InitTCpRtpLog()
{
    char log[256];

    mkdir(WA_LOG_FOLDER, 0755);
    snprintf(log, sizeof(log), "%s/%s", WA_LOG_FOLDER, WA_LOG_FILE);
    vLogHandle = fopen((char*)log, "a");
    if (vLogHandle){
        return 0;
    }
    else{
        return  - 1;
    }
}

static long _getfilesize(FILE*stream)
{
    long curpos,length;
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
 
    if (vLogHandle)
    {
        fclose(vLogHandle);
        
        for (int i = (MAX_FILE_COUNT-1); i > 0; i --)
        {
            snprintf(tmp, sizeof(tmp), "%s/%s.%d", WA_LOG_FOLDER, WA_LOG_FILE, i);
            snprintf(tmp2, sizeof(tmp), "%s/%s.%d", WA_LOG_FOLDER, WA_LOG_FILE, i+1);
            if((access(tmp,F_OK))!=-1)   
            {
                remove(tmp2);
                rename(tmp, tmp2);
            }
        }

        snprintf(tmp, sizeof(tmp), "%s/%s", WA_LOG_FOLDER, WA_LOG_FILE);
        snprintf(tmp2, sizeof(tmp), "%s/%s.1", WA_LOG_FOLDER, WA_LOG_FILE);

        remove(tmp2);
        rename(tmp, tmp2);

        vLogHandle = fopen((char*)tmp, "a");
    }

    return 1;
}


void LogTcpRtp(char *p_fmt, ...)
{
    va_list ap;

    if (!vLogHandle){
        return;
    }

    pthread_mutex_lock(&_vLogMutex);
    va_start(ap, p_fmt);
    vfprintf(vLogHandle, p_fmt, ap);
    va_end(ap);

    fflush(vLogHandle);

    long file_size = _getfilesize(vLogHandle);
    if (file_size >= MAX_FILE_SIZE)
    {
        _rebuildLogFiles();
    }

    pthread_mutex_unlock(&_vLogMutex);
}