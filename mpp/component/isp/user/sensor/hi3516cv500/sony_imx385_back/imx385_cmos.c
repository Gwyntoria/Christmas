#if !defined(__IMX385_CMOS_H_)
#define __IMX385_CMOS_H_

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hi_comm_sns.h"
#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"
#include "mpi_isp.h"
#include "mpi_ae.h"
#include "mpi_awb.h"
#include "imx385_cmos_ex.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */


// #define MCLK_LINE         (180000) //180M Mclk
// #define ROWTIME_LINE      (0xbb8) //from reg0xe~0xf
// #define FPS_LINE          (30)


// #define TFRM_Std(x,y,z)   (1000000/(x*y*1000/z))
// #define TFRM_Std_Line     TFRM_Std(FPS_LINE,ROWTIME_LINE,MCLK_LINE)
// #define VMAX_IMX385_1080P30_LINE 			(TFRM_Std_Line)//fps=30


#define IMX385_ID 385

/****************************************************************************
 * global variables                                                            *
 ****************************************************************************/
ISP_SNS_STATE_S*  g_astimx385[ISP_MAX_PIPE_NUM] = {HI_NULL};

#define IMX385_SENSOR_GET_CTX(dev, pstCtx)   (pstCtx = g_astimx385[dev])
#define IMX385_SENSOR_SET_CTX(dev, pstCtx)   (g_astimx385[dev] = pstCtx)
#define IMX385_SENSOR_RESET_CTX(dev)         (g_astimx385[dev] = HI_NULL)


static HI_U32 g_au32InitExposure[ISP_MAX_PIPE_NUM]  = {0};
static HI_U32 g_au32LinesPer500ms[ISP_MAX_PIPE_NUM] = {0};
static HI_U16 g_au16InitWBGain[ISP_MAX_PIPE_NUM][3] = {{0}};
static HI_U16 g_au16SampleRgain[ISP_MAX_PIPE_NUM] = {0};
static HI_U16 g_au16SampleBgain[ISP_MAX_PIPE_NUM] = {0};

ISP_SNS_COMMBUS_U     g_aunImx385BusInfo[ISP_MAX_PIPE_NUM] = {
    [0] = { .s8I2cDev = 1},
    [1 ... ISP_MAX_PIPE_NUM - 1] = { .s8I2cDev = -1}
};

static HI_U32  gu32AGain[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = 1024};
static HI_U32  gu32DGain[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = 1024};

extern const unsigned int imx385_i2c_addr;
extern unsigned int imx385_addr_byte;
extern unsigned int imx385_data_byte;

typedef struct hiIMX385_STATE_S
{
    HI_U8       u8Hcg;
    HI_U32      u32BRL;
    HI_U32      u32RHS1_MAX;
    HI_U32      u32RHS2_MAX;
} IMX385_STATE_S;

IMX385_STATE_S g_astimx385State[ISP_MAX_PIPE_NUM] = {{0}};

extern void imx385_init(VI_PIPE ViPipe);
extern void imx385_exit(VI_PIPE ViPipe);
extern void imx385_standby(VI_PIPE ViPipe);
extern void imx385_restart(VI_PIPE ViPipe);
extern int imx385_write_register(VI_PIPE ViPipe, int addr, int data);
extern int imx385_read_register(VI_PIPE ViPipe, int addr);

#define IMX385_FULL_LINES_MAX  (0xFFFF)
#define IMX385_FULL_LINES_MAX_2TO1_WDR  (0x8AA)    // considering the YOUT_SIZE and bad frame
#define IMX385_FULL_LINES_MAX_3TO1_WDR  (0x7FC)
/*****Imx385 Register Address*****/
#define IMX385_SHS1_ADDR (0x3020) 
#define IMX385_SHS2_ADDR (0x3024) 
#define IMX385_SHS3_ADDR (0x3028) 
#define IMX385_GAIN_ADDR (0x3014)
#define IMX385_HCG_ADDR  (0x3009)
#define IMX385_VMAX_ADDR (0x3018)
#define IMX385_HMAX_ADDR (0x301c)
#define IMX385_RHS1_ADDR (0x3030) 
#define IMX385_RHS2_ADDR (0x3034)
#define IMX385_Y_OUT_SIZE_ADDR (0x3418)

#define IMX385_INCREASE_LINES (1) /* make real fps less than stand fps because NVR require*/

#define IMX385_VMAX_1080P30_LINEAR  (1125+IMX385_INCREASE_LINES)
#define IMX385_VMAX_1080P60TO30_WDR (1220+IMX385_INCREASE_LINES)
#define IMX385_VMAX_1080P120TO30_WDR (1125+IMX385_INCREASE_LINES)
#define IMX385_VMAX_1080P60_LINEAR (1125+IMX385_INCREASE_LINES)

//sensor fps mode
#define IMX385_SENSOR_1080P_30FPS_LINEAR_MODE      (1)
#define IMX385_SENSOR_1080P_30FPS_3t1_WDR_MODE     (2)
#define IMX385_SENSOR_1080P_30FPS_2t1_WDR_MODE     (3)
#define IMX385_SENSOR_1080P_60FPS_LINEAR_MODE      (4)


