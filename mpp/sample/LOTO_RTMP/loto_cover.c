#include "loto_cover.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <inttypes.h>

#include "loto_comm.h"
#include "ringfifo.h"
#include "loto_osd.h"
#include "common.h"
#include "loto_controller.h"
#include "loto_venc.h"

static RGN_HANDLE gs_rgnHandle = 5;

static MPP_CHN_S gs_stMppChnAttr = {
    .enModId = HI_ID_VPSS,
    .s32DevId = 0,
    .s32ChnId = 0
};

static RGN_CHN_ATTR_S gs_stRgnChnAttr = {
    .bShow = HI_TRUE,
    .enType = COVER_RGN,
    .unChnAttr.stCoverChn.enCoverType = AREA_RECT,
    .unChnAttr.stCoverChn.stRect.s32X = 0,
    .unChnAttr.stCoverChn.stRect.s32Y = 0,
    .unChnAttr.stCoverChn.stRect.u32Width = 1920,
    .unChnAttr.stCoverChn.stRect.u32Height = 1080,
    .unChnAttr.stCoverChn.u32Color = 0x202020,
    .unChnAttr.stCoverChn.u32Layer = 0,
    .unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR
};

HI_S32 LOTO_COVER_InitCoverRegion() {

    HI_S32 ret = 0;
    RGN_ATTR_S stRgnAttr = {0};
    stRgnAttr.enType = COVER_RGN;

    ret = HI_MPI_RGN_Create(gs_rgnHandle, &stRgnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_Create failed with %#x\n", ret);
        return HI_FAILURE;
    }

    return ret;
}

HI_S32 LOTO_COVER_UninitCoverRegion() {
    HI_S32 ret;

    ret = HI_MPI_RGN_Destroy(gs_rgnHandle);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_Destroy failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    return ret;
}


HI_S32 LOTO_COVER_AddCover() {
    HI_S32 ret = 0;

    ret = HI_MPI_RGN_AttachToChn(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_AttachToChn failed with %#x!\n", ret);
        return HI_FAILURE;
    }
    
    if (LOTO_VENC_SetVencBitrate(16) != HI_SUCCESS) {
        LOGE("LOTO_VENC_SetVencBitrate failed!\n");
        return HI_FAILURE;
    }

    LOGI("cover_add!\n");

    return HI_SUCCESS;
}

HI_S32 LOTO_COVER_RemoveCover() {
    HI_S32 ret = 0;
    VENC_CHN_ATTR_S stVencChnAttr = {0};

    ret = HI_MPI_RGN_DetachFromChn(gs_rgnHandle, &gs_stMppChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_DetachFromChn failed with %#x\n", ret);
        return HI_FAILURE;
    }

    if (LOTO_VENC_SetVencBitrate(3072) != HI_SUCCESS) {
        LOGE("LOTO_VENC_SetVencBitrate failed!\n");
        return HI_FAILURE;
    }

    LOGI("cover_remove!\n");

    return HI_SUCCESS;
}

static int gs_cover_switch = 0;
static int gs_cover_state = COVER_OFF;

void LOTO_COVER_Switch(int state) {
    if (gs_cover_state != state) {
        gs_cover_switch = 1;
        gs_cover_state = state;
    }
}

HI_S32 LOTO_COVER_ChangeCover() {
    if (gs_cover_switch == 0) {
        return 0;
    }

    if (gs_cover_state == 1) {
        if (LOTO_COVER_AddCover() != 0) {
            LOGE("LOTO_COVER_AddCover failed!\n");
            return -1;
        }
    } else {
        if (LOTO_COVER_RemoveCover() != 0) {
            LOGE("LOTO_COVER_RemoveCover failed!\n");
            return -1;
        }
    }

    gs_cover_switch = 0;

    return 0;
}
