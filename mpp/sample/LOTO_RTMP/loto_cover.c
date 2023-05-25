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

static RGN_HANDLE gs_rgnHandle = 5;
static MPP_CHN_S gs_stMppChnAttr = {0};
static RGN_CHN_ATTR_S gs_stRgnChnAttr = {0};

HI_S32 CreateCoverRegion(RGN_HANDLE rgnHandle, MPP_CHN_S *stMppChnAttr, RGN_CHN_ATTR_S *stRgnChnAttr)
{
    HI_S32 ret;
    RGN_ATTR_S stRgnAttr;

    // stRgnAttr.enType = COVER_RGN;

    // stMppChnAttr->enModId = HI_ID_VPSS;
    // stMppChnAttr->s32DevId = 0;
    // stMppChnAttr->s32ChnId = 0;

    // stRgnChnAttr->bShow = HI_TRUE;
    // stRgnChnAttr->enType = COVER_RGN;
    // stRgnChnAttr->unChnAttr.stCoverChn.enCoverType = AREA_RECT;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.s32X = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.s32Y = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.u32Width = 1920;
    // stRgnChnAttr->unChnAttr.stCoverChn.stRect.u32Height = 1080;
    // stRgnChnAttr->unChnAttr.stCoverChn.u32Color = 0xFF4500;
    // stRgnChnAttr->unChnAttr.stCoverChn.u32Layer = 0;
    // stRgnChnAttr->unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR;

    stRgnAttr.enType = COVEREX_RGN;

    stMppChnAttr->enModId = HI_ID_VPSS;
    stMppChnAttr->s32DevId = 0;
    stMppChnAttr->s32ChnId = 0;

    // stRgnChnAttr->bShow = HI_TRUE;
    stRgnChnAttr->bShow = HI_FALSE;
    stRgnChnAttr->enType = COVEREX_RGN;
    stRgnChnAttr->unChnAttr.stCoverExChn.enCoverType = AREA_RECT;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.s32X = 0;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.s32Y = 0;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.u32Width = 1080;
    stRgnChnAttr->unChnAttr.stCoverExChn.stRect.u32Height = 1920;
    // stRgnChnAttr->unChnAttr.stCoverExChn.u32Color = 0xFF4500;
    stRgnChnAttr->unChnAttr.stCoverExChn.u32Color = 0x202020;
    stRgnChnAttr->unChnAttr.stCoverExChn.u32Layer = 0;

    ret = HI_MPI_RGN_Create(rgnHandle, &stRgnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Create failed with %#x!\n", ret);
        return HI_FAILURE;
    }



    // HI_S32 s32Ret;
    // RGN_ATTR_S stRegion;

    // /* OverlayEx */
    // stRegion.enType = OVERLAYEX_RGN;
    // stRegion.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_ARGB_8888;
    // stRegion.unAttr.stOverlayEx.stSize.u32Width = 1080;
    // stRegion.unAttr.stOverlayEx.stSize.u32Height = 1920;
    // stRegion.unAttr.stOverlayEx.u32BgColor = 0b00;
    // stRegion.unAttr.stOverlayEx.u32CanvasNum = 2;

    // s32Ret = HI_MPI_RGN_Create(rgnHandle, &stRegion);
    // if (s32Ret != HI_SUCCESS)
    // {
    //     LOGE("HI_MPI_RGN_Create failed with %#x\n", s32Ret);
    //     return HI_FAILURE;
    // }

    // stMppChnAttr->enModId = HI_ID_VPSS;
    // stMppChnAttr->s32ChnId = 0;
    // stMppChnAttr->s32DevId = 0;

    // stRgnChnAttr->bShow = HI_TRUE;
    // stRgnChnAttr->enType = OVERLAYEX_RGN;
    // stRgnChnAttr->unChnAttr.stOverlayExChn.stPoint.s32X = 0;
    // stRgnChnAttr->unChnAttr.stOverlayExChn.stPoint.s32Y = 0;
    // stRgnChnAttr->unChnAttr.stOverlayExChn.u32FgAlpha = 255;
    // stRgnChnAttr->unChnAttr.stOverlayExChn.u32BgAlpha = 0;
    // stRgnChnAttr->unChnAttr.stOverlayExChn.u32Layer = 0;

    return HI_SUCCESS;
}