static HI_U32 gain_table[299]=
{
	1024,1036,1048,1060,1072,1085,1097,1110,1123,1136,1149,1162,
	1176,1189,1203,1217,1231,1245,1260,1274,1289,1304,
	1319,1334,1350,1366,1381,1397,1413,1430,1446,1463,
	1480,1497,1515,1532,1550,1568,1586,1604,1623,1642,
	1661,1680,1699,1719,1739,1759,1779,1800,1821,1842,
	1863,1885,1907,1929,1951,1974,1997,2020,2043,2067,
	2091,2115,2139,2164,2189,2215,2240,2266,2292,2319,
	2346,2373,2400,2428,2456,2485,2514,2543,2572,2602,
	2632,2662,2693,2724,2756,2788,2820,2853,2886,2919,
	2953,2987,3022,3057,3092,3128,3164,3201,3238,3276,
	3313,3352,3391,3430,3470,3510,3550,3592,3633,3675,
	3718,3761,3804,3848,3893,3938,3984,4030,4076,4124,
	4171,4220,4269,4318,4368,4419,4470,4521,4574,4627,
	4680,4734,4789,4845,4901,4958,5015,5073,5132,5191,
	5251,5312,5374,5436,5499,5563,5627,5692,5758,5825,
	5892,5960,6029,6099,6170,6241,6313,6387,6461,6535,
	6611,6688,6765,6843,6923,7003,7084,7166,7249,7333,
	7418,7504,7590,7678,7767,7857,7948,8040,8133,8227,
	8323,8419,8517,8615,8715,8816,8918,9021,9126,9231,
	9338,9446,9556,9666,9778,9891,10006,10122,10239,10358,
	10478,10599,10722,10846,10971,11098,11227,11357,11488,11621,
	11756,11892,12030,12169,12310,12453,12597,12743,12890,13039,
	13190,13343,13498,13654,13812,13972,14134,14297,14463,14630,
	14800,14971,15144,15320,15497,15677,15858,16042,16228,16415,
	16606,16798,16992,17189,17388,17589,17793,17999,18208,18418,
	18632,18847,19066,19286,19510,19736,19964,20195,20429,20666,
	20905,21147,21392,21640,21890,22144,22400,22659,22922,23187,
	23456,23727,24002,24280,24561,24845,25133,25424,25718,26016,
	26318,26622,26930,27242,27558,27877,28200,28526,28856,29191,
	29529,29871,30216,30566,30920,31278,31744,
};


static HI_U32 dgain_table[421]=
{
	1024,1036,1048,1060,1072,1085,1097,1110,1123,1136,1149,1162,
	1176,1189,1203,1217,1231,1245,1260,1274,1289,1304,
	1319,1334,1350,1366,1381,1397,1413,1430,1446,1463,
	1480,1497,1515,1532,1550,1568,1586,1604,1623,1642,
	1661,1680,1699,1719,1739,1759,1779,1800,1821,1842,
	1863,1885,1907,1929,1951,1974,1997,2020,2043,2067,
	2091,2115,2139,2164,2189,2215,2240,2266,2292,2319,
	2346,2373,2400,2428,2456,2485,2514,2543,2572,2602,
	2632,2662,2693,2724,2756,2788,2820,2853,2886,2919,
	2953,2987,3022,3057,3092,3128,3164,3201,3238,3276,
	3313,3352,3391,3430,3470,3510,3550,3592,3633,3675,
	3718,3761,3804,3848,3893,3938,3984,4030,4076,4124,
	4171,4220,4269,4318,4368,4419,4470,4521,4574,4627,
	4680,4734,4789,4845,4901,4958,5015,5073,5132,5191,
	5251,5312,5374,5436,5499,5563,5627,5692,5758,5825,
	5892,5960,6029,6099,6170,6241,6313,6387,6461,6535,
	6611,6688,6765,6843,6923,7003,7084,7166,7249,7333,
	7418,7504,7590,7678,7767,7857,7948,8040,8133,8227,
	8323,8419,8517,8615,8715,8816,8918,9021,9126,9231,
	9338,9446,9556,9666,9778,9891,10006,10122,10239,10358,
	10478,10599,10722,10846,10971,11098,11227,11357,11488,11621,
	11756,11892,12030,12169,12310,12453,12597,12743,12890,13039,
	13190,13343,13498,13654,13812,13972,14134,14297,14463,14630,
	14800,14971,15144,15320,15497,15677,15858,16042,16228,16415,
	16606,16798,16992,17189,17388,17589,17793,17999,18208,18418,
	18632,18847,19066,19286,19510,19736,19964,20195,20429,20666,
	20905,21147,21392,21640,21890,22144,22400,22659,22922,23187,
	23456,23727,24002,24280,24561,24845,25133,25424,25718,26016,
	26318,26622,26930,27242,27558,27877,28200,28526,28856,29191,
	29529,29871,30216,30566,30920,31278,31640,32007,32377,32752,
	33131,33515,33903,34296,34693,35095,35501,35912,36328,36748,
	37174,37604,38040,38480,38926,39377,39833,40294,40760,41232,
	41710,42193,42681,43175,43675,44181,44693,45210,45734,46263,
	46799,47341,47889,48443,49004,49572,50146,50726,51314,51908,
	52509,53117,53732,54354,54983,55620,56264,56916,57575,58241,
	58916,59598,60288,60986,61692,62407,63129,63860,64600,65348,
	66104,66870,67644,68427,69219,70021,70832,71652,72482,73321,
	74170,75029,75897,76776,77665,78564,79474,80394,81325,82267,
	83220,84183,85158,86144,87141,88150,89171,90204,91248,92305,
	93373,94455,95548,96655,97774,98906,100051,101210,102382,103567,
	104766,105979,107206,108448,109704,110974,112259,113559,114874,116204,
	117549,118910,120287,121680,123089,124514,125956,127414,129024,
};


// 6    14
static HI_S32 cmos_get_ae_default(VI_PIPE ViPipe,AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    HI_U32 u32Fll;
    HI_U32 U32MaxFps = 30;

    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAeSnsDft);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    // u32Fll = VMAX_IMX385_1080P30_LINE;
    u32Fll = IMX385_VMAX_1080P30_LINEAR;

    pstSnsState->u32FLStd = u32Fll;
    pstAeSnsDft->stIntTimeAccu.f32Offset = 0;
    pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;


    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FlickerFreq = 0;
    // pstAeSnsDft->u32FlickerFreq = 50 * 256;
    pstAeSnsDft->u32FullLinesMax = IMX385_FULL_LINES_MAX;
    pstAeSnsDft->u32HmaxTimes = (1000000) / (pstSnsState->u32FLStd * 30);


    pstAeSnsDft->stIntTimeAccu.enAccuType = AE_ACCURACY_LINEAR;
    pstAeSnsDft->stIntTimeAccu.f32Accuracy = 1;
    pstAeSnsDft->stIntTimeAccu.f32Offset = 0;//0.1083;  //3.85us

    pstAeSnsDft->stAgainAccu.enAccuType = AE_ACCURACY_TABLE;
    pstAeSnsDft->stAgainAccu.f32Accuracy = 0.1;
    // pstAeSnsDft->stAgainAccu.f32Accuracy = 1;

    pstAeSnsDft->stDgainAccu.enAccuType = AE_ACCURACY_TABLE;
    pstAeSnsDft->stDgainAccu.f32Accuracy = 0.1;
    // pstAeSnsDft->stDgainAccu.f32Accuracy = 1;
    
    pstAeSnsDft->u32ISPDgainShift = 8;
    pstAeSnsDft->u32MinISPDgainTarget = 1 << pstAeSnsDft->u32ISPDgainShift;
    pstAeSnsDft->u32MaxISPDgainTarget = 8 << pstAeSnsDft->u32ISPDgainShift; //lzw

