
#ifndef LOTO_CONTROLLER_H
#define LOTO_CONTROLLER_H

#include <netinet/in.h>
#include <stdint.h>

#define PING_PACK 0x0103

// packet type
enum PACKET_TYPE {
    PACK_TYPE_MESG = 0x0100,
    PACK_TYPE_CTRL = 0x0101,
    PACK_TYPE_HERT = 0x0102,
    PACK_TYPE_NULL = 0xffff,
};

// message
// enum CONTROLLER_MESSAGE {
//     MESSAGE_COVER_ON            = 0x00A0,
//     MESSAGE_COVER_OFF           = 0x00A1,
//     MESSAGE_USER_OFFLINE        = 0x00B0,
//     MESSAGE_USER_ONLINE         = 0x00B1,
// };

// commond
enum CONTROLLER_COMMOND {
    CONTROL_SET_PUSH_URL        = 0x01A0,
    CONTROL_ADD_COVER           = 0x01A1,
    CONTROL_RMV_COVER           = 0x01A2,
    CONTROL_GET_COVER_STATE     = 0x01A3,
    CONTROL_SET_COVER_STATE     = 0x01A4,
    CONTROL_SERVER_TEST         = 0x01A5,
    CONTROL_SERVER_OFFI         = 0x01A6,
    CONTROL_REQUEST_RESEND      = 0x01C0,
};

enum CONTROLLER_COVER_STATE {
    COVER_OFF   = 0x00,
    COVER_ON    = 0x01,
    COVER_NULL  = 0x0F,
};

// server_url option
enum CONTROLLER_SERVER_URL_OPT {
    SERVER_TEST = 0x21,
    SERVER_OFFI = 0x22,
    SERVER_NULL = 0x2F,
};

typedef struct PacketHeader {
    uint16_t type;
    uint16_t length;    // Number of bytes in the whole packet
} PacketHeader;

typedef struct MessagePacket {
    PacketHeader header;
    uint16_t msg_len;
    char* msg;
    uint8_t checksum;
} MessagePacket;

typedef struct ControlPacket {
    PacketHeader header;
    uint16_t command;
    uint8_t checksum;
} ControlPacket;

typedef struct HeartbeatPacket {
    PacketHeader header;
    // uint32_t timestamp;
    uint8_t cover_state;
    uint8_t checksum;
} HeartbeatPacket;


#ifdef __cplusplus
extern "C" {
#endif

void *server_thread(void *arg);

#ifdef __cplusplus
}
#endif

#endif