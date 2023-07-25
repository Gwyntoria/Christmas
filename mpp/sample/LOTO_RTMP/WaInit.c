/*
 * WaInit.c:
 *
 * By Jessica Mao 2020/04/17
 *
 * Copyright (c) 2012-2020 Lotogram Inc. <lotogram.com, zhuagewawa.com>

 * Version 1.0.0.73	Details in update.log
 ***********************************************************************
 */

#include "WaInit.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <net/if.h>
#include <sys/ioctl.h>

#include "cJSON.h"
#include "http.h"
#include "md5.h"

#include "ConfigParser.h"
#include "common.h"

static int parse_jsonstring(loto_room_info *pRoomInfo, const char *pszResponse)
{
    if (pRoomInfo == NULL || pszResponse == NULL)
        return -1;

    cJSON *json = cJSON_Parse(pszResponse);

    if (NULL != json)
    {
        cJSON *pItem = cJSON_GetObjectItem(json, "status");
        if (NULL != pItem)
        {
            strcpy(pRoomInfo->szStatus, pItem->valuestring);
        }

        pItem = cJSON_GetObjectItem(json, "code");
        if (NULL != pItem)
        {
            pRoomInfo->iCode = pItem->valueint;
        }

        cJSON *pRoomItem = cJSON_GetObjectItem(json, "room");
        if (NULL != pRoomItem)
        {
            pItem = cJSON_GetObjectItem(pRoomItem, "type");
            if (NULL != pItem)
            {
                pRoomInfo->iRoomType = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "subtype");
            if (NULL != pItem)
            {
                pRoomInfo->iSubType = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "encodetype");
            if (NULL != pItem)
            {
                pRoomInfo->iEncodeType = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "monster");
            if (NULL != pItem)
            {
                pRoomInfo->iSupportMonster = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "machtype");
            if (NULL != pItem)
            {
                pRoomInfo->iMatchType = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "vip");
            if (NULL != pItem)
            {
                pRoomInfo->iVip = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "jp");
            if (NULL != pItem)
            {
                pRoomInfo->iJP = pItem->valueint;
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "_id");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szRoomId, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "domain");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szDomain, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "name");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szName, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "hostname");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szHostName, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "routerip");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szRouterIp, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "websocket");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szWebsocket, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "ip");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szIp, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "routerdns");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szRouterDNS, pItem->valuestring);
            }
            pItem = cJSON_GetObjectItem(pRoomItem, "frpport");
            if (NULL != pItem)
            {
                strcpy(pRoomInfo->szFrpPort, pItem->valuestring);
            }
            cJSON *pRtmpItem = cJSON_GetObjectItem(pRoomItem, "rtmp");
            if (NULL != pRtmpItem)
            {
                pItem = cJSON_GetObjectItem(pRtmpItem, "pushurl");
                if (NULL != pItem)
                {
                    strcpy(pRoomInfo->szPushURL, pItem->valuestring);
                }
                pItem = cJSON_GetObjectItem(pRtmpItem, "screenshot");
                if (NULL != pItem)
                {
                    strcpy(pRoomInfo->szScreenShot, pItem->valuestring);
                }
            }
            cJSON *pGameItem = cJSON_GetObjectItem(pRoomItem, "game");
            if (NULL != pGameItem)
            {
                pItem = cJSON_GetObjectItem(pGameItem, "count");
                if (NULL != pItem)
                {
                    pRoomInfo->iPlayerCount = pItem->valueint;
                }
            }
        }

        cJSON_Delete(json);
        json = NULL;

        return 0;
    }
    return -1;
}

char *random_uuid(char* buf)
{
    char *p = buf;
    int n;

    for( n = 0; n < 16; ++n )
    {
        int b = rand()%255;
        sprintf(p, "%02x", b);

        p += 2;
    }

    *p = 0;

    return buf;
}

char* create_token(char* pszMac, char* pszTS, char* pszTokenKey)
{
    uint8_t result[16];
    char*	pszToken = NULL;
    char	szMD5Result[64] = "";
    char	szInitial_msg[128];

    if (pszMac == NULL || pszTS == NULL || pszTokenKey == NULL)
        return	NULL;

    sprintf(szInitial_msg, "%s%s%s", pszMac, pszTS, pszTokenKey);

    md5((uint8_t*)szInitial_msg, strlen(szInitial_msg), result);
    for (int i = 0; i < 16; i++)
        sprintf(szMD5Result, "%s%02x", szMD5Result, result[i]);

    pszToken = (char*)malloc(128*sizeof(char));
    sprintf(pszToken, "%s.%s.%s", pszMac, pszTS, szMD5Result);

    return	pszToken;
}

void fill_http_header(char *header, char *content_type, int content_length) {

    sprintf(header, "Content-Type: %s\r\nContent-Length: %d", 
        content_type,
        content_length);
}

