#include "http_server.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "loto_controller.h"
#include "loto_cover.h"

#define MAX_PENDING           5
#define MAX_RESPONSE_SIZE     5120
#define MAX_HTML_CONTENT_SIZE 4096

#define HTML_FILE_PATH   "html/index.html"
#define DEVICE_FILE_PATH "html/device.html"

#define ISspace(x) isspace((int)(x))

#define SERVER_STRING "Server: loto_hisidv300/1.0.0\r\n"

#define RESPONSE_TEMPLATE "HTTP/1.1 200 OK\r\nContent-Type: text/%s\r\nContent-Length: %d\r\n\r\n%s"

/**
 * @brief 打印出错误信息并结束程序
 *
 * @param sc 错误信息
 */
void error_die(const char* sc) {
    perror(sc);
    exit(1);
}

/**
 * @brief 读取一行 http 报文，以 '\r' 或 '\r\n' 为行结束符
 *        注：只是把 '\r\n' 之前的内容读取到 buf 中，最后再加一个'\n\0'
 *
 * @param sock socket fd
 * @param buf 缓存指针
 * @param size 读取最大值
 * @return int 读取到的字符个数,不包括'\0'
 */
int get_request_line(int sock, char* buf, int size) {
    int  i = 0;
    char c = '\0';
    int  n;

    // 读取到的字符个数大于size或者读到\n结束循环
    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0); // 接收一个字符
        // DEBUG printf("%02X\n", c);
        if (n > 0) {
            if (c == '\r') // 回车字符
            {
                // 使用 MSG_PEEK
                // 标志使下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动
                n = recv(sock, &c, 1, MSG_PEEK);
                /* DEBUG printf("%02X\n", c); */

                // 读取到回车换行
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1,
                         0); // 还需要读取，因为之前一次的读取，相当于没有读取
                else
                    c = '\n'; // 如果只读取到\r，也要终止读取
            }
            // 没有读取到\r,则把读取到内容放在buf中
            buf[i] = c;
            i++;
        } else
            c = '\n';
    }

    buf[i] = '\0';

    return i;
}

/**
 * @brief 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持，501
 *
 * @param client client_socket_fd
 */
void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, SERVER_STRING); // Server: jdbhttpd/0.1.0\r\n
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 * @brief 告知客户端404错误(没有找到)
 *
 * @param client client_socket_fd
 */
void not_found(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);

    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**
 * @brief 把文件 resource 中的数据读取到client中
 *
 * @param client client_socket_fd
 * @param resource 需要传输的文件指针
 */
void cat(int client, FILE* resource) {
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

// 告知客户端该请求有错误400
void bad_request(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);

    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);

    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);

    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);

    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

void parse_request_line(char* request_line, int request_line_len, char* method, char* url, char* version) {
    size_t i = 0, j = 0;

    // 获取 method
    while (!ISspace(request_line[j]) && (i < request_line_len)) {
        method[i] = request_line[j];
        i++;
        j++;
    }
    method[i] = '\0';
    printf("Method: %s\n", method);

    while (ISspace(request_line[j]) && (j < request_line_len))
        j++;

    // 获取 url
    i = 0;
    while (!ISspace(request_line[j]) && (i < request_line_len) && (j < request_line_len)) {
        url[i] = request_line[j];
        i++;
        j++;
    }
    url[i] = '\0';
    printf("Url: %s\n", url);

    while (ISspace(request_line[j]) && (j < request_line_len))
        j++;

    // 获取 version
    i = 0;
    while (!ISspace(request_line[j]) && (i < request_line_len) && (j < request_line_len)) {
        version[i] = request_line[j];
        i++;
        j++;
    }
    version[i] = '\0';
    printf("Version: %s\n", version);

    // // strcpy(method, strtok(request_line, " "));
    // // strcpy(path, strtok(NULL, " "));
    // // strcpy(version, strtok(NULL, " "));
}

void parse_path_with_params(const char* url, char* path, char* parameters) {
    char* params = strchr(url, '?'); // 查找路径中的问号位置

    if (params != NULL) {
        char* path_only = strndup(url, params - url); // 提取不带参数的路径部分
        strcpy(path, path_only);
        free(path_only);
        printf("Path: %s\n", path);

        strcpy(parameters, params + 1);
        printf("Param: %s\n", parameters);

        // char* param = strtok(params + 1, "&");  // 解析参数
        // while (param != NULL) {
        //     printf("Param: %s\n", param);
        //     param = strtok(NULL, "&");
        // }
    } else {
        strcpy(path, url);
        printf("Path: %s\n", url);
    }
}

/**
 * @brief server向client 发送响应头部信息
 *
 * @param client client_socket_fd
 * @param content_type 内容类型：html or plain
 */
void send_header(int client, const char* content_type, int content_length) {
    char header[1024];
    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/%s\r\n"
            "Content-Length: %d\r\n"
            "\r\n",
            content_type, content_length);

    send(client, header, strlen(header), 0);
}

// 函数用于读取HTML文件的内容
char* read_html_file(const char* file_path, int* content_length) {
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        perror("fopen");
        return NULL;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // 分配足够的内存来存储文件内容
    char* content = (char*)malloc(file_size + 1);
    if (content == NULL) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    // 读取文件内容
    size_t bytes_read = fread(content, 1, file_size, file);
    if (bytes_read != file_size) {
        perror("fread");
        fclose(file);
        free(content);
        return NULL;
    }

    // 添加字符串结束符
    content[file_size] = '\0';

    fclose(file);
    *content_length = file_size + 1;

    return content;
}