// memcpy(&pstAeSnsDft->stPirisAttr, &gstPirisAttr, sizeof(ISP_PIRIS_ATTR_S));

    pstAeSnsDft->u32InitExposure = 921600;
    pstAeSnsDft->enMaxIrisFNO = ISP_IRIS_F_NO_1_0;
    pstAeSnsDft->enMinIrisFNO = ISP_IRIS_F_NO_32_0;

    pstAeSnsDft->bAERouteExValid = HI_FALSE;
	
    pstAeSnsDft->stAERouteAttr.u32TotalNum = 0;
    pstAeSnsDft->stAERouteAttrEx.u32TotalNum = 0;

    if (g_au32LinesPer500ms[ViPipe] == 0)
    {
        pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd*30/2;
        switch(pstSnsState->enWDRMode)
        {
            default:
            case WDR_MODE_NONE:   /*linear mode*/

                pstAeSnsDft->au8HistThresh[0] = 0xd;
                pstAeSnsDft->au8HistThresh[1] = 0x28;
                pstAeSnsDft->au8HistThresh[2] = 0x60;
                pstAeSnsDft->au8HistThresh[3] = 0x80;

                pstAeSnsDft->u32MaxAgain = 31*1024; 

                pstAeSnsDft->u32MinAgain = 1024;
                pstAeSnsDft->u32MaxAgainTarget = pstAeSnsDft->u32MaxAgain;
                pstAeSnsDft->u32MinAgainTarget = pstAeSnsDft->u32MinAgain;

                pstAeSnsDft->u32MaxDgain = 125*1024;

                pstAeSnsDft->u32MinDgain = 1024;
                
                pstAeSnsDft->u32MaxDgainTarget = 125*1024;

                pstAeSnsDft->u32MinDgainTarget = pstAeSnsDft->u32MinDgain;
                
                pstAeSnsDft->u8AeCompensation = 0x38;
                pstAeSnsDft->enAeExpMode = AE_EXP_HIGHLIGHT_PRIOR; 
                
                pstAeSnsDft->u32InitExposure = g_au32InitExposure[ViPipe] ? g_au32InitExposure[ViPipe] : 148859;
                
                pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
                pstAeSnsDft->u32MinIntTime = 1;
                pstAeSnsDft->u32MaxIntTimeTarget = 65535;
                pstAeSnsDft->u32MinIntTimeTarget = 1;
            break;
        }
    }
    else
    {
        pstAeSnsDft->u32LinesPer500ms = g_au32LinesPer500ms[ViPipe];
    }


    return 0;
}

//  15
/* the function of sensor set fps */
static HI_VOID cmos_fps_set(VI_PIPE ViPipe, HI_FLOAT f32Fps, AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{

    HI_U32 u32MaxFps;
    HI_U32 u32Lines;
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);
                                                                       
                                                                                                                          
    switch (pstSnsState->u8ImgMode)                                                                                           
    {
        case IMX385_SENSOR_1080P_30FPS_2t1_WDR_MODE:
            u32MaxFps = 30;
            // u32Lines = IMX385_VMAX_1080P60TO30_WDR * u32MaxFps / f32Fps;
            u32Lines = IMX385_VMAX_1080P60TO30_WDR * u32MaxFps / DIV_0_TO_1_FLOAT(f32Fps);;

            pstSnsState->u32FLStd = u32Lines;
            pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
            break; 
                
        case IMX385_SENSOR_1080P_30FPS_LINEAR_MODE:
            u32MaxFps = 30;
            // u32Lines = IMX385_VMAX_1080P30_LINEAR * u32MaxFps / f32Fps;
            u32Lines = IMX385_VMAX_1080P30_LINEAR * u32MaxFps / DIV_0_TO_1_FLOAT(f32Fps);;
            pstSnsState->u32FLStd = u32Lines;
            pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
            break; 

        case IMX385_SENSOR_1080P_60FPS_LINEAR_MODE:
            u32MaxFps = 60;
            // u32Lines = IMX385_VMAX_1080P60_LINEAR * u32MaxFps / f32Fps;
            u32Lines = IMX385_VMAX_1080P60_LINEAR * u32MaxFps / DIV_0_TO_1_FLOAT(f32Fps);
            
            pstSnsState->u32FLStd = u32Lines;
            pstAeSnsDft->u32MaxIntTime = pstSnsState->u32FLStd - 2;
            break;                                                                                                  
        default:                                                                                                              
            return;
            break;                                                                                                            
    }

    pstAeSnsDft->f32Fps = f32Fps;  
    // pstAeSnsDft->u32LinesPer500ms = pstSnsState->u32FLStd * f32Fps / 2;
    pstAeSnsDft->u32FullLinesStd = pstSnsState->u32FLStd;   
    pstSnsState->au32FL[0] = pstSnsState->u32FLStd;
    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];
    pstAeSnsDft->u32HmaxTimes = (1000000) / (pstSnsState->u32FLStd * DIV_0_TO_1_FLOAT(f32Fps));

    pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = pstSnsState->au32FL[0] & 0xFF;
    pstSnsState->astRegsInfo[0].astI2cData[5].u32Data = ((pstSnsState->au32FL[0] & 0xFF00) >> 8);

    return;
}

