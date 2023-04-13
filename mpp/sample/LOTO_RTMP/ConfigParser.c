/*
 * ConfigParser.c:
 *
 * By Jessica Mao 2020/04/30
 *
 * Copyright (c) 2012-2020 Lotogram Inc. <lotogram.com, zhuagewawa.com>

 * Version 1.0.0.73	Details in update.log
 ***********************************************************************
 */

#include "ConfigParser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void delete_char(char str[], char target)
{
    int i, j;
    for (i = j = 0; str[i] != '\0'; i++)
    {
        if (str[i] != target)
        {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}

char *GetIniKeyString(char *title, char *key, char *filename)
{
    FILE *fp;
    int flag = 0;
    char sTitle[32], *wTmp;
    static char sLine[1024];

    sprintf(sTitle, "[%s]", title);
    if (NULL == (fp = fopen(filename, "r")))
    {
        perror("fopen");
        return "NULL";
    }

    while (NULL != fgets(sLine, 1024, fp))
    {
        // skip comment line
        if (0 == strncmp("//", sLine, 2))
            continue;
        if ('#' == sLine[0])
            continue;

        // find the specified key and return the value
        wTmp = strchr(sLine, '=');
        if ((NULL != wTmp) && (1 == flag))
        {
            if (0 == strncmp(key, sLine, wTmp - sLine))
            {
                if (sLine[strlen(sLine) - 1] == 0b1010) // 0b1010 refers to '\n'
                {
                    sLine[strlen(sLine) - 1] = '\0';
                }
                else if (sLine[strlen(sLine) - 1] > 0b00011111)
                {
                    sLine[strlen(sLine)] = '\0';
                }
                fclose(fp);
                return wTmp + 1;
            }
        }
        else
        {
            // find the title
            if (0 == strncmp(sTitle, sLine, strlen(sLine) - 1))
            {
                flag = 1;
            }
        }
    }
    fclose(fp);
    return NULL;
}

int GetIniKeyInt(char *title, char *key, char *filename)
{
    return atoi(GetIniKeyString(title, key, filename));
}

int PutIniKeyString(char *title, char *key, char *val, char *filename)
{
    FILE *fpr, *fpw;
    int flag = 0;
    char sLine[1024], sTitle[32], *wTmp;

    sprintf(sTitle, "[%s]", title);
    if (NULL == (fpr = fopen(filename, "r")))
        printf("fopen"); // 读取原文件
    sprintf(sLine, "%s.tmp", filename);
    if (NULL == (fpw = fopen(sLine, "w")))
        printf("fopen"); // 写入临时文件

    if (fpr == NULL)
    {
        fputs(sTitle, fpw); // 写入临时文件
        sprintf(sLine, "\n%s=%s\n", key, val);
        fputs(sLine, fpw);
    }
    else
    {
        while (NULL != fgets(sLine, 1024, fpr))
        {
            if (2 != flag)
            { // 如果找到要修改的那一行，则不会执行内部的操作
                wTmp = strchr(sLine, '=');
                if ((NULL != wTmp) && (1 == flag))
                {
                    if (0 == strncmp(key, sLine, wTmp - sLine))
                    {             // 长度依文件读取的为准
                        flag = 2; // 更改值，方便写入文件
                        sprintf(wTmp + 1, "%s\n", val);
                    }
                }
                else
                {
                    if (0 == strncmp(sTitle, sLine, strlen(sLine) - 1))
                    {             // 长度依文件读取的为准
                        flag = 1; // 找到标题位置
                    }
                }
            }
            fputs(sLine, fpw); // 写入临时文件
        }
        fclose(fpr);
    }

    fclose(fpw);

    sprintf(sLine, "%s.tmp", filename);
    return rename(sLine, filename); // 将临时文件更新到原文件
}

/*
 * 函数名：         PutIniKeyString
 * 入口参数：        title
 *                      配置文件中一组数据的标识
 *                  key
 *                      这组数据中要读出的值的标识
 *                  val
 *                      更改后的值
 *                  filename
 *                      要读取的文件路径
 * 返回值：         成功返回  0
 *                  否则返回 -1
 */
int PutIniKeyInt(char *title, char *key, int val, char *filename)
{
    char sVal[32];
    sprintf(sVal, "%d", val);
    return PutIniKeyString(title, key, sVal, filename);
}