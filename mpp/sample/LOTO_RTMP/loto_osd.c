/**
 * @file Loto_osd.c
 * @copyright 2022, Lotogram Tech. Co., Ltd.
 * @brief Add video OSD on hi3516dv300
 * @version 0.1
 * @date 2022-10-19
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "loto_osd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/timeb.h>

#include "hi_comm_video.h"
#include "loto_comm.h"
#include "common.h"

#define OSD_HANDLE_DEVICENUM 1
#define OSD_HANDLE_TIMESTAMP 2

#define FILE_COUNT 12

static HI_U8 *bmpBuffer[FILE_COUNT];

static OSD_BITMAPFILEHEADER gs_bmpFileHeader[FILE_COUNT];
static OSD_BITMAPINFO gs_bmpInfo[FILE_COUNT];

OSD_COMP_INFO s_OSDCompInfo[OSD_COLOR_FMT_BUTT] = {
    {0, 4, 4, 4}, /*RGB444*/
    {4, 4, 4, 4}, /*ARGB4444*/
    {0, 5, 5, 5}, /*RGB555*/
    {0, 5, 6, 5}, /*RGB565*/
    {1, 5, 5, 5}, /*ARGB1555*/
    {0, 0, 0, 0}, /*RESERVED*/
    {0, 8, 8, 8}, /*RGB888*/
    {8, 8, 8, 8}  /*ARGB8888*/
};

HI_U16 OSD_MAKECOLOR_U16(HI_U8 r, HI_U8 g, HI_U8 b, OSD_COMP_INFO compinfo)
{
    HI_U8 r1, g1, b1;
    HI_U16 pixel = 0;
    HI_U32 tmp = 15;

    r1 = g1 = b1 = 0;
    r1 = r >> (8 - compinfo.rlen);
    g1 = g >> (8 - compinfo.glen);
    b1 = b >> (8 - compinfo.blen);
    while (compinfo.alen)
    {
        pixel |= (1 << tmp);
        tmp--;
        compinfo.alen--;
    }

    pixel |= (r1 | (g1 << compinfo.blen) | (b1 << (compinfo.blen + compinfo.glen)));
    return pixel;
}

HI_U16 OSD_MAKECOLOR_ALPHA_U32(HI_U8 a, HI_U8 r, HI_U8 g, HI_U8 b, OSD_COMP_INFO compinfo)
{
    HI_U8 r1, g1, b1;
    HI_U16 pixel = 0;

    r1 = g1 = b1 = 0;
    r1 = r >> (8 - compinfo.rlen);
    g1 = g >> (8 - compinfo.glen);
    b1 = b >> (8 - compinfo.blen);
    if (a > 10)
    {
        pixel = 0x8000;
    }

    pixel |= (r1 | (g1 << compinfo.blen) | (b1 << (compinfo.blen + compinfo.glen)));
    return pixel;
}

/**
 * @brief Create OSD region
 *
 * @param handle region's handle (one & only)
 * @return HI_S32 Errors Codes
 */