static HI_VOID cmos_slow_framerate_set(VI_PIPE ViPipe,HI_U32 u32FullLines,
    AE_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER_VOID(pstAeSnsDft);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    u32FullLines = (u32FullLines > IMX385_FULL_LINES_MAX) ? IMX385_FULL_LINES_MAX : u32FullLines;
    
    pstSnsState->au32FL[0] = u32FullLines;  
    pstSnsState->astRegsInfo[0].astI2cData[4].u32Data = (pstSnsState->au32FL[0] & 0xFF);
    pstSnsState->astRegsInfo[0].astI2cData[5].u32Data = ((pstSnsState->au32FL[0] & 0xFF00) >> 8);
    

    pstAeSnsDft->u32FullLines = pstSnsState->au32FL[0];
    pstAeSnsDft->u32MaxIntTime = pstSnsState->au32FL[0] - 2; 

    return;
}


//   17
/* while isp notify ae to update sensor regs, ae call these funcs. */
static HI_VOID cmos_inttime_update(VI_PIPE ViPipe,HI_U32 u32IntTime)
{
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    HI_U32 u32Value = 0;
                                                                             
    u32Value = pstSnsState->au32FL[0] - u32IntTime - 1; 
                                                                                                
    pstSnsState->astRegsInfo[0].astI2cData[0].u32Data = (u32Value & 0xFF);                                    
    pstSnsState->astRegsInfo[0].astI2cData[1].u32Data = ((u32Value & 0xFF00) >> 8);                           
                                                                                    
    return;
}

//   16
static HI_VOID cmos_again_calc_table(VI_PIPE ViPipe,HI_U32 *pu32AgainLin, HI_U32 *pu32AgainDb)
{
    CMOS_CHECK_POINTER_VOID(pu32AgainLin);
    CMOS_CHECK_POINTER_VOID(pu32AgainDb);

    int i;

    if (*pu32AgainLin >= gain_table[298])
    {
         *pu32AgainLin = gain_table[298];
         *pu32AgainDb = 298;
         return ;
    }
    
    for (i = 1; i < 299; i++)
    {
        if (*pu32AgainLin < gain_table[i])
        {
            *pu32AgainLin = gain_table[i - 1];
            *pu32AgainDb = i - 1;
            break;
        }
    }

    gu32AGain[ViPipe] = *pu32AgainLin;
    return;
}

static HI_VOID cmos_dgain_calc_table(VI_PIPE ViPipe,HI_U32 *pu32DgainLin, HI_U32 *pu32DgainDb)
{

    CMOS_CHECK_POINTER_VOID(pu32DgainLin);
    CMOS_CHECK_POINTER_VOID(pu32DgainDb);
    int i;

    if (*pu32DgainLin >= dgain_table[420])
    {
         *pu32DgainLin = dgain_table[420];
         *pu32DgainDb = 420;
         return ;
    }
    
    for (i = 1; i < 421; i++)
    {
        if (*pu32DgainLin < gain_table[i])
        {
            *pu32DgainLin = gain_table[i - 1];
            *pu32DgainDb = i - 1;
            break;
        }
    }

    gu32DGain[ViPipe] = *pu32DgainLin;
    return;
}


//  16
static HI_VOID cmos_gains_update(VI_PIPE ViPipe,HI_U32 u32Again, HI_U32 u32Dgain)
{  
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    HI_U32 u32Tmp;


    u32Tmp=u32Again+u32Dgain;

	if(u32Tmp >= 720)
		u32Tmp = 720;
        
    pstSnsState->astRegsInfo[0].astI2cData[2].u32Data = (u32Tmp & 0xFF);
    pstSnsState->astRegsInfo[0].astI2cData[3].u32Data = (u32Tmp >> 8 ) & 0x3;
    
    return;
}


// 5
static HI_S32 cmos_init_ae_exp_function(AE_SENSOR_EXP_FUNC_S *pstExpFuncs)
{
    CMOS_CHECK_POINTER(pstExpFuncs);

    memset(pstExpFuncs, 0, sizeof(AE_SENSOR_EXP_FUNC_S));

    pstExpFuncs->pfn_cmos_get_ae_default    = cmos_get_ae_default;
    pstExpFuncs->pfn_cmos_fps_set           = cmos_fps_set;
    pstExpFuncs->pfn_cmos_slow_framerate_set= cmos_slow_framerate_set;    
    pstExpFuncs->pfn_cmos_inttime_update    = cmos_inttime_update;
    pstExpFuncs->pfn_cmos_gains_update      = cmos_gains_update;
    pstExpFuncs->pfn_cmos_again_calc_table  = cmos_again_calc_table;
    pstExpFuncs->pfn_cmos_dgain_calc_table  = cmos_dgain_calc_table;


    return 0;
}

#define GOLDEN_RGAIN 0   
#define GOLDEN_BGAIN 0 


// 8      18
static HI_S32 cmos_get_awb_default(VI_PIPE ViPipe,AWB_SENSOR_DEFAULT_S *pstAwbSnsDft)
{
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstAwbSnsDft);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(pstAwbSnsDft, 0, sizeof(AWB_SENSOR_DEFAULT_S));

    pstAwbSnsDft->u16WbRefTemp = 4900;

    pstAwbSnsDft->au16GainOffset[0] = 0x1C3;
    pstAwbSnsDft->au16GainOffset[1] = 0x100;
    pstAwbSnsDft->au16GainOffset[2] = 0x100;
    pstAwbSnsDft->au16GainOffset[3] = 0x1D4;

    pstAwbSnsDft->as32WbPara[0] = -37;
    pstAwbSnsDft->as32WbPara[1] = 293;
    pstAwbSnsDft->as32WbPara[2] = 0;
    pstAwbSnsDft->as32WbPara[3] = 179537;
    pstAwbSnsDft->as32WbPara[4] = 128;
    pstAwbSnsDft->as32WbPara[5] = -123691;
   
    pstAwbSnsDft->u16GoldenRgain = GOLDEN_RGAIN;
    pstAwbSnsDft->u16GoldenBgain = GOLDEN_BGAIN;
	
    switch (pstSnsState->enWDRMode)
    {
        default:
        case WDR_MODE_NONE:
            memcpy(&pstAwbSnsDft->stCcm, &g_stAwbCcm, sizeof(AWB_CCM_S));
            memcpy(&pstAwbSnsDft->stAgcTbl, &g_stAwbAgcTable, sizeof(AWB_AGC_TABLE_S));
            break;
    }
	
    pstAwbSnsDft->u16InitRgain = g_au16InitWBGain[ViPipe][0];
    pstAwbSnsDft->u16InitGgain = g_au16InitWBGain[ViPipe][1];
    pstAwbSnsDft->u16InitBgain = g_au16InitWBGain[ViPipe][2];
    pstAwbSnsDft->u16SampleRgain = g_au16SampleRgain[ViPipe];
    pstAwbSnsDft->u16SampleBgain = g_au16SampleBgain[ViPipe];

    return 0;
}

