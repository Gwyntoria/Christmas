#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 http server；
 *        响应 GET
 *
 * @return int HTTP server 启动状态
 */
int http_server();

#ifdef __cplusplus
}
#endif

#endif