HI_S32 LOTO_OSD_REGION_Create(RGN_HANDLE handle, HI_U32 canvasWidth, HI_U32 canvasHeight)
{
    HI_S32 s32Ret;
    RGN_ATTR_S stRegion;

    /* Overlay */
    // stRegion.enType	= OVERLAY_RGN;
    // stRegion.unAttr.stOverlay.enPixelFmt			= PIXEL_FORMAT_ARGB_1555;
    // stRegion.unAttr.stOverlay.stSize.u32Width		= 2 * 8 * 20;
    // stRegion.unAttr.stOverlay.stSize.u32Height		= 2 * 11;
    // stRegion.unAttr.stOverlay.u32CanvasNum			= 2; /* recommend: 2 */
    // stRegion.unAttr.stOverlay.u32BgColor			= 0b01; /* Bit[1] specifies the background transparency, and bit[0] specifies the background color */

    /* OverlayEx */
    stRegion.enType = OVERLAYEX_RGN;
    stRegion.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_ARGB_8888;
    stRegion.unAttr.stOverlayEx.stSize.u32Width = canvasWidth;
    stRegion.unAttr.stOverlayEx.stSize.u32Height = canvasHeight;
    stRegion.unAttr.stOverlayEx.u32BgColor = 0b00;
    stRegion.unAttr.stOverlayEx.u32CanvasNum = 2;

    s32Ret = HI_MPI_RGN_Create(handle, &stRegion);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Create failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    if (OSD_HANDLE_DEVICENUM == handle)
    {
        LOGI("Create Device_Num OSD region success!\n");
    }
    else if (OSD_HANDLE_TIMESTAMP == handle)
    {
        LOGI("Create Timestamp OSD region success!\n");
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_REGION_Destroy(RGN_HANDLE handle)
{
    HI_S32 s32Ret;

    s32Ret = HI_MPI_RGN_Destroy(handle);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("HI_MPI_RGN_Destroy failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }
    LOGE("Destroy OSD region success!\n");
    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_REGION_AttachToChn(RGN_HANDLE handle, HI_S32 canvasLocX, HI_S32 canvasLocY)
{
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    RGN_CHN_ATTR_S stChnAttr;

    /* Overlay */
    /* stChn.enModId	= HI_ID_VENC;
    stChn.s32DevId	= 0;
    stChn.s32ChnId	= 0;

    stChnAttr.bShow		= HI_TRUE;
    stChnAttr.enType 	= OVERLAY_RGN;

    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X	= 16 * 6;
    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y	= 16 * 15;
    stChnAttr.unChnAttr.stOverlayChn.u32Layer		= 1;

    stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;
    stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;

    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable 	= HI_FALSE;
    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp 		= HI_FALSE;
    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp  		= 0;

    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height 	= 16;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width 	= 16;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh 			= 128;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod 				= LESSTHAN_LUM_THRESH;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn 				= HI_FALSE;

    stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[0] 	= 0x2abc;
    stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[1] 	= 0x7FF0;
    stChnAttr.unChnAttr.stOverlayChn.enAttachDest 		= ATTACH_JPEG_MAIN; */

    /* OverlayEx */
    stChn.enModId = HI_ID_VPSS;
    stChn.s32ChnId = 0;
    stChn.s32DevId = 0;

    stChnAttr.bShow = HI_TRUE;
    stChnAttr.enType = OVERLAYEX_RGN;

    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = canvasLocX;
    stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = canvasLocY;
    stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha = 255;
    stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha = 0;
    stChnAttr.unChnAttr.stOverlayExChn.u32Layer = 1;
    // stChnAttr.unChnAttr.stOverlayExChn.u16ColorLUT[0]			= 0x7fff;
    // stChnAttr.unChnAttr.stOverlayExChn.u16ColorLUT[1]			= 0x7fff;

    s32Ret = HI_MPI_RGN_AttachToChn(handle, &stChn, &stChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_RGN_AttachToChn failed with %#x!\n", s32Ret);
        LOTO_OSD_REGION_Destroy(handle);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_CheckBmpInfo(OSD_BITMAPINFO *pBmpInfo)
{
    HI_U16 Bpp;

    Bpp = pBmpInfo->bmiHeader.biBitCount / 8;
    if (Bpp < 2)
    {
        /* only support 1555.8888  888 bitmap */
        LOGE("bitmap format not supported!\n");
        return -1;
    }

    if (pBmpInfo->bmiHeader.biCompression != 0)
    {
        LOGE("not support compressed bitmap file!\n");
        LOGE("bitCompression:%d\n", pBmpInfo->bmiHeader.biCompression);
        return -1;
    }

    if (pBmpInfo->bmiHeader.biHeight < 0)
    {
        LOGE("bmpInfo.bmiHeader.biHeight < 0\n");
        return -1;
    }
    return 0;
}

HI_S32 LOTO_OSD_GetBmpInfo(const char *filename, OSD_BITMAPFILEHEADER *pBmpFileHeader, OSD_BITMAPINFO *pBmpInfo)
{
    FILE *pFile;

    HI_U16 bfType;

    if (NULL == filename)
    {
        LOGE("OSD_LoadBMP: filename=NULL\n");
        return -1;
    }

    if ((pFile = fopen((char *)filename, "rb")) == NULL)
    {
        LOGE("Open file faild:%s!\n", filename);
        return -1;
    }

    (void)fread(&bfType, 1, sizeof(bfType), pFile);
    if (bfType != 0x4d42)
    {
        LOGE("not bitmap file\n");
        fclose(pFile);
        return -1;
    }

    (void)fread(pBmpFileHeader, 1, sizeof(OSD_BITMAPFILEHEADER), pFile);
    (void)fread(pBmpInfo, 1, sizeof(OSD_BITMAPINFO), pFile);
    fclose(pFile);

    LOTO_OSD_CheckBmpInfo(pBmpInfo);

    return 0;
}

extern char g_device_num[16];

HI_VOID LOTO_OSD_GetDeviceNum(HI_S16 *deviceNum)
{
    for (int i = 0; i < 3; i++)
    {
        switch (g_device_num[i])
        {
        case '0':
            deviceNum[i] = 0;
            break;
        case '1':
            deviceNum[i] = 1;
            break;
        case '2':
            deviceNum[i] = 2;
            break;
        case '3':
            deviceNum[i] = 3;
            break;
        case '4':
            deviceNum[i] = 4;
            break;
        case '5':
            deviceNum[i] = 5;
            break;
        case '6':
            deviceNum[i] = 6;
            break;
        case '7':
            deviceNum[i] = 7;
            break;
        case '8':
            deviceNum[i] = 8;
            break;
        case '9':
            deviceNum[i] = 9;
            break;
        default:
            LOGE("Get device number character [%d] error!\n", i);
        }
    }
}

HI_VOID LOTO_OSD_GetTimeNum(HI_S16 *timeNum)
{
    time_t timep;
    struct tm *pLocalTime;
    time(&timep);

#if 1
    pLocalTime = localtime(&timep);
#else
    struct tm tmLoc;
    timep += global_get_cfg_apptime_zone_seconds();
    global_convert_time(timep, 0, &tmLoc);
    pLocalTime = &tmLoc;
#endif

    /* year */
    timeNum[0] = (1900 + pLocalTime->tm_year) / 1000;
    timeNum[1] = ((1900 + pLocalTime->tm_year) / 100) % 10;
    timeNum[2] = ((1900 + pLocalTime->tm_year) % 100) / 10;
    timeNum[3] = (1900 + pLocalTime->tm_year) % 10;

    /* dash */
    timeNum[4] = -1;

    /* month */
    timeNum[5] = (1 + pLocalTime->tm_mon) / 10;
    timeNum[6] = (1 + pLocalTime->tm_mon) % 10;

    /* dash */
    timeNum[7] = -1;

    /* day */
    timeNum[8] = (pLocalTime->tm_mday) / 10;
    timeNum[9] = (pLocalTime->tm_mday) % 10;

    /* hour */
    timeNum[10] = (pLocalTime->tm_hour) / 10;
    timeNum[11] = (pLocalTime->tm_hour) % 10;

    /* colon */
    timeNum[12] = -1;

    /* minute */
    timeNum[13] = (pLocalTime->tm_min) / 10;
    timeNum[14] = (pLocalTime->tm_min) % 10;

    /* colon */
    timeNum[15] = -1;

    /* second */
    timeNum[16] = (pLocalTime->tm_sec) / 10;
    timeNum[17] = (pLocalTime->tm_sec) % 10;
}

HI_S32 LOTO_OSD_OpenBmpFile(FILE **pFile, const char **fileName, HI_S32 fileCount)
{
    for (int i = 0; i < fileCount; i++)
    {
        if ((*(pFile + i) = fopen(*(fileName + i), "rb")) == NULL)
        {
            LOGE("Open file faild:%s!\n", *(fileName + i));
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_CloseBmpFile(FILE **pFile, HI_S32 fileCount)
{
    for (int i = 0; i < fileCount; i++)
    {
        if (HI_SUCCESS != fclose(*(pFile + i)))
        {
            return HI_FAILURE;
        };
    }
    return HI_SUCCESS;
}

/**
 * @brief 判断获取到的位图大小是否超出画布容纳范围
 *
 * @param pCanvas 画布指针
 * @param bmpWidth 位图宽度
 * @param bmpHeight 位图高度
 * @param stride 位图跨度
 * @return HI_VOID
 */
HI_S32 LOTO_OSD_CheckBmpSize(OSD_LOGO_T *pCanvas, HI_U32 bmpWidth, HI_U32 bmpHeight, HI_U32 stride)
{
    if (stride > pCanvas->stride)
    {
        LOGE("Bitmap's stride(%d) is bigger than canvas's stide(%d). Load bitmap error!\n", stride, pCanvas->stride);
        return -1;
    }

    if (bmpHeight > pCanvas->height)
    {
        LOGE("Bitmap's height(%d) is bigger than canvas's height(%d). Load bitmap error!\n", bmpHeight, pCanvas->height);
        return -1;
    }

    if (bmpWidth > pCanvas->width)
    {
        LOGE("Bitmap's width(%d) is bigger than canvas's width(%d). Load bitmap error!\n", bmpWidth, pCanvas->width);
        return -1;
    }
    return 0;
}

#ifdef OSD_HANDLE_LOGO
HI_S32 LOTO_OSD_LoadBmp_Single(const char *filename, OSD_LOGO_T *pCanvas, OSD_COLOR_FMT_E enFmt)
{
    FILE *pFile;
    HI_U16 i, j;

    HI_U32 bmpWidth, bmpHeight;
    HI_U16 Bpp;

    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    HI_U8 *pBitmapAddr;
    HI_U8 *pCanvasAddr;
    HI_U32 stride;
    HI_U8 a, r, g, b;
    HI_U8 *pStart;
    HI_U16 *pDst;

    if (NULL == filename)
    {
        LOGE("OSD_LoadBMP: filename=NULL\n");
        return -1;
    }

    if (LOTO_OSD_GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0)
    {
        return -1;
    }

    Bpp = bmpInfo.bmiHeader.biBitCount / 8;
    LOGE("Bpp = %d\n", Bpp);

    if (Bpp < 2)
    {
        /* only support 1555.8888  888 bitmap */
        printf("[%s]-%d: bitmap format not supported!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (bmpInfo.bmiHeader.biCompression != 0)
    {
        printf("[%s]-%d: not support compressed bitmap file!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (bmpInfo.bmiHeader.biHeight < 0)
    {
        printf("[%s]-%d: bmpInfo.bmiHeader.biHeight < 0\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if ((pFile = fopen((char *)filename, "rb")) == NULL)
    {
        printf("[%s]-%d: Open file faild:%s!\n", __FUNCTION__, __LINE__, filename);
        return -1;
    }

    bmpWidth = (HI_U16)bmpInfo.bmiHeader.biWidth;
    bmpHeight = (HI_U16)((bmpInfo.bmiHeader.biHeight > 0) ? bmpInfo.bmiHeader.biHeight : (-bmpInfo.bmiHeader.biHeight));

    stride = bmpWidth * Bpp;

#if 1
    if (stride % 4)
    {
        stride = (stride & 0xfffc) + 4;
    }
#endif

    /* RGB8888 or RGB1555 */
    pBitmapAddr = (HI_U8 *)malloc(bmpHeight * stride);
    if (NULL == pBitmapAddr)
    {
        printf("not enough memory to malloc!\n");
        fclose(pFile);
        return -1;
    }

    pCanvasAddr = pCanvas->pRGBBuffer;

    if (stride > pCanvas->stride)
    {
        printf("[%s]-%d: Bitmap's stride(%d) is bigger than canvas's stide(%d). Load bitmap error!\n", __FUNCTION__, __LINE__, stride, pCanvas->stride);
        free(pBitmapAddr);
        fclose(pFile);
        return -1;
    }

    if (bmpHeight > pCanvas->height)
    {
        printf("[%s]-%d: Bitmap's height(%d) is bigger than canvas's height(%d). Load bitmap error!\n", __FUNCTION__, __LINE__, bmpHeight, pCanvas->height);
        free(pBitmapAddr);
        fclose(pFile);
        return -1;
    }

    if (bmpWidth > pCanvas->width)
    {
        printf("[%s]-%d: Bitmap's width(%d) is bigger than canvas's width(%d). Load bitmap error!\n", __FUNCTION__, __LINE__, bmpWidth, pCanvas->width);
        free(pBitmapAddr);
        fclose(pFile);
        return -1;
    }

    fseek(pFile, bmpFileHeader.bfOffBits, 0);
    if (fread(pBitmapAddr, 1, bmpHeight * stride, pFile) != (bmpHeight * stride))
    {
        LOGE("fread (%d*%d)error!\n", bmpHeight, stride);
        perror("fread:");
    }

    for (i = 0; i < bmpHeight; i++)
    {
        for (j = 0; j < bmpWidth; j++)
        {
            if (Bpp == 4)
            {
                switch (enFmt)
                {
                case OSD_COLOR_FMT_RGB444:
                case OSD_COLOR_FMT_RGB555:
                case OSD_COLOR_FMT_RGB565:
                case OSD_COLOR_FMT_RGB1555:
                case OSD_COLOR_FMT_RGB4444:
                    /* start color convert */
                    pStart = pBitmapAddr + ((bmpHeight - 1) - i) * stride + j * Bpp;
                    pDst = (HI_U16 *)(pCanvasAddr + i * pCanvas->stride + j * 2);
                    r = *(pStart);
                    g = *(pStart + 1);
                    b = *(pStart + 2);
                    a = *(pStart + 3);
                    // printf("Func: %s, line:%d, Bpp: %d, bmp stride: %d, Canvas stride: %d, bmpHeight:%d, bmpWidth:%d.\n",
                    //     __FUNCTION__, __LINE__, Bpp, stride, pCanvas->stride, i, j);
                    *pDst = OSD_MAKECOLOR_ALPHA_U32(a, r, g, b, s_OSDCompInfo[enFmt]);
                    break;

                case OSD_COLOR_FMT_RGB888:
                case OSD_COLOR_FMT_RGB8888:
                    memcpy(pCanvasAddr + i * pCanvas->stride + j * 4, pBitmapAddr + ((bmpHeight - 1) - i) * stride + j * Bpp, Bpp);
                    //*(pCanvasAddr + i * pCanvas->stride + j * 4 + 3) = 0xff; /*alpha*/
                    break;

                default:
                    printf("file(%s), line(%d), no such format!\n", __FILE__, __LINE__);
                    break;
                }
            }
            else if (Bpp == 3)
            {
                switch (enFmt)
                {
                case OSD_COLOR_FMT_RGB444:
                case OSD_COLOR_FMT_RGB555:
                case OSD_COLOR_FMT_RGB565:
                case OSD_COLOR_FMT_RGB1555:
                case OSD_COLOR_FMT_RGB4444:
                    /* start color convert */
                    pStart = pBitmapAddr + ((bmpHeight - 1) - i) * stride + j * Bpp;
                    pDst = (HI_U16 *)(pCanvasAddr + i * pCanvas->stride + j * 2);
                    r = *(pStart);
                    g = *(pStart + 1);
                    b = *(pStart + 2);
                    a = *(pStart + 3);
                    // printf("Func: %s, line:%d, Bpp: %d, bmp stride: %d, Canvas stride: %d, bmpHeight:%d, bmpWidth:%d.\n",
                    //     __FUNCTION__, __LINE__, Bpp, stride, pCanvas->stride, i, j);
                    *pDst = OSD_MAKECOLOR_U16(r, g, b, s_OSDCompInfo[enFmt]);
                    break;

                case OSD_COLOR_FMT_RGB888:
                case OSD_COLOR_FMT_RGB8888:
                    memcpy(pCanvasAddr + i * pCanvas->stride + j * 4, pBitmapAddr + ((bmpHeight - 1) - i) * stride + j * Bpp, Bpp);
                    *(pCanvasAddr + i * pCanvas->stride + j * 4 + 3) = 0xff; /*alpha*/
                    break;

                default:
                    printf("file(%s), line(%d), no such format!\n", __FILE__, __LINE__);
                    break;
                }
            }
            else if ((Bpp == 2) || (Bpp == 4)) /*..............*/
            {
                memcpy(pCanvasAddr + i * pCanvas->stride + j * Bpp, pBitmapAddr + ((bmpHeight - 1) - i) * stride + j * Bpp, Bpp);
            }
        }
    }

    free(pBitmapAddr);
    pBitmapAddr = NULL;

    fclose(pFile);
    return 0;
}
#endif

HI_S32 LOTO_OSD_LoadBmp_DeviceNum(OSD_LOGO_T *pCanvas, OSD_COLOR_FMT_E enFmt)
{
    HI_U16 row;
    // HI_U16  column;

    HI_U32 bmpWidth, bmpHeight;
    HI_U16 Bpp;				  // Byte per pixel
    HI_U16 space_32 = 4 * 16; // bpp * pixels

    OSD_BITMAPINFO bmpInfoTem;
    HI_U8 *pBitmapAddr; // bitmap address
    HI_U8 *pCanvasAddr; // canvas address
    HI_U16 stride;		// 加载的位图的一行像素的字节量

    HI_U8 *pSrcPixel;
    HI_U8 *pDstPixel;

    HI_S16 deviceNum[3];

    pCanvasAddr = pCanvas->pRGBBuffer;
    if (pCanvasAddr == NULL)
    {
        LOGE("Get canvas address error!\n");
        return HI_FAILURE;
    }

    LOTO_OSD_GetDeviceNum(deviceNum);

    for (int k = 0; k < 3; k++)
    {
        bmpInfoTem = gs_bmpInfo[deviceNum[k]];
        pBitmapAddr = bmpBuffer[deviceNum[k]];

        if (pBitmapAddr == NULL)
        {
            LOGE("Get bitmap [%d] address error!\n", k);
            return HI_FAILURE;
        }

        /* bitmap infomation */
        Bpp = bmpInfoTem.bmiHeader.biBitCount / 8;
        // LOGD("Bpp = %d\n", Bpp);

        bmpWidth = (HI_U16)bmpInfoTem.bmiHeader.biWidth;
        bmpHeight = (HI_U16)((bmpInfoTem.bmiHeader.biHeight > 0) ? bmpInfoTem.bmiHeader.biHeight : (-bmpInfoTem.bmiHeader.biHeight));
        // LOGD("bmpWidth:%d, bmpHeight:%d \n", bmpWidth, bmpHeight);

        /* stride 按 4 补齐 */
        stride = bmpWidth * Bpp;
        if (stride % 4)
        {
            stride = (stride & 0xfffc) + 4;
        }

        HI_U16 strideSum = 0;
        strideSum += stride;
        if (strideSum > pCanvas->stride)
        {
            LOGE("strideSum is wider!\n");
            return HI_FAILURE;
        }

        if (enFmt == OSD_COLOR_FMT_RGB8888)
        {
            /* pixel by pixel */
            /* for (row = 0; row < bmpHeight; row++) {
                for (column = 0; column < bmpWidth; column++) {
                    pSrcPixel = pBitmapAddr + ((bmpHeight - 1) - row) * stride + column * Bpp;
                    pDstPixel = pCanvasAddr + row * pCanvas->stride + column * Bpp + k * space_32;
                    if (k >= 10) {
                        pDstPixel += Bpp * 16;
                    }
                    pDstPixel[0] = pSrcPixel[0]; // r
                    pDstPixel[1] = pSrcPixel[1]; // g
                    pDstPixel[2] = pSrcPixel[2]; // b
                    pDstPixel[3] = pSrcPixel[3]; // a
                }
            } */

            /* stride by stride */
            for (row = 0; row < bmpHeight; row++)
            {
                pSrcPixel = pBitmapAddr + ((bmpHeight - 1) - row) * stride;
                pDstPixel = pCanvasAddr + row * pCanvas->stride + k * space_32;
                memcpy(pDstPixel, pSrcPixel, stride);
            }
        }
        else
        {
            LOGE("The pixel format is not supported!\n");
        }
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_LoadBmp_Timestamp(OSD_LOGO_T *pCanvas, OSD_COLOR_FMT_E enFmt)
{
    // HI_S32 	s32Ret;

    HI_U16 row; // i: row
    // HI_U16  colum;		// j: column
    HI_U16 k; // k: bitmap num
    HI_U32 bmpWidth, bmpHeight;
    HI_U16 Bpp;				  // Byte per pixel
    HI_U16 space_32 = 4 * 16; // bpp * pixels

    OSD_BITMAPINFO bmpInfoTem;
    HI_U8 *pBitmapAddr; // bitmap address
    HI_U8 *pCanvasAddr; // canvas address
    HI_U16 stride;		// 加载的位图的一行像素的字节量

    HI_U8 *pSrcPixel;
    HI_U8 *pDstPixel;

    HI_S16 timeNum[18];

    pCanvasAddr = pCanvas->pRGBBuffer; // 画布虚拟地址
    if (pCanvasAddr == NULL)
    {
        LOGE("Get canvas address error!\n");
        return HI_FAILURE;
    }

    LOTO_OSD_GetTimeNum(timeNum);

    // YYYY-MM-DD HH:MM:SS
    for (k = 0; k < 18; k++)
    {
        if (k == 4 || k == 7)
        {
            // bmpFileHeaderTem = bmpFileHeader[10];
            bmpInfoTem = gs_bmpInfo[10];
            pBitmapAddr = bmpBuffer[10];
        }
        else if (k == 12 || k == 15)
        {
            // bmpFileHeaderTem = bmpFileHeader[11];
            bmpInfoTem = gs_bmpInfo[11];
            pBitmapAddr = bmpBuffer[11];
        }
        else
        {
            if (timeNum[k] != -1)
            {
                // bmpFileHeaderTem = bmpFileHeader[timeNum[k]];
                bmpInfoTem = gs_bmpInfo[timeNum[k]];
                pBitmapAddr = bmpBuffer[timeNum[k]];
            }
            else
            {
                LOGE("Get time infomation error!\n");
                return HI_FAILURE;
            }
        }

        if (pBitmapAddr == NULL)
        {
            LOGE("Get bitmap address error!\n");
            return HI_FAILURE;
        }

        /* bitmap infomation */
        Bpp = bmpInfoTem.bmiHeader.biBitCount / 8;
        // LOGE("Bpp = %d\n", Bpp);

        bmpWidth = (HI_U16)bmpInfoTem.bmiHeader.biWidth;
        bmpHeight = (HI_U16)((bmpInfoTem.bmiHeader.biHeight > 0) ? bmpInfoTem.bmiHeader.biHeight : (-bmpInfoTem.bmiHeader.biHeight));
        // LOGE("bmpWidth:%d, bmpHeight:%d \n", bmpWidth, bmpHeight);

        /* stride 按 4 补齐 */
        stride = bmpWidth * Bpp;
        if (stride % 4)
        {
            stride = (stride & 0xfffc) + 4;
        }

        HI_U16 strideSum = 0;
        strideSum += stride;
        if (strideSum > pCanvas->stride)
        {
            LOGE("strideSum is wider!\n");
            return HI_FAILURE;
        }

        if (enFmt == OSD_COLOR_FMT_RGB8888)
        {
            /* pixel by pixel */
            /* for (i = 0; i < bmpHeight; i++) {
                for (column = 0; column < bmpWidth; column++) {
                    pSrcPixel = pBitmapAddr + ((bmpHeight - 1) - row) * stride + column * Bpp;
                    pDstPixel = pCanvasAddr + row * pCanvas->stride + column * Bpp + k * space_32;
                    if (k >= 10) {
                        pDstPixel += Bpp * 16;
                    }
                    pDstPixel[0] = pSrcPixel[0]; // r
                    pDstPixel[1] = pSrcPixel[1]; // g
                    pDstPixel[2] = pSrcPixel[2]; // b
                    pDstPixel[3] = pSrcPixel[3]; // a
                }
            } */

            /* stride by stride */
            for (row = 0; row < bmpHeight; row++)
            {
                pSrcPixel = pBitmapAddr + ((bmpHeight - 1) - row) * stride;
                pDstPixel = pCanvasAddr + row * pCanvas->stride + k * space_32;
                if (k >= 10)
                {
                    pDstPixel += Bpp * 16;
                }
                memcpy(pDstPixel, pSrcPixel, stride);
            }
        }
        else
        {
            LOGE("The pixel format is not supported!\n");
        }
    }
    return HI_SUCCESS;
}

/**
 * @brief 加载BMP图
 *
 * @param handle 		画布句柄
 * @param pCanvas
 * @param enFmt 		像素格式
 * @return HI_S32
 */
HI_S32 LOTO_OSD_LoadBmp(RGN_HANDLE handle, OSD_LOGO_T *pCanvas, OSD_COLOR_FMT_E enFmt)
{
    HI_S32 s32Ret = 0;

    if (handle == OSD_HANDLE_DEVICENUM)
    {
        s32Ret = LOTO_OSD_LoadBmp_DeviceNum(pCanvas, enFmt);
    }
    else if (handle == OSD_HANDLE_TIMESTAMP)
    {
        s32Ret = LOTO_OSD_LoadBmp_Timestamp(pCanvas, enFmt);
    }

    if (s32Ret != HI_SUCCESS)
    {
        LOGE("load bmp error!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/**
 * @brief 添加图层
 *
 * @param handle 		画布句柄
 * @param ptrSurface 	图层
 * @param pu8Virt 		画布虚拟地址
 * @param u32Width 		画布宽度
 * @param u32Height 	画布高度
 * @param u32Stride 	画布跨距
 * @return HI_S32
 */
HI_S32 LOTO_OSD_CreateSurfaceByCanvas(RGN_HANDLE handle, OSD_SURFACE_S *ptrSurface, HI_U8 *pu8Virt, HI_U32 u32Width, HI_U32 u32Height, HI_U32 u32Stride)
{
    HI_S32 s32Ret;

    OSD_LOGO_T pCanvas;
    pCanvas.pRGBBuffer = pu8Virt; // 画布虚拟数据地址 pstBitmap->pData = stCanvasInfo.u64VirtAddr;
    pCanvas.width = u32Width;
    pCanvas.height = u32Height;
    pCanvas.stride = u32Stride;

    s32Ret = LOTO_OSD_LoadBmp(handle, &pCanvas, ptrSurface->enColorFmt);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_LoadBmp error!\n");
        return HI_FAILURE;
    }

    ptrSurface->u16Height = u32Height;
    ptrSurface->u16Width = u32Width;
    ptrSurface->u16Stride = u32Stride;

    return HI_SUCCESS;
}

/**
 * @brief 更新画布信息
 *
 * @param handle 		画布句柄
 * @param pstBitmap 	位图指针，保存位图数据的地址为画布的虚拟地址
 * @param bFil 			是否需要填充颜色
 * @param u16FilColor 	需要填充的颜色值
 * @param pstSize 		画布大小
 * @param u32Stride 	画布跨距
 * @param enPixelFmt 	像素格式
 * @return HI_S32 		0: success; other: error codes
 */
HI_S32 LOTO_OSD_UpdateCanvasInfo(RGN_HANDLE handle, BITMAP_S *pstBitmap, HI_BOOL bFil, HI_U32 u16FilColor, SIZE_S *pstSize, HI_U32 u32Stride, PIXEL_FORMAT_E enPixelFmt)
{
    OSD_SURFACE_S Surface;

    if (PIXEL_FORMAT_ARGB_1555 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    }
    else if (PIXEL_FORMAT_ARGB_4444 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    }
    else if (PIXEL_FORMAT_ARGB_8888 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    }
    else
    {
        LOGE("Pixel format is not support!\n");
        return HI_FAILURE;
    }

    if (NULL == pstBitmap->pData)
    {
        LOGE("malloc osd memroy err!\n");
        return HI_FAILURE;
    }

    LOTO_OSD_CreateSurfaceByCanvas(handle, &Surface, (HI_U8 *)(pstBitmap->pData), pstSize->u32Width, pstSize->u32Height, u32Stride);

    pstBitmap->u32Width = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;

    if (PIXEL_FORMAT_ARGB_1555 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_1555;
    }
    else if (PIXEL_FORMAT_ARGB_4444 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_4444;
    }
    else if (PIXEL_FORMAT_ARGB_8888 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_8888;
    }

    int i = 0, j = 0;
    HI_U16 *pu16Temp = NULL;
    pu16Temp = (HI_U16 *)pstBitmap->pData;

    if (bFil)
    {
        // LOGD("@@@@----H:%d, W:%d \n", pstBitmap->u32Height,pstBitmap->u32Width);
        for (i = 0; i < pstBitmap->u32Height; i++)
        {
            for (j = 0; j < pstBitmap->u32Width; j++)
            {
                // LOGD("(%d,%d): %04x!\n", i,j,*pu16Temp);

                // if (u16FilColor == *pu16Temp)
                if (0x0000 == *pu16Temp || 0xFFFF == *pu16Temp)
                { // TODO fixed value, Fun para not work
                    *pu16Temp &= 0x7FFF;
                }
                pu16Temp++;
            }
        }
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_AddBitMap(RGN_HANDLE handle)
{
    HI_S32 s32Ret = HI_SUCCESS;
    RGN_ATTR_S stRgnAttrSet;
    RGN_CANVAS_INFO_S stCanvasInfo;
    BITMAP_S pstBitmap;
    SIZE_S stSize;

    s32Ret = HI_MPI_RGN_GetAttr(handle, &stRgnAttrSet);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_RGN_GetAttr failed with %#x\n", s32Ret);
        return s32Ret;
    }

    s32Ret = HI_MPI_RGN_GetCanvasInfo(handle, &stCanvasInfo);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_RGN_GetCanvasInfo failed with %#x\n", s32Ret);
        return s32Ret;
    }

    pstBitmap.pData = (HI_VOID *)(HI_UL)stCanvasInfo.u64VirtAddr;
    stSize.u32Width = stCanvasInfo.stSize.u32Width;
    stSize.u32Height = stCanvasInfo.stSize.u32Height;

    /* Clean canvas */
    if (NULL != pstBitmap.pData)
    {
        memset(pstBitmap.pData, 0x00, stCanvasInfo.u32Stride * stSize.u32Height);
    }

    // LOGD("Memorry Size:%d\n", stCanvasInfo.u32Stride * stSize.u32Height);

    // LOGD("LOTO_OSD_UpdateCanvasInfo start time: %s\n", GetLocalTime());

    s32Ret = LOTO_OSD_UpdateCanvasInfo(handle, &pstBitmap, HI_FALSE, 0xffffff, &stSize, stCanvasInfo.u32Stride, stRgnAttrSet.unAttr.stOverlay.enPixelFmt);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("LOTO_OSD_UpdateCanvasInfo failed with %#x.\n", s32Ret);
        return s32Ret;
    }

    // LOGE("Canvas Infomation: width = %d, height = %d.\n", stCanvasInfo.stSize.u32Width, stCanvasInfo.stSize.u32Height);
    // LOGE("Canvas Infomation: Stride = %d\n", stCanvasInfo.u32Stride);

    // LOGE("LOTO_OSD_UpdateCanvasInfo end time: %s\n", logTime());

    s32Ret = HI_MPI_RGN_UpdateCanvas(handle);
    if (HI_SUCCESS != s32Ret)
    {
        LOGE("HI_MPI_RGN_UpdateCanvas failed with %#x.\n", s32Ret);
        return s32Ret;
    }

    // LOGE("HI_MPI_RGN_UpdateCanvas finish time: %s\n", logTime());

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_GetBmpBuffer()
{
    HI_S32 s32Ret;

    HI_U16 Bpp;		  // Bytes per Pixel
    HI_U16 bmpWidth;  // bitmap width
    HI_U16 bmpHeight; // bitmap height

    const char *fileName[] = {
        "/root/res/0.bmp", "/root/res/1.bmp", "/root/res/2.bmp", "/root/res/3.bmp", "/root/res/4.bmp",
        "/root/res/5.bmp", "/root/res/6.bmp", "/root/res/7.bmp", "/root/res/8.bmp", "/root/res/9.bmp",
        "/root/res/dash.bmp", "/root/res/colon.bmp"};

    HI_U8 *bmpBufferTem;
    HI_U8 *bmp_buf_zero, *bmp_buf_one, *bmp_buf_two, *bmp_buf_three, *bmp_buf_four,
        *bmp_buf_five, *bmp_buf_six, *bmp_buf_seven, *bmp_buf_eight, *bmp_buf_nine,
        *bmp_buf_dash, *bmp_buf_colon;

    HI_U32 stride;

    FILE *pFile[FILE_COUNT];

    /* 获取BMP文件信息，检测BMP信息是否正确 */
    for (int f = 0; f < FILE_COUNT; f++)
    {
        if (LOTO_OSD_GetBmpInfo(*(fileName + f), gs_bmpFileHeader + f, gs_bmpInfo + f) < 0)
        {
            return -1;
        };
    }

    /* 打开BMP文件 */
    s32Ret = LOTO_OSD_OpenBmpFile(pFile, fileName, FILE_COUNT);
    if (s32Ret != 0)
    {
        LOGE("LOTO_OSD_OpenBmpFile error!\n");
    }

    for (int f = 0; f < FILE_COUNT; f++)
    {
        Bpp = (HI_U16)gs_bmpInfo[f].bmiHeader.biBitCount / 8;
        bmpWidth = (HI_U16)gs_bmpInfo[f].bmiHeader.biWidth;
        bmpHeight = (HI_U16)((gs_bmpInfo[f].bmiHeader.biHeight > 0) ? gs_bmpInfo[f].bmiHeader.biHeight : (-gs_bmpInfo[f].bmiHeader.biHeight));

        stride = bmpWidth * Bpp;
        if (stride % 4)
        {
            stride = (stride & 0xfffc) + 4; // 按4补齐
        }

        // LOGE("stride * bmpHeight = %d\n", stride * bmpHeight);

        bmpBufferTem = (HI_U8 *)malloc(stride * bmpHeight);
        if (NULL == bmpBufferTem)
        {
            LOGE("Not enough memory to allocate!\n");
            s32Ret = LOTO_OSD_CloseBmpFile(pFile, FILE_COUNT);
            if (s32Ret != 0)
            {
                LOGE("LOTO_OSD_CloseBmpFile error!\n");
            }
            return -1;
        }

        fseek(*(pFile + f), gs_bmpFileHeader[f].bfOffBits, 0);
        if (fread(bmpBufferTem, 1, bmpHeight * stride, *(pFile + f)) != (bmpHeight * stride))
        {
            LOGE("fread (%d*%d)error!\n", bmpHeight, stride);
            perror("fread:");
        }

        switch (f)
        {
        case 0:
            bmp_buf_zero = malloc(bmpHeight * stride);
            memcpy(bmp_buf_zero, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_zero;
            break;
        case 1:
            bmp_buf_one = malloc(bmpHeight * stride);
            memcpy(bmp_buf_one, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_one;
            break;
        case 2:
            bmp_buf_two = malloc(bmpHeight * stride);
            memcpy(bmp_buf_two, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_two;
            break;
        case 3:
            bmp_buf_three = malloc(bmpHeight * stride);
            memcpy(bmp_buf_three, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_three;
            break;
        case 4:
            bmp_buf_four = malloc(bmpHeight * stride);
            memcpy(bmp_buf_four, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_four;
            break;
        case 5:
            bmp_buf_five = malloc(bmpHeight * stride);
            memcpy(bmp_buf_five, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_five;
            break;
        case 6:
            bmp_buf_six = malloc(bmpHeight * stride);
            memcpy(bmp_buf_six, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_six;
            break;
        case 7:
            bmp_buf_seven = malloc(bmpHeight * stride);
            memcpy(bmp_buf_seven, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_seven;
            break;
        case 8:
            bmp_buf_eight = malloc(bmpHeight * stride);
            memcpy(bmp_buf_eight, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_eight;
            break;
        case 9:
            bmp_buf_nine = malloc(bmpHeight * stride);
            memcpy(bmp_buf_nine, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_nine;
            break;
        case 10:
            bmp_buf_dash = malloc(bmpHeight * stride);
            memcpy(bmp_buf_dash, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_dash;
            break;
        case 11:
            bmp_buf_colon = malloc(bmpHeight * stride);
            memcpy(bmp_buf_colon, bmpBufferTem, bmpHeight * stride);
            bmpBuffer[f] = bmp_buf_colon;
            break;
        }
        if (bmpBuffer[f] == NULL)
        {
            LOGE("Get NO.%d bmp buffer error!\n", f);
            return HI_FAILURE;
        }
        // LOGD("Get NO.%d bmp buffer success!\n", f);

        free(bmpBufferTem);
        // bmpBufferTem = NULL;
    }

    /* 关闭BMP文件 */
    s32Ret = LOTO_OSD_CloseBmpFile(pFile, FILE_COUNT);
    if (s32Ret != 0)
    {
        LOGE("LOTO_OSD_CloseBmpFile error!\n");
    }

    return HI_SUCCESS;
}

HI_S32 LOTO_OSD_FreeBmpBuffer()
{
    HI_S32 ret;

    for (int i = 0; i < FILE_COUNT; i++)
    {
        if (bmpBuffer[i] == NULL)
        {
            LOGE("bmp[%d] buffer is empty\n", i);
            return HI_FAILURE;
        }
        else
        {
            free(bmpBuffer[i]);
        }
    }

    return HI_SUCCESS;
}

HI_VOID *LOTO_OSD_AddVideoOsd(void *handle)
{
    HI_S32 s32Ret = HI_SUCCESS;
    time_t timep;
    struct tm *pLocalTime;
    HI_S32 seconds = 80;

    OsdHanlde *handle_info = (OsdHanlde *)handle;

    if (handle == NULL)
    {
        LOGE("OSD handle is empty! \n");
    }

    RGN_HANDLE deviceNumHandle = handle_info->deviceNum;
    RGN_HANDLE timestampHandle = handle_info->timestamp;
    free(handle_info);

    do
    {
        s32Ret = LOTO_OSD_AddBitMap(deviceNumHandle);
        if (s32Ret != HI_SUCCESS)
        {
            LOGE("LOTO_OSD_AddBitMap for DeviceNum failed!\n");
            break;
        }

        while (1)
        {
            time(&timep);
            pLocalTime = localtime(&timep);
            if (seconds == pLocalTime->tm_sec)
            {
                usleep(100 * 1000);
                continue;
            }

            seconds = pLocalTime->tm_sec;
            // LOGD("Second = %d\n", seconds);

            // LOGD("LOTO_OSD_AddBitMap start: %s\n", GetLocalTime());
            s32Ret = LOTO_OSD_AddBitMap(timestampHandle);
            if (s32Ret != HI_SUCCESS)
            {
                LOGE("LOTO_OSD_AddBitMap for Timestamp failed!\n");
                break;
            }
            // LOGD("LOTO_OSD_AddBitMap end: %s\n", GetLocalTime());
        }
    } while (0);

    pthread_detach(pthread_self());

    return 0;
}

HI_S32 LOTO_OSD_CreateVideoOsdThread(HI_VOID)
{
    HI_S32 s32Ret;

    OsdHanlde *handle_info = (OsdHanlde *)malloc(sizeof(OsdHanlde));
    handle_info->deviceNum = OSD_HANDLE_DEVICENUM;
    handle_info->timestamp = OSD_HANDLE_TIMESTAMP;

    OsdRegionInfo deviceNumRgnInfo = {
        .width = 2 * 8 * 3,
        .height = 2 * 11,
        .pointX = 16 * 6,
        .pointY = 16 * 15,
    };

    OsdRegionInfo timestampRgnInfo = {
        .width = 2 * 8 * 19,
        .height = 2 * 11,
        .pointX = 16 * 10,
        .pointY = 16 * 15,
    };

    /* Create DeviceNum Region */
    s32Ret = LOTO_OSD_REGION_Create(handle_info->deviceNum, deviceNumRgnInfo.width, deviceNumRgnInfo.height);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_REGION_Create failed\n");
        return HI_FAILURE;
    }

    s32Ret = LOTO_OSD_REGION_AttachToChn(handle_info->deviceNum, deviceNumRgnInfo.pointX, deviceNumRgnInfo.pointY);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_REGION_AttachToChn failed\n");
        return HI_FAILURE;
    }

    /*  Create Timestamp Region */
    s32Ret = LOTO_OSD_REGION_Create(handle_info->timestamp, timestampRgnInfo.width, timestampRgnInfo.height);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_REGION_Create failed\n");
        return HI_FAILURE;
    }

    s32Ret = LOTO_OSD_REGION_AttachToChn(handle_info->timestamp, timestampRgnInfo.pointX, timestampRgnInfo.pointY);
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_REGION_AttachToChn failed\n");
        return HI_FAILURE;
    }

    s32Ret = LOTO_OSD_GetBmpBuffer();
    if (s32Ret != HI_SUCCESS)
    {
        LOGE("LOTO_OSD_GetBmpBuffer error!\n");
    }

    pthread_t osd_thread_id = 0;
    pthread_create(&osd_thread_id, NULL, LOTO_OSD_AddVideoOsd, handle_info);

    return HI_SUCCESS;
}