// 7
static HI_S32 cmos_init_awb_exp_function(AWB_SENSOR_EXP_FUNC_S *pstExpFuncs)
{
    CMOS_CHECK_POINTER(pstExpFuncs);

    memset(pstExpFuncs, 0, sizeof(AWB_SENSOR_EXP_FUNC_S));

    pstExpFuncs->pfn_cmos_get_awb_default = cmos_get_awb_default;

    return 0;
}

static ISP_CMOS_DNG_COLORPARAM_S g_stDngColorParam =
{
    {378, 256, 430},
    {439, 256, 439}
};

//  12
static HI_U32 cmos_get_isp_default(VI_PIPE ViPipe,ISP_CMOS_DEFAULT_S *pstDef)
{
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstDef);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    memset(pstDef, 0, sizeof(ISP_CMOS_DEFAULT_S));
    
    pstDef->unKey.bit1Ca       = 1;
    pstDef->pstCa              = &g_stIspCA;
    pstDef->unKey.bit1Clut     = 1;
    pstDef->pstClut            = &g_stIspCLUT;
    pstDef->unKey.bit1Dpc      = 1;
    pstDef->pstDpc             = &g_stCmosDpc;
    pstDef->unKey.bit1Wdr      = 1;
    pstDef->pstWdr             = &g_stIspWDR;
    pstDef->unKey.bit1Hlc      = 0;
    pstDef->pstHlc             = &g_stIspHlc;
    pstDef->unKey.bit1EdgeMark = 0;
    pstDef->pstEdgeMark        = &g_stIspEdgeMark;
    pstDef->unKey.bit1Lsc      = 0;
    pstDef->pstLsc             = &g_stCmosLsc;

    switch (pstSnsState->enWDRMode)
    {
        // default:
        case WDR_MODE_NONE:
            pstDef->unKey.bit1Demosaic       = 1;
            pstDef->pstDemosaic              = &g_stIspDemosaic;
            pstDef->unKey.bit1Sharpen        = 1;
            pstDef->pstSharpen               = &g_stIspYuvSharpen;
            pstDef->unKey.bit1Drc            = 1;
            pstDef->pstDrc                   = &g_stIspDRC;
            pstDef->unKey.bit1Gamma          = 1;
            pstDef->pstGamma                 = &g_stIspGamma;
            pstDef->unKey.bit1BayerNr        = 1;
            pstDef->pstBayerNr               = &g_stIspBayerNr;
            pstDef->unKey.bit1Ge             = 1;
            pstDef->pstGe                    = &g_stIspGe;
            pstDef->unKey.bit1AntiFalseColor = 1;
            pstDef->pstAntiFalseColor        = &g_stIspAntiFalseColor;
            pstDef->unKey.bit1Ldci           = 1;
            pstDef->pstLdci                  = &g_stIspLdci;
            memcpy(&pstDef->stNoiseCalibration, &g_stIspNoiseCalibration, sizeof(ISP_CMOS_NOISE_CALIBRATION_S));
            break;
        default:
            break;
    }

    pstDef->stSensorMaxResolution.u32MaxWidth  = 1920;
    pstDef->stSensorMaxResolution.u32MaxHeight = 1080;
	
    pstDef->stSensorMode.u32SensorID = IMX385_ID;
    pstDef->stSensorMode.u8SensorMode = pstSnsState->u8ImgMode;

    memcpy(&pstDef->stDngColorParam, &g_stDngColorParam, sizeof(ISP_CMOS_DNG_COLORPARAM_S));

    switch (pstSnsState->u8ImgMode)
    {
        default:
            pstDef->stSensorMode.stDngRawFormat.u8BitsPerSample = 12;
            pstDef->stSensorMode.stDngRawFormat.u32WhiteLevel = 0xD0000;
            break;
    }

    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleH.u32Denominator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleH.u32Numerator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleV.u32Denominator = 1;
    pstDef->stSensorMode.stDngRawFormat.stDefaultScale.stDefaultScaleV.u32Numerator = 1;
    pstDef->stSensorMode.stDngRawFormat.stCfaRepeatPatternDim.u16RepeatPatternDimRows = 2;
    pstDef->stSensorMode.stDngRawFormat.stCfaRepeatPatternDim.u16RepeatPatternDimCols = 2;
    pstDef->stSensorMode.stDngRawFormat.stBlcRepeatDim.u16BlcRepeatRows = 2;
    pstDef->stSensorMode.stDngRawFormat.stBlcRepeatDim.u16BlcRepeatCols = 2;
    pstDef->stSensorMode.stDngRawFormat.enCfaLayout = CFALAYOUT_TYPE_RECTANGULAR;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[0] = 0;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[1] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPlaneColor[2] = 2;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[0] = 0;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[1] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[2] = 1;
    pstDef->stSensorMode.stDngRawFormat.au8CfaPattern[3] = 2;
    pstDef->stSensorMode.bValidDngRawFormat = HI_TRUE;

    return 0;
}

//   13
static HI_U32 cmos_get_isp_black_level(VI_PIPE ViPipe,ISP_CMOS_BLACK_LEVEL_S *pstBlackLevel)
{
    CMOS_CHECK_POINTER(pstBlackLevel);

    pstBlackLevel->bUpdate = HI_FALSE;
    
    pstBlackLevel->au16BlackLevel[0] = 0xF0;
    pstBlackLevel->au16BlackLevel[1] = 0xF0;
    pstBlackLevel->au16BlackLevel[2] = 0xF0;
    pstBlackLevel->au16BlackLevel[3] = 0xF0;
    
    return 0;  
}

