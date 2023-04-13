#ifndef LOTO_AENC_H
#define LOTO_AENC_H

#include <pthread.h>

#include "loto_comm.h"

typedef struct tagLOTO_AENC_S {
    HI_BOOL     bStart;
    HI_S32      AeChn;
    pthread_t   stAencPid;
} LOTO_AENC_S;

void *LOTO_AENC_CLASSIC(void *p);


#endif // LOTO_AENC_H