char *get_http_header_body(char *method, char *url, char *http_body, char *codec_key, char *pszTS)
{
    char szContentType[64] = "Content-Type: application/json\r\n";
    char szContentLength[32] = "Content-Length: ";

    char *pszHttpHeader = NULL;

   
    pszHttpHeader = (char *)malloc(1024);
    if (pszHttpHeader != NULL)
    {
        sprintf(pszHttpHeader, "%s%s%d", 
                szContentType,
                szContentLength, 
                strlen(http_body));

        // LOGD ("[%s] httpHeader: %s\n", log_Time(), pszHttpHeader);
    }
    return pszHttpHeader;
}

char *decode_response(const char *response, char *codec_key)
{
    if (response != NULL)
    {
        return decode((char *)response, (const char *)codec_key);
    }
    return NULL;
}

loto_room_info* loto_room_init(const char* server_url, const char* server_token)
{
    loto_room_info *pRoomInfo = NULL;
    char szServerUrl[256] = {0};
    char szServerToken[256] = {0};

    strcpy(szServerUrl, server_url);
    if (strlen(szServerUrl) > 0)
    {
        // char szServerToken[256] = "dadq0(~@E#Q)DSD12E1@_2{[QWE]2125+_a)E_QISDJ8NC8281@njfsGj";
        strcpy(szServerToken, server_token);

        // char* pszServerToken = GetConfigKeyValue((char*)"server", (char*)"token", (char*)"/home/pi/wawaji/WaController/server.ini");
        // if (pszServerToken != NULL)
        // strcpy(szServerToken, pszServerToken);

        while (1)
        {
            char szMac[256] = "";
            char szPostBody[512] = "";
            char szCodecKey[64] = "";
            char szT[64] = "";
            struct timeval tv;
            int hash_code = 0;
            int i = 0;

            get_mac(szMac);
            GetTimestampU64(szT, 0);

            MD5_CTX md5;
            MD5Init(&md5);
            unsigned char encrypt[256];
            unsigned char decrypt[32];
            char hexdecrypt[64] = "";
            memcpy(encrypt, szMac, strlen(szMac));
            memcpy(encrypt + strlen(szMac), szServerToken, strlen(szServerToken));
            memcpy(encrypt + strlen(szMac) + strlen(szServerToken), szT, strlen(szT));
            MD5Update(&md5, encrypt, strlen((char *)encrypt));
            MD5Final(&md5, decrypt);
            for (i = 0; i < 16; i++)
            {
                sprintf(hexdecrypt, "%s%02x", hexdecrypt, decrypt[i]);
            }

            char szCombineString[256] = "";
            sprintf(szCombineString, "%s%s%s", szServerToken, szMac, szT);
            hash_code = get_hash_code_24(szCombineString);
            printf("hash_code: %d\n", hash_code);

            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "ts", szT);
            cJSON_AddStringToObject(root, "mac", szMac);
            cJSON_AddStringToObject(root, "sign", hexdecrypt);
            cJSON_AddNumberToObject(root, "code", hash_code);
            cJSON_AddStringToObject(root, "token", szServerToken);

            char *pszBody = cJSON_Print(root);
            strcpy(szPostBody, pszBody);
            cJSON_Delete(root);

            printf("szPostBody: %s\n", szPostBody);

            ft_http_client_t *http = 0;
            ft_http_init();
            http = ft_http_new();
            if (!http)
            {
                LOGE("http is null\n");
                break;
            }

            //char *pszHttpHeader = get_http_header_body("POST", szServerUrl, szPostBody, szCodecKey, szT);
            char szContentType[64] = "application/json";
            int szContentLength = strlen(szPostBody);
            char szHttpHeader[1024] = {0};
            sprintf(szHttpHeader, "Content-Type: %s\r\nContent-Length: %d\r\n", szContentType, szContentLength);

            const char *response = NULL;
            response = ft_http_sync_request(http, szServerUrl, M_POST, szPostBody, strlen(szPostBody), szHttpHeader, strlen(szHttpHeader));
            printf("response: %s\n", response);

            pRoomInfo = (loto_room_info *)malloc(sizeof(loto_room_info));
            memset(pRoomInfo, 0, sizeof(loto_room_info));
            int iRet = parse_jsonstring(pRoomInfo, response);

            if (http)
            {
                ft_http_destroy(http);
            }
            ft_http_deinit();

            if (iRet == 0)
                if (strcmp(pRoomInfo->szStatus, "ok") == 0 && pRoomInfo->iCode == 0 && strlen(pRoomInfo->szRoomId) > 0)
                    break;

            free(pRoomInfo);
            pRoomInfo = NULL;
            sleep(10);
        }
    }

    return pRoomInfo;
}