static HI_VOID cmos_set_pixel_detect(VI_PIPE ViPipe,HI_BOOL bEnable)
{
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    HI_U32 u32FullLines_5Fps, u32MaxIntTime_5Fps;
    
    if (WDR_MODE_2To1_LINE == pstSnsState->enWDRMode)
    {
        return;
    }

    else if(WDR_MODE_3To1_LINE == pstSnsState->enWDRMode)
    {
        return;
    }

    else
    {
        if (IMX385_SENSOR_1080P_30FPS_LINEAR_MODE == pstSnsState->u8ImgMode)
        {
            u32FullLines_5Fps = (IMX385_VMAX_1080P30_LINEAR * 30) / 5;
        }
        
        else
        {
            return;
        }
    }

 //   u32FullLines_5Fps = (u32FullLines_5Fps > IMX385_FULL_LINES_MAX) ? IMX385_FULL_LINES_MAX : u32FullLines_5Fps;
    u32MaxIntTime_5Fps = 4;

    if (bEnable) /* setup for ISP pixel calibration mode */
    {
        imx385_write_register (ViPipe,IMX385_GAIN_ADDR,0x00);
        
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR, u32FullLines_5Fps & 0xFF); 
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR + 1, (u32FullLines_5Fps & 0xFF00) >> 8); 
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR + 2, (u32FullLines_5Fps & 0xF0000) >> 16);

        imx385_write_register (ViPipe,IMX385_SHS1_ADDR, u32MaxIntTime_5Fps & 0xFF);
        imx385_write_register (ViPipe,IMX385_SHS1_ADDR + 1,  (u32MaxIntTime_5Fps & 0xFF00) >> 8); 
        imx385_write_register (ViPipe,IMX385_SHS1_ADDR + 2, (u32MaxIntTime_5Fps & 0xF0000) >> 16); 
          
    }
    else /* setup for ISP 'normal mode' */
    {
        pstSnsState->u32FLStd = (pstSnsState->u32FLStd > 0x1FFFF) ? 0x1FFFF : pstSnsState->u32FLStd;
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR, pstSnsState->u32FLStd & 0xFF); 
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR + 1, (pstSnsState->u32FLStd & 0xFF00) >> 8); 
        imx385_write_register (ViPipe,IMX385_VMAX_ADDR + 2, (pstSnsState->u32FLStd & 0xF0000) >> 16);
        pstSnsState->bSyncInit = HI_FALSE;
    }

    return;
}

// 10
static HI_VOID cmos_set_wdr_mode(VI_PIPE ViPipe,HI_U8 u8Mode)                                          
{       
                          
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

#if 1
    pstSnsState->bSyncInit = HI_FALSE;       
    pstSnsState->enWDRMode = WDR_MODE_NONE;
    pstSnsState->u32FLStd = IMX385_VMAX_1080P30_LINEAR;
    // pstSnsState->u8ImgMode = IMX385_SENSOR_1080P_30FPS_LINEAR_MODE;   

    pstSnsState->au32FL[0]= pstSnsState->u32FLStd;
    pstSnsState->au32FL[1] = pstSnsState->au32FL[0];
    memset(pstSnsState->au32WDRIntTime, 0, sizeof(pstSnsState->au32WDRIntTime));
    
    return;  
#else
    pstSnsState->bSyncInit = HI_FALSE;                                                           
													   
    switch(u8Mode)                                                               
    {                                                                            
        case WDR_MODE_NONE:                                                      
            pstSnsState->enWDRMode = WDR_MODE_NONE;                                       
            pstSnsState->u32FLStd = IMX385_VMAX_1080P30_LINEAR;
            printf("linear mode\n");                                             
        break;                                                                   
                                                                                 
        case WDR_MODE_2To1_LINE:                                                 
            pstSnsState->enWDRMode = WDR_MODE_2To1_LINE;                                  
            pstSnsState->u32FLStd = IMX385_VMAX_1080P60TO30_WDR * 2;
            printf("2to1 line WDR 1080p mode(60fps->30fps)\n");
       break;                                                                   


        case WDR_MODE_3To1_LINE:                                                 
            pstSnsState->enWDRMode = WDR_MODE_3To1_LINE;                                  
            pstSnsState->u32FLStd = IMX385_VMAX_1080P120TO30_WDR * 4; 
            printf("3to1 line WDR 1080p mode(120fps->30fps)\n");
        break; 
                                                                                 
        default:                                                                 
            printf("NOT support this mode!\n");                                  
            return;                                                              
        break;                                                                   
    }  

    pstSnsState->au32FL[0]= pstSnsState->u32FLStd;
    pstSnsState->au32FL[1] = pstSnsState->au32FL[0];
    memset(pstSnsState->au32WDRIntTime, 0, sizeof(pstSnsState->au32WDRIntTime));
    return;
#endif
}


