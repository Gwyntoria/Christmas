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

#define MAX_LINE_LENGTH  1024
#define MAX_TITLE_LENGTH 32

typedef enum ErrorCodes {
    SUCCESS = 0,
    FILE_OPEN_ERROR,
    FILE_WRITE_ERROR,
    INVALID_ARGUMENTS,
    TITLE_NOT_FOUND
} ErrorCodes;

void DeleteChar(char str[], char target) {
    int i, j;
    for (i = j = 0; str[i] != '\0'; i++) {
        if (str[i] != target) {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}

char* GetConfigKeyValue(const char* title, const char* key, const char* filename) {
    FILE*       fp;
    int         title_state              = 0;
    char        sTitle[MAX_TITLE_LENGTH] = {0};
    static char line[MAX_LINE_LENGTH]    = {0}; // 函数的返回的字符串的内存不会被自动释放
    char*       value                    = NULL;

    sprintf(sTitle, "[%s]", title);
    // printf("sTitle = %s\n", sTitle);

    if (NULL == (fp = fopen(filename, "r"))) {
        perror("fopen");
        return NULL;
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

int GetIniKeyInt(char* title, char* key, char* filename) { return atoi(GetConfigKeyValue(title, key, filename)); }

int PutConfigKeyValue(const char* title, const char* key, const char* val, const char* filename) {
    if (!title || !key || !val || !filename) {
        return INVALID_ARGUMENTS;
    }

    FILE *fpr, *fpw;
    int   title_state = 0; // 0 - 未找到，1 - 找到但未修改，2 - 找到并已修改
    char  line[MAX_LINE_LENGTH], sTitle[MAX_TITLE_LENGTH];
    snprintf(sTitle, sizeof(sTitle), "[%s]", title);

    fpr = fopen(filename, "r");
    if (fpr == NULL) {
        perror("无法打开文件进行读取");
        return FILE_OPEN_ERROR;
    }

    char tmp_filename[MAX_LINE_LENGTH];
    snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", filename);
    fpw = fopen(tmp_filename, "w");
    if (fpw == NULL) {
        fclose(fpr);
        perror("无法打开文件进行写入");
        return FILE_WRITE_ERROR;
    }

    while (fgets(line, sizeof(line), fpr) != NULL) {
        if (title_state != 2) { // 如果不是已修改的状态，进入匹配查询
            char* value = strchr(line, '=');
            if (value != NULL && title_state == 1) {
                if (strncmp(key, line, value - line) == 0) {
                    title_state = 2;
                    fprintf(fpw, "%s=%s\n", key, val);
                    continue;
                }
            } else {
                if (strncmp(sTitle, line, strlen(sTitle)) == 0) {
                    title_state = 1; // 找到 title
                }
            }
        }

        // 将读取到的不需要修改的行写入临时文件
        fputs(line, fpw);
    }

    if (title_state != 2) {
        // 添加新的 title 和 键值对 到临时文件中
        fputs(sTitle, fpw);
        fprintf(fpw, "\n%s=%s\n", key, val);
    }

    fclose(fpr);
    fclose(fpw);

    if (rename(tmp_filename, filename) != 0) {
        perror("重命名临时文件出错");
        return FILE_WRITE_ERROR;
    }

    return SUCCESS;
}

/*
 * 函数名：         PutConfigKeyValue
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
int PutIniKeyInt(char* title, char* key, int val, char* filename) {
    char sVal[32];
    sprintf(sVal, "%d", val);
    return PutConfigKeyValue(title, key, sVal, filename);
}