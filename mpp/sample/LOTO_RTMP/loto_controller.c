/**
 * @file loto_controller.c
 * @author Karl Meng (karlmfloating@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "loto_controller.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>

#include "common.h"
#include "loto_venc.h"
#include "loto_cover.h"

static const uint8_t crc8Table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

uint8_t CalculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;

    for (size_t i = 0; i < length; i++) {
        crc = crc8Table[crc ^ data[i]];
    }

    return crc;
}


// uint8_t calculate_checksum(uint8_t *data, int len) {
//     uint32_t sum = 0;
//     uint16_t word = 0;
//     int i;

//     // 计算所有16位的和
//     for (i = 0; i < len; i += 2) {
//         word = ((data[i] << 8) & 0xFF00) + (data[i + 1] & 0xFF);
//         sum += word;
//     }

//     // 如果数据长度是奇数，则处理最后一个字节
//     if (len % 2) {
//         sum += ((uint32_t)(data[len - 1]) << 8) & 0xffff;
//     }

//     // 将高16位和低16位相加，直到高16位为0
//     while (sum >> 16) {
//         sum = (sum & 0xFFFF) + (sum >> 16);
//     }

//     // 对和取反，得到校验位
//     uint8_t checksum = (uint8_t)(~sum & 0xFF);

//     return checksum;
// }

void serialize_control_packet(uint8_t *buffer, ControlPacket *packet) {
    buffer[0] = (packet->header.type >> 8) & 0x00FF;
    buffer[1] = packet->header.type & 0xFF;

    buffer[2] = (packet->header.length >> 8) & 0x00FF;
    buffer[3] = packet->header.length & 0xFF;

    buffer[4] = (packet->command >> 8) & 0x00FF;
    buffer[5] = packet->command & 0xFF;

    // uint8_t checksum = calculate_checksum(buffer, sizeof(ControlPacket) - 2);
    uint8_t checksum = CalculateCRC8(buffer, sizeof(ControlPacket) - 2);
    packet->checksum = checksum;

    buffer[6] = (checksum >> 8) & 0x00FF;
    buffer[7] = checksum & 0xFF;
}

void deserilize_packet_header(const uint8_t *buffer, PacketHeader *packet_header, uint32_t *offset) {
    packet_header->type = GetByteStream(buffer, sizeof(packet_header->type), offset);
    packet_header->length = GetByteStream(buffer, sizeof(packet_header->length), offset);
}

void deserialize_control_packet(const uint8_t *buffer, ControlPacket *packet, uint32_t *offset) {
    // GetByteStream(buffer, &(packet->header.type), sizeof(packet->header.type), offset);
    // GetByteStream(buffer, &(packet->header.length), sizeof(packet->header.length), offset);
    deserilize_packet_header(buffer, &(packet->header), offset);
    packet->command = GetByteStream(buffer, sizeof(packet->command), offset);
    packet->checksum = GetByteStream(buffer, sizeof(packet->checksum), offset);

    // packet->header.type = ((buffer[0] << 8) & 0xff00) | (buffer[1] & 0x00ff);
    // packet->header.length = ((buffer[2] << 8) & 0xff00) | (buffer[3] & 0x00ff);
    // packet->command = ((buffer[4] << 8) & 0xff00) | (buffer[5] & 0x00ff);
    // packet->checksum = ((buffer[6] << 8) & 0xff00) | (buffer[7] & 0x00ff);
}

void deserialize_message_packet(const uint8_t *buffer, MessagePacket *packet, uint32_t *offset) {
    deserilize_packet_header(buffer, &(packet->header), offset);
    packet->msg_len = GetByteStream(buffer, sizeof(packet->msg_len), offset);
    packet->msg = (char*)&(buffer[*offset]);
    (*offset) += packet->msg_len;
    packet->checksum = GetByteStream(buffer, sizeof(packet->checksum), offset);
}

void deserialize_heartbeat_packet(const uint8_t *buffer, HeartbeatPacket *packet, uint32_t *offset) {
    deserilize_packet_header(buffer, &(packet->header), offset);
    // packet->timestamp = GetByteStream(buffer, sizeof(packet->timestamp), offset);
    packet->cover_state = GetByteStream(buffer, sizeof(packet->cover_state), offset);
    packet->checksum = GetByteStream(buffer, sizeof(packet->checksum), offset);
}


#define PORT 8998
#define MAX_PENDING 5
#define MAX_BUF_SIZE 1024

extern int get_server_option();
extern void set_server_option(int server_option);

int create_server_socket(struct sockaddr_in *server_address) {
    int server_socket;

    struct sockaddr_in address;
    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOGE("Failed to create socket\n");
        return -1;
    }

    // Set server address
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(server_socket);
        LOGE("Failed to bind socket\n");
        return -1;
    }

    // Listen for connections
    if (listen(server_socket, MAX_PENDING) < 0) {
        close(server_socket);
        LOGE("Failed to listen for connections\n");
        return -1;
    }

    memcpy(server_address, (void *)&address, sizeof(struct sockaddr_in));

    return server_socket;
}

typedef struct HeartbeatArg {
    int client_socket;
    int heartbeat_thrd;
} HeartbeatArg;

void *heart_beat_thread(void *arg) {
    HeartbeatArg *heartbeatArg = (HeartbeatArg *)arg;
    heartbeatArg->heartbeat_thrd = 1;

    uint8_t beat[2];
    SaveInBigEndian(beat, PACK_TYPE_HERT, 2);

    while (1) {
        if (send(heartbeatArg->client_socket, beat, sizeof(beat), 0) < 0) {
            LOGE("Client disconnected\n");
            heartbeatArg->heartbeat_thrd = 0;
            break;
        }

        sleep(10);
    }

    pthread_exit(NULL);
}

void *socket_server_thread(void *arg) {
    int server_socket, client_socket;
    socklen_t address_length;
    struct sockaddr_in server_address;

    int cover_state;
    const int cover_state_on = COVER_ON;
    const int cover_state_off = COVER_OFF;
    const int server_state_test = SERVER_TEST;
    const int server_state_offi = SERVER_OFFI;

    // uint8_t recv_buffer[MAX_BUF_SIZE];
    // memset(recv_buffer, 0, MAX_BUF_SIZE);
    uint8_t send_buffer_c[MAX_BUF_SIZE];
    memset(send_buffer_c, 0, MAX_BUF_SIZE);

    int resend_state = 0;
    HeartbeatArg heartbeatArg = {0};
    pthread_t heartbeat;

    do {
        server_socket = create_server_socket(&server_address);

        if (server_socket < 0) {
            sleep(5);
        }
    } while (server_socket < 0);
    LOGI("Waiting for connections...\n");

    address_length = sizeof(server_address);

    while (1) {
        // Accept connection from client
        if ((client_socket = accept(server_socket, (struct sockaddr *)&server_address, &address_length)) < 0) {
            LOGE("Failed to accept connection\n");
            continue;
        }
        LOGD("Client connected: %s:%d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));

        if (heartbeatArg.heartbeat_thrd == 0) {
            heartbeatArg.client_socket = client_socket;
            pthread_create(&heartbeat, NULL, heart_beat_thread, &heartbeatArg);
        }

        while (1) {
            uint8_t recv_buffer[MAX_BUF_SIZE];
            memset(recv_buffer, 0, MAX_BUF_SIZE);
            uint8_t send_buffer[MAX_BUF_SIZE];
            memset(send_buffer, 0, MAX_BUF_SIZE);
            
            // ControlPacket send_packet;
            // memset(&send_packet, 0, sizeof(send_packet));

            int recv_buffer_len = recv(client_socket, recv_buffer, MAX_BUF_SIZE, 0);
            if (recv_buffer_len <= 0) {
                LOGE("Client disconnected\n");
                break;
            }

            printf("recv_buffer: \n");
            PrintDataStreamHex(recv_buffer, recv_buffer_len);

            /* Get type of packet */
            uint16_t packet_type = 0;
            packet_type = (uint16_t)ExtractFromBigEndian(recv_buffer, 2);

            // LOGD("packet_type = %04x\n", packet_type);

            if (packet_type == PACK_TYPE_CTRL) {
                uint32_t offset = 0;
                ControlPacket recv_ctrl_packet;
                memset(&recv_ctrl_packet, 0, sizeof(recv_ctrl_packet));

                // Save the contents of the array into a struct
                deserialize_control_packet(recv_buffer, &recv_ctrl_packet, &offset);

                // uint8_t checksum = calculate_checksum(recv_buffer, sizeof(ControlPacket) - 1);
                uint8_t checksum = CalculateCRC8(recv_buffer, recv_buffer_len - 1);
                if (checksum != recv_ctrl_packet.checksum) {
                    LOGE("the received packet is wrong! Please resend.\n");
                    // sprintf(send_buffer, "RESEND");
                    memcpy(send_buffer, "RESEND", 7);

                    if (send(client_socket, (void *)send_buffer, strlen((char *)send_buffer), 0) < 0) {
                        LOGE("Client disconnected\n");
                        break;
                    }

                    break;
                } else {
                    if (recv_ctrl_packet.command == CONTROL_ADD_COVER) {
                        LOTO_COVER_Switch(cover_state_on);
                        memcpy(send_buffer, "COVER ON", 9);
                    } else if (recv_ctrl_packet.command == CONTROL_RMV_COVER) {
                        LOTO_COVER_Switch(cover_state_off);
                        memcpy(send_buffer, "COVER OFF", 10);
                    } else if (recv_ctrl_packet.command = CONTROL_SERVER_TEST) {
                        set_server_option(server_state_test);
                    } else if (recv_ctrl_packet.command = CONTROL_SERVER_OFFI) {
                        set_server_option(server_state_offi);
                    }
                }
            } else if (packet_type == PACK_TYPE_MESG) {
                uint32_t offset = 0;
                MessagePacket recv_mesg_packet;
                memset(&recv_mesg_packet, 0, sizeof(recv_mesg_packet));

                deserialize_message_packet(recv_buffer, &recv_mesg_packet, &offset);
                char message[1024];
                strncpy(message, recv_mesg_packet.msg, recv_mesg_packet.msg_len);

                // uint8_t checksum = calculate_checksum(recv_buffer, recv_buffer_len - 1);
                uint8_t checksum = CalculateCRC8((uint8_t *)recv_buffer, recv_buffer_len - 1);

                // 

            } else if (packet_type == PACK_TYPE_HERT) {
                uint32_t offset = 0;
                HeartbeatPacket recv_heart_packet;
                memset(&recv_heart_packet, 0, sizeof(recv_heart_packet));

                deserialize_heartbeat_packet(recv_buffer, &recv_heart_packet, &offset);

                // LOGD("recv_heart_packet.header.length = %04x\n", recv_heart_packet.header.length);
                // LOGD("recv_heart_packet.cover_state = %02x\n", recv_heart_packet.cover_state);
                // LOGD("recv_heart_packet.checksum = %02x\n", recv_heart_packet.checksum);

                uint8_t checksum = CalculateCRC8(recv_buffer, recv_buffer_len - 1);
                // LOGD("calculated_checksum = %02x\n", checksum);

                if (checksum != recv_heart_packet.checksum) {
                    LOGE("the received packet is wrong! Please resend.\n");
                    memcpy(send_buffer, "RESEND", 7);

                    if (send(client_socket, (void *)send_buffer, strlen((char *)send_buffer), 0) < 0) {
                        LOGE("Client disconnected\n");
                        break;
                    }

                    continue;
                } else {
                    LOTO_COVER_Switch(recv_heart_packet.cover_state);
                    if (recv_heart_packet.cover_state == COVER_ON) {
                        memcpy(send_buffer, "COVER_ON", 9);
                    } else {
                        memcpy(send_buffer, "COVER_OFF", 10);
                    }
                }
            }
            
            if (send(client_socket, (void *)send_buffer, strlen((char *)send_buffer), 0) < 0) {
                LOGE("Client disconnected\n");
                break;
            }
            // sleep(MAX_PENDING);
        }

        close(client_socket);
    }

    close(server_socket);
    LOGD("close server socket\n");

    pthread_exit(NULL);
}