// 19
static HI_U32 cmos_get_sns_regs_info(VI_PIPE ViPipe,ISP_SNS_REGS_INFO_S *pstSnsRegsInfo)
{
    HI_S32 i;

    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstSnsRegsInfo);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    if ((HI_FALSE == pstSnsState->bSyncInit) || (HI_FALSE == pstSnsRegsInfo->bConfig))
    {
    	printf("cmos_get_sns_regs_info \n");
        pstSnsState->astRegsInfo[0].enSnsType = ISP_SNS_I2C_TYPE;
        pstSnsState->astRegsInfo[0].unComBus.s8I2cDev = g_aunImx385BusInfo[ViPipe].s8I2cDev;        
        pstSnsState->astRegsInfo[0].u8Cfg2ValidDelayMax = 2;
        pstSnsState->astRegsInfo[0].u32RegNum = 6;

        for (i=0; i<pstSnsState->astRegsInfo[0].u32RegNum; i++)
        {
            pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_TRUE;
            pstSnsState->astRegsInfo[0].astI2cData[i].u8DevAddr = imx385_i2c_addr;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32AddrByteNum = imx385_addr_byte;
            pstSnsState->astRegsInfo[0].astI2cData[i].u32DataByteNum = imx385_data_byte;
        }

        //Linear Mode Regs
        pstSnsState->astRegsInfo[0].astI2cData[0].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[0].u32RegAddr = IMX385_SHS1_ADDR;       
        pstSnsState->astRegsInfo[0].astI2cData[1].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[1].u32RegAddr = IMX385_SHS1_ADDR + 1;        
      
        pstSnsState->astRegsInfo[0].astI2cData[2].u8DelayFrmNum = 0;       //make shutter and gain effective at the same time
        pstSnsState->astRegsInfo[0].astI2cData[2].u32RegAddr = IMX385_GAIN_ADDR;  //gain     

		pstSnsState->astRegsInfo[0].astI2cData[3].u8DelayFrmNum = 0;       //make shutter and gain effective at the same time
        pstSnsState->astRegsInfo[0].astI2cData[3].u32RegAddr = IMX385_GAIN_ADDR + 1;  //gain     
    
	
        pstSnsState->astRegsInfo[0].astI2cData[4].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[4].u32RegAddr = IMX385_VMAX_ADDR;
        pstSnsState->astRegsInfo[0].astI2cData[5].u8DelayFrmNum = 0;
        pstSnsState->astRegsInfo[0].astI2cData[5].u32RegAddr = IMX385_VMAX_ADDR + 1;
     
        pstSnsState->bSyncInit = HI_TRUE;

    }
    else
    {
        for (i=0; i<pstSnsState->astRegsInfo[0].u32RegNum; i++)
        {
            if (pstSnsState->astRegsInfo[0].astI2cData[i].u32Data == pstSnsState->astRegsInfo[1].astI2cData[i].u32Data)
            {
                pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_FALSE;
            }
            
            else
            {
                pstSnsState->astRegsInfo[0].astI2cData[i].bUpdate = HI_TRUE;
            }
        }
    }

    pstSnsRegsInfo->bConfig = HI_FALSE;
    memcpy(pstSnsRegsInfo, &pstSnsState->astRegsInfo[0], sizeof(ISP_SNS_REGS_INFO_S)); 
    memcpy(&pstSnsState->astRegsInfo[1], &pstSnsState->astRegsInfo[0], sizeof(ISP_SNS_REGS_INFO_S)); 

    pstSnsState->au32FL[1] = pstSnsState->au32FL[0];

    return 0;
}

//   11
static HI_S32 cmos_set_image_mode(VI_PIPE ViPipe,ISP_CMOS_SENSOR_IMAGE_MODE_S *pstSensorImageMode)
{
    HI_U8 u8SensorImageMode;
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    CMOS_CHECK_POINTER(pstSensorImageMode);
    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER(pstSnsState);

    pstSnsState->bSyncInit = HI_FALSE; 
        
    if (HI_NULL == pstSensorImageMode )
    {
        printf("null pointer when set image mode\n");
        return -1;
    }
	

    if ((pstSensorImageMode->u16Width <= 1920) && (pstSensorImageMode->u16Height <= 1080)) 
    {
         if (pstSensorImageMode->f32Fps <= 30)
        {
            pstSnsState->u8ImgMode = IMX385_SENSOR_1080P_30FPS_LINEAR_MODE;
        }
        else if(pstSensorImageMode->f32Fps <= 60)
        {
            pstSnsState->u8ImgMode = IMX385_SENSOR_1080P_60FPS_LINEAR_MODE;
        }
        else
        {
            printf("Not support! Width:%d, Height:%d, Fps:%f\n", 
                pstSensorImageMode->u16Width, 
                pstSensorImageMode->u16Height,
                pstSensorImageMode->f32Fps);

            return -1;
        }
    }  
    else
    {
        printf("Not support! Width:%d, Height:%d, Fps:%f, WDRMode:%d\n", 
        pstSensorImageMode->u16Width, 
        pstSensorImageMode->u16Height,
        pstSensorImageMode->f32Fps,
        pstSnsState->enWDRMode);

        return -1;
    }

    if ((HI_TRUE == pstSnsState->bInit) && (u8SensorImageMode == pstSnsState->u8ImgMode))     
    {                                                                              
        /* Don't need to switch SensorImageMode */                                 
        return -1;                                                                 
    }

    return 0;
}


// 4
static HI_VOID sensor_global_init(VI_PIPE ViPipe)
{     
    ISP_SNS_STATE_S* pstSnsState = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pstSnsState);
    CMOS_CHECK_POINTER_VOID(pstSnsState);

    pstSnsState->bInit = HI_FALSE; 
    pstSnsState->bSyncInit = HI_FALSE;
    pstSnsState->u8ImgMode = IMX385_SENSOR_1080P_30FPS_LINEAR_MODE;
    pstSnsState->enWDRMode = WDR_MODE_NONE;
    pstSnsState->u32FLStd = IMX385_VMAX_1080P30_LINEAR;
    pstSnsState->au32FL[0] = IMX385_VMAX_1080P30_LINEAR;
    pstSnsState->au32FL[1] = IMX385_VMAX_1080P30_LINEAR;
    
    memset(&pstSnsState->astRegsInfo[0], 0, sizeof(ISP_SNS_REGS_INFO_S));
    memset(&pstSnsState->astRegsInfo[1], 0, sizeof(ISP_SNS_REGS_INFO_S));
}


// 3
static HI_S32 cmos_init_sensor_exp_function(ISP_SENSOR_EXP_FUNC_S *pstSensorExpFunc)
{
    CMOS_CHECK_POINTER(pstSensorExpFunc);

    memset(pstSensorExpFunc, 0, sizeof(ISP_SENSOR_EXP_FUNC_S));

    pstSensorExpFunc->pfn_cmos_sensor_init = imx385_init;
    pstSensorExpFunc->pfn_cmos_sensor_exit = imx385_exit;
    pstSensorExpFunc->pfn_cmos_sensor_global_init = sensor_global_init;
    pstSensorExpFunc->pfn_cmos_set_image_mode = cmos_set_image_mode;
    pstSensorExpFunc->pfn_cmos_set_wdr_mode = cmos_set_wdr_mode;
    // pstSensorExpFunc->pfn_cmos_set_wdr_mode = NULL;
    pstSensorExpFunc->pfn_cmos_get_isp_default = cmos_get_isp_default;
    pstSensorExpFunc->pfn_cmos_get_isp_black_level = cmos_get_isp_black_level;
    pstSensorExpFunc->pfn_cmos_set_pixel_detect = cmos_set_pixel_detect;
    pstSensorExpFunc->pfn_cmos_get_sns_reg_info = cmos_get_sns_regs_info;

    return 0;
}

