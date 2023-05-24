#include "https.h"

#include <stdio.h>
#include <string.h>

#include <curl/curl.h>

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userdata)
{
    size_t total_size = size * nmemb;
    char* response_buffer = (char*)userdata;
    memcpy(response_buffer, contents, total_size);
    // printf("%.*s", total_size, (char *)contents); // 打印回复内容
    return total_size;
}