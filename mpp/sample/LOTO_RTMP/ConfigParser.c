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

char *GetIniKeyString(const char *title, const char *key, const char *filename)
{
    FILE *fp;
    int title_state = 0;
    char sTitle[32], *value;
    static char line[1024];

    sprintf(sTitle, "[%s]", title);
    // printf("sTitle = %s\n", sTitle);
    
    if (NULL == (fp = fopen(filename, "r"))) {
        perror("fopen");
        return "NULL";
    }

    while (NULL != fgets(line, 1024, fp)) {
        // skip comment line
        if (0 == strncmp("//", line, 2))
            continue;
        if ('#' == line[0])
            continue;

        // find title
        if (title_state == 0 && strncmp(sTitle, line, strlen(sTitle)) == 0) {
            title_state = 1;
            continue;
        }

        // find the specified key and return the value
        if (title_state == 1) {
            value = strchr(line, '=');
            if (value != NULL && (strncmp(key, line, value - line) == 0)) {
                // 0b1010 refers to '\n'
                if (line[strlen(line) - 1] == 0b1010) { 
                    line[strlen(line) - 1] = '\0';
                } else if (line[strlen(line) - 1] > 0b00011111) {
                    line[strlen(line)] = '\0';
                }
                fclose(fp);
                return value + 1;
            }
        }
    }
    fclose(fp);
    printf("get title failed!\n");
    exit(1);
}

int GetIniKeyInt(char *title, char *key, char *filename)
{
    return atoi(GetIniKeyString(title, key, filename));
}

int PutIniKeyString(const char *title, const char *key, const char *val, const char *filename)
{
    FILE *fpr, *fpw;
    int title_state = 0;
    char line[1024], sTitle[32], *value;

    sprintf(sTitle, "[%s]", title);
    if (NULL == (fpr = fopen(filename, "r")))
        printf("fopen"); // 读取原文件
    sprintf(line, "%s.tmp", filename);
    if (NULL == (fpw = fopen(line, "w")))
        printf("fopen"); // 写入临时文件

    if (fpr == NULL)
    {
        fputs(sTitle, fpw); // 写入临时文件
        sprintf(line, "\n%s=%s\n", key, val);
        fputs(line, fpw);
    }
    else
    {
        while (NULL != fgets(line, 1024, fpr))
        {
            if (2 != title_state)
            { // 如果找到要修改的那一行，则不会执行内部的操作
                value = strchr(line, '=');
                if ((NULL != value) && (1 == title_state))
                {
                    if (0 == strncmp(key, line, value - line))
                    {             // 长度依文件读取的为准
                        title_state = 2; // 更改值，方便写入文件
                        sprintf(value + 1, "%s\n", val);
                    }
                }
                else
                {
                    if (0 == strncmp(sTitle, line, strlen(sTitle)))
                    {             // 长度依文件读取的为准
                        title_state = 1; // 找到标题位置
                    }
                }
            }
            fputs(line, fpw); // 写入临时文件
        }
        fclose(fpr);
    }

    fclose(fpw);

    sprintf(line, "%s.tmp", filename);
    return rename(line, filename); // 将临时文件更新到原文件
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