/****************************************************************************
 * callback structure                                                       *
 ****************************************************************************/
// 9
static int imx385_set_bus_info(VI_PIPE ViPipe, ISP_SNS_COMMBUS_U unSNSBusInfo)
{
    g_aunImx385BusInfo[ViPipe].s8I2cDev = unSNSBusInfo.s8I2cDev;        // s8I2cDev 1

    return 0;
}


// 2
static HI_S32 sensor_ctx_init(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S* pastSnsStateCtx = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);

    if (HI_NULL == pastSnsStateCtx)
    {
        pastSnsStateCtx = (ISP_SNS_STATE_S*)malloc(sizeof(ISP_SNS_STATE_S));

        if (HI_NULL == pastSnsStateCtx)
        {
            ISP_ERR_TRACE("Isp[%d] SnsCtx malloc memory failed!\n", ViPipe);
            return HI_ERR_ISP_NOMEM;
        }
    }

    memset(pastSnsStateCtx, 0, sizeof(ISP_SNS_STATE_S));

    IMX385_SENSOR_SET_CTX(ViPipe, pastSnsStateCtx);

    return HI_SUCCESS;
}


// 1 
static int imx385_sensor_register_callback(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib, ALG_LIB_S *pstAwbLib)
{
    HI_S32 s32Ret;
    ISP_SENSOR_REGISTER_S stIspRegister;
    AE_SENSOR_REGISTER_S  stAeRegister;
    AWB_SENSOR_REGISTER_S stAwbRegister;

    ISP_SNS_ATTR_INFO_S   stSnsAttrInfo;

    CMOS_CHECK_POINTER(pstAeLib);
    CMOS_CHECK_POINTER(pstAwbLib);

    s32Ret = sensor_ctx_init(ViPipe);

    if (HI_SUCCESS != s32Ret)
    {
        printf("sensor_ctx_init faild!\n");
        return HI_FAILURE;
    }

    stSnsAttrInfo.eSensorId = IMX385_ID;

    s32Ret = cmos_init_sensor_exp_function(&stIspRegister.stSnsExp);
    s32Ret |= HI_MPI_ISP_SensorRegCallBack(ViPipe, &stSnsAttrInfo, &stIspRegister);
    if (HI_SUCCESS != s32Ret)
    {
        printf("sensor register callback function failed!\n");
        return s32Ret;
    }

    s32Ret = cmos_init_ae_exp_function(&stAeRegister.stSnsExp);
    s32Ret |= HI_MPI_AE_SensorRegCallBack(ViPipe, pstAeLib, &stSnsAttrInfo, &stAeRegister);
    if (HI_SUCCESS != s32Ret)
    {
        printf("sensor register callback function to ae lib failed!\n");
        return s32Ret;
    }

    s32Ret = cmos_init_awb_exp_function(&stAwbRegister.stSnsExp);
    s32Ret |= HI_MPI_AWB_SensorRegCallBack(ViPipe, pstAwbLib, &stSnsAttrInfo, &stAwbRegister);
    if (HI_SUCCESS != s32Ret)
    {
        printf("sensor register callback function to awb lib failed!\n");
        return s32Ret;
    }
    return 0;
}

static HI_VOID sensor_ctx_exit(VI_PIPE ViPipe)
{
    ISP_SNS_STATE_S* pastSnsStateCtx = HI_NULL;

    IMX385_SENSOR_GET_CTX(ViPipe, pastSnsStateCtx);
    SENSOR_FREE(pastSnsStateCtx);
    IMX385_SENSOR_RESET_CTX(ViPipe);
}

static int imx385_sensor_unregister_callback(VI_PIPE ViPipe, ALG_LIB_S *pstAeLib, ALG_LIB_S *pstAwbLib)
{
    HI_S32 s32Ret;

    CMOS_CHECK_POINTER(pstAeLib);
    CMOS_CHECK_POINTER(pstAwbLib);

    s32Ret = HI_MPI_ISP_SensorUnRegCallBack(ViPipe, IMX385_ID);
    if (s32Ret)
    {
        printf("sensor unregister callback function failed!\n");
        return s32Ret;
    }
    
    s32Ret = HI_MPI_AE_SensorUnRegCallBack(ViPipe, pstAeLib, IMX385_ID);
    if (s32Ret)
    {
        printf("sensor unregister callback function to ae lib failed!\n");
        return s32Ret;
    }

    s32Ret = HI_MPI_AWB_SensorUnRegCallBack(ViPipe, pstAwbLib, IMX385_ID);
    if (s32Ret)
    {
        printf("sensor unregister callback function to awb lib failed!\n");
        return s32Ret;
    }
    
    sensor_ctx_exit(ViPipe);

    return 0;
}
static int imx385_sensor_set_init(VI_PIPE ViPipe, ISP_INIT_ATTR_S *pstInitAttr)
{
    CMOS_CHECK_POINTER(pstInitAttr);

    g_au32InitExposure[ViPipe] = pstInitAttr->u32Exposure;
    g_au32LinesPer500ms[ViPipe] = pstInitAttr->u32LinesPer500ms;
    g_au16InitWBGain[ViPipe][0] = pstInitAttr->u16WBRgain;
    g_au16InitWBGain[ViPipe][1] = pstInitAttr->u16WBGgain;
    g_au16InitWBGain[ViPipe][2] = pstInitAttr->u16WBBgain;
    g_au16SampleRgain[ViPipe] = pstInitAttr->u16SampleRgain;
    g_au16SampleBgain[ViPipe] = pstInitAttr->u16SampleBgain;
    
    return 0;
}


ISP_SNS_OBJ_S stSnsImx385Obj = 
{
    .pfnRegisterCallback    = imx385_sensor_register_callback,
    .pfnUnRegisterCallback  = imx385_sensor_unregister_callback,
    .pfnStandby             = imx385_standby,
    .pfnRestart             = imx385_restart,
    .pfnMirrorFlip          = HI_NULL,
    .pfnWriteReg            = imx385_write_register,
    .pfnReadReg             = imx385_read_register,
    .pfnSetBusInfo          = imx385_set_bus_info,
	.pfnSetInit             = imx385_sensor_set_init
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* __IMX385_CMOS_H_ */