void write_html_file(const char* content, const char* file_path) {
    FILE* file = fopen(file_path, "w");
    if (file == NULL) {
        file = fopen(file_path, "a");
        if (file == NULL) {
            perror("fopen");
            return;
        }
    }

    // 写入内容到文件
    fprintf(file, "<html>\r\n");
    fprintf(file, "<head><title>Device Infomation</title></head>\r\n");
    fprintf(file, "<body>\r\n");
    fprintf(file, "%s\r\n", content);
    fprintf(file, "</body>\r\n");
    fprintf(file, "</html>\r\n");

    fclose(file);
}

int send_html_response(int client_socket, const char* file_path) {
    int   content_length;
    char* html_content = read_html_file(file_path, &content_length);
    if (html_content == NULL) {
        printf("Failed to read HTML file.\n");
        return -1;
    }

    char response[MAX_RESPONSE_SIZE];
    snprintf(response, sizeof(response), RESPONSE_TEMPLATE, "html", content_length, html_content);

    // printf("content_length: %d\n", content_length);
    // printf("response: %s\n", response);

    if (send(client_socket, response, strlen(response), 0) < 0) {
        perror("send");
        free(html_content);
        return -1;
    }
    free(html_content);

    return 0;
}

int send_plain_response(int client_socket, const char* content) {
    int  content_length = strlen(content);
    char response[1024];

    sprintf(response, RESPONSE_TEMPLATE, "plain", content_length, content);

    if (send(client_socket, response, strlen(response), 0) < 0) {
        return -1;
    }

    return 0;
}

extern time_t     program_start_time;
extern DeviceInfo device_info;

void get_device_info(char* device_info_content) {
    device_info.running_time = time(NULL) - program_start_time;
    device_info.stream_state = LOTO_COVER_GetCoverState();

    char running_time[32];
    FormatTime(device_info.running_time, running_time);

    char temp[1024];
    sprintf(temp, "version:         %s\r\n", device_info.version);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "device_num:      %s\r\n", device_info.device_num);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    if (device_info.stream_state == 0) {
        sprintf(temp, "stream_state:    on\r\n");
        strcat(device_info_content, temp);
        memset(temp, 0, sizeof(temp));

    } else {
        sprintf(temp, "stream_state:    off\r\n");
        strcat(device_info_content, temp);
        memset(temp, 0, sizeof(temp));
    }

    sprintf(temp, "audio_state:     %s\r\n", device_info.audio_state);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "ip_addr:         %s\r\n", device_info.ip_addr);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "mac_addr:        %s\r\n", device_info.mac_addr);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "start_time:      %s\r\n", device_info.start_time);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "running_time:    %s\r\n", running_time);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "push_url:        %s\r\n", device_info.push_url);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    sprintf(temp, "server_url:      %s\r\n", device_info.server_url);
    strcat(device_info_content, temp);
    memset(temp, 0, sizeof(temp));

    // write_html_file(device_info_html, DEVICE_FILE_PATH);
}

void* accept_request(void* pclient) {
    int  numchars;
    char buf[1024];
    int  client = *(int*)pclient;

    char method[256];
    char url[256];
    char version[256];
    char path[256];
    char params[256];

    // 获取一行HTTP请求报文
    numchars = get_request_line(client, buf, sizeof(buf));

    parse_request_line(buf, numchars, method, url, version);

    // 暂时只支持get方法
    if (strcasecmp(method, "GET")) {
        unimplemented(client);
        return NULL;
    }

    // 如果是 get 方法，path 可能带 ？参数
    if (strcasecmp(method, "GET") == 0) {
        parse_path_with_params(url, path, params);
    }
    // 以上将 request_line 解析完毕

    if (strcasecmp(path, "/hello") == 0) {
        char content[32] = "Hello world!\r\n";
        if (send_plain_response(client, content) != 0)
            printf("send error\n");
    } else if (strcasecmp(path, "/html") == 0) {
        if (send_html_response(client, HTML_FILE_PATH) != 0)
            printf("send error\n");
    } else if (strcasecmp(path, "/cover") == 0) {
        if (strcasecmp(params, "on") == 0) {
            LOTO_COVER_Switch(COVER_ON);
            send_plain_response(client, "add cover\r\n");
        } else if (strcasecmp(params, "off") == 0) {
            LOTO_COVER_Switch(COVER_OFF);
            send_plain_response(client, "remove cover\r\n");
        }
    } else if (strcmp(path, "/") == 0) {
        char device_info_content[4096];
        get_device_info(device_info_content);
        if (send_plain_response(client, device_info_content) != 0)
            printf("send error\n");
    } else {
        not_found(client);
    }

    close(client);
    // printf("HTTP client disconnected.\n");
    return NULL;
}

// socket initial: socket() ---> bind() ---> listen()
int startup(uint16_t* port) {
    int                httpd = 0;
    struct sockaddr_in name;

    httpd = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    memset(&name, 0, sizeof(name));
    name.sin_family      = AF_INET;
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    name.sin_port        = htons(*port);

    if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0) {
        close(httpd);
        error_die("bind");
    }

    if (*port == 0) /* Check if the incoming port number is 0.
                       If it is 0, the port needs to be dynamically allocated */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }

    if (listen(httpd, MAX_PENDING) < 0) {
        close(httpd);
        error_die("listen");
    }
    return (httpd);
}

int http_server(void) {
    int                server_sock = -1;
    uint16_t           port        = 80; // 监听端口号
    int                client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t          client_name_len = sizeof(client_name);
    pthread_t          newthread;

    server_sock = startup(&port); // 服务器端监听套接字设置
    printf("httpd running on port %d\n", port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
        if (client_sock == -1) {
            error_die("accept");
        } else {
            // printf("HTTP client connected.\n");
        }

        /* accept_request(client_sock); */
        if (pthread_create(&newthread, NULL, accept_request, (void*)&client_sock) != 0)
            perror("accept_request");
    }

    close(server_sock);

    return (0);
}
