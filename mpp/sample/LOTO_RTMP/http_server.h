#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#define TEST_SERVER_URL "http://t.zhuagewawa.com/admin/room/register.pusher"
#define RELE_SERVER_URL "http://r.zhuagewawa.com/admin/room/register.pusher"

#ifdef __cplusplus
extern "C" {
#endif

void* http_server(void* arg);

#ifdef __cplusplus
}
#endif

#endif