HI_S32 DestroyCoverRegion(RGN_HANDLE rgnHandle)
{
    HI_S32 ret;
    ret = HI_MPI_RGN_Destroy(rgnHandle);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Destroy failed with %#x!\n", ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 AttachCover(RGN_HANDLE rgnHandle, const MPP_CHN_S *stMppChnAttr, const RGN_CHN_ATTR_S *stRgnChnAttr)
{
    HI_S32 ret;

    ret = HI_MPI_RGN_AttachToChn(rgnHandle, stMppChnAttr, stRgnChnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_AttachToChn failed with %#x!\n", ret);
        return HI_FAILURE;
    }

    // LOGI("cover_add!\n");
    return HI_SUCCESS;
}

HI_S32 DetachCover(RGN_HANDLE rgnHandle, const MPP_CHN_S *stMppChnAttr)
{
    HI_S32 ret;

    ret = HI_MPI_RGN_DetachFromChn(rgnHandle, stMppChnAttr);
    if (ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_DetachFromChn failed with %#x\n", ret);
        return HI_FAILURE;
    }

    // LOGI("cover_remove!\n");
    return HI_SUCCESS;
}

void LOTO_COVER_InitCoverRegion()
{
    CreateCoverRegion(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    AttachCover(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
}

void LOTO_COVER_UninitCoverRegion()
{
    DetachCover(gs_rgnHandle, &gs_stMppChnAttr);
    DestroyCoverRegion(gs_rgnHandle);
}

// HI_S32 LOTO_COVER_AttachCover()
// {
//     return AttachCover(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
// }

// HI_S32 LOTO_COVER_DetachCover()
// {
//     return DetachCover(gs_rgnHandle, &gs_stMppChnAttr);
// }

HI_S32 LOTO_COVER_AddCover() {
    HI_S32 ret;

    ret = HI_MPI_RGN_GetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_GetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 0;
    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 0;

    ret = HI_MPI_RGN_SetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_SetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    LOGI("cover_add!\n");

    return ret;
}

HI_S32 LOTO_COVER_RemoveCover() {
    HI_S32 ret;

    ret = HI_MPI_RGN_GetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_GetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 1080;
    gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 0;

    ret = HI_MPI_RGN_SetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
    if (ret != HI_SUCCESS) {
        LOGE("HI_MPI_RGN_SetDisplayAttr failed with %#x\n", ret);
        return ret;
    }

    LOGI("cover_remove!\n");

    return ret;
}

static int gs_cover_switch = 0;
static int gs_cover_state = COVER_OFF;

void LOTO_COVER_Switch(int state) {
    if (gs_cover_state != state) {
        gs_cover_switch = 1;
        gs_cover_state = state;
    }
}

// HI_S32 LOTO_COVER_ChangeCover() {
//     int ret = 0;
//     if (gs_cover_switch == 0) {
//         return 0;
//     }

//     ret = HI_MPI_RGN_GetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
//     if (ret != HI_SUCCESS) {
//         LOGE("HI_MPI_RGN_GetDisplayAttr failed with %#x\n", ret);
//         return ret;
//     }

//     // gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.s32X = gs_cover_state == 1 ? 0 : 1080;
//     // gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.u32Width = gs_cover_state == 1 ? 1080 : 2;
//     // gs_stRgnChnAttr.unChnAttr.stCoverExChn.stRect.u32Height = gs_cover_state == 1 ? 1920 : 2;
//     gs_stRgnChnAttr.bShow = gs_cover_state == 1 ? HI_TRUE : HI_FALSE;

//     ret = HI_MPI_RGN_SetDisplayAttr(gs_rgnHandle, &gs_stMppChnAttr, &gs_stRgnChnAttr);
//     if (ret != HI_SUCCESS) {
//         LOGE("HI_MPI_RGN_SetDisplayAttr failed with %#x\n", ret);
//         return ret;
//     }

//     gs_cover_switch = 0;

// }


HI_S32 LOTO_COVER_ChangeCover(int* state) {
    if (gs_cover_switch == 0) {
        return 0;
    }

    *state = gs_cover_state;
    LOGI("cover_state = %d\n", gs_cover_state);

    gs_cover_switch = 0;

    return *state;

}