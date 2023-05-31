#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"

#ifdef HI_GPIO_I2C
#include "gpioi2c_ex.h"
#else
#include "hi_i2c.h"
#endif

const unsigned char imx385_i2c_addr = 0x34; /* I2C Address of IMX385 */
const unsigned int imx385_addr_byte = 2;
const unsigned int imx385_data_byte = 1;
static int g_fd[ISP_MAX_PIPE_NUM] = {[0 ...(ISP_MAX_PIPE_NUM - 1)] = -1};

extern ISP_SNS_STATE_S *g_astimx385[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U g_aunImx385BusInfo[];

// sensor fps mode
#define IMX385_SENSOR_1080P_30FPS_LINEAR_MODE (1)
#define IMX385_SENSOR_1080P_30FPS_3t1_WDR_MODE (2)
#define IMX385_SENSOR_1080P_30FPS_2t1_WDR_MODE (3)

int imx385_i2c_init(VI_PIPE ViPipe)
{
    char acDevFile[16];
    HI_U8 u8DevNum;

    if (g_fd[ViPipe] >= 0)
    {
        return 0;
    }
#ifdef HI_GPIO_I2C
    int ret;

    g_fd[ViPipe] = open("/dev/gpioi2c_ex", 0);
    if (g_fd[ViPipe] < 0)
    {
        printf("Open gpioi2c_ex error!\n");
        return -1;
    }
#else
    int ret;

    u8DevNum = g_aunImx385BusInfo[ViPipe].s8I2cDev;
    snprintf_s(acDevFile, sizeof(acDevFile), sizeof(acDevFile) - 1, "/dev/i2c-%d", u8DevNum);

    g_fd[ViPipe] = open(acDevFile, O_RDWR);
    if (g_fd[ViPipe] < 0)
    {
        printf("Open /dev/i2c-%d error!\n", ViPipe);
        return -1;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (imx385_i2c_addr >> 1));
    if (ret < 0)
    {
        printf("CMD_SET_DEV error!\n");
        return ret;
    }
#endif

    return 0;
}

int imx385_i2c_exit(VI_PIPE ViPipe)
{
    if (g_fd[ViPipe] >= 0)
    {
        close(g_fd[ViPipe]);
        g_fd[ViPipe] = -1;
        return 0;
    }
    return -1;
}

int imx385_read_register(VI_PIPE ViPipe, int addr)
{
    // TODO:

    return 0;
}

int imx385_write_register(VI_PIPE ViPipe, int addr, int data)
{
    if (0 > g_fd[ViPipe])
    {
        return 0;
    }

#ifdef HI_GPIO_I2C
    i2c_data.dev_addr = imx385_i2c_addr;
    i2c_data.reg_addr = addr;
    i2c_data.addr_byte_num = imx385_addr_byte;
    i2c_data.data = data;
    i2c_data.data_byte_num = imx385_data_byte;

    ret = ioctl(g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data);

    if (ret)
    {
        printf("GPIO-I2C write faild!\n");
        return ret;
    }
#else
    int idx = 0;
    int ret;
    char buf[8];

    if (imx385_addr_byte == 2)
    {
        buf[idx] = (addr >> 8) & 0xff;
        idx++;
        buf[idx] = addr & 0xff;
        idx++;
    }
    else
    {
        //    buf[idx] = addr & 0xff;
        //    idx++;
    }

    if (imx385_data_byte == 2)
    {
        //    buf[idx] = (data >> 8) & 0xff;
        //    idx++;
        //    buf[idx] = data & 0xff;
        //    idx++;
    }
    else
    {
        buf[idx] = data & 0xff;
        idx++;
    }

    ret = write(g_fd[ViPipe], buf, imx385_addr_byte + imx385_data_byte);
    if (ret < 0)
    {
        printf("I2C_WRITE error!\n");
        return -1;
    }

#endif
    return 0;
}

static void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void imx385_prog(VI_PIPE ViPipe, int *rom)
{
    int i = 0;
    while (1)
    {
        int lookup = rom[i++];
        int addr = (lookup >> 16) & 0xFFFF;
        int data = lookup & 0xFFFF;
        if (addr == 0xFFFE)
        {
            delay_ms(data);
        }
        else if (addr == 0xFFFF)
        {
            return;
        }
        else
        {
            imx385_write_register(ViPipe, addr, data);
        }
    }
}

void imx385_standby(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void imx385_restart(VI_PIPE ViPipe)
{
    // TODO:
    return;
}

void imx385_wdr_1080p30_2to1_init(VI_PIPE ViPipe);
void imx385_wdr_1080p60_2to1_init(VI_PIPE ViPipe);
void imx385_wdr_1080p120_2to1_init(VI_PIPE ViPipe);
void imx385_wdr_720p60_2to1_init(VI_PIPE ViPipe);
void imx385_wdr_1080p30_3to1_init(VI_PIPE ViPipe);
void imx385_wdr_1080p120_3to1_init(VI_PIPE ViPipe);
void imx385_wdr_720p60_3to1_init(VI_PIPE ViPipe);
void imx385_linear_1080p30_init(VI_PIPE ViPipe);
void imx385_linear_1080p60_init(VI_PIPE ViPipe);

void imx385_init(VI_PIPE ViPipe)
{
    HI_U32 i;
    // WDR_MODE_E       enWDRMode;
    // HI_BOOL          bInit;
    HI_U8 u8ImgMode;
    // bInit       = g_astimx385[ViPipe]->bInit;
    // enWDRMode   = g_astimx385[ViPipe]->enWDRMode;
    u8ImgMode = g_astimx385[ViPipe]->u8ImgMode;

    imx385_i2c_init(ViPipe);

    printf("\n      u8ImgMode %d\n", u8ImgMode);

    /* Sensor Type */
    imx385_linear_1080p30_init(ViPipe);
    // imx385_linear_1080p60_init (ViPipe);

    for (i = 0; i < g_astimx385[ViPipe]->astRegsInfo[0].u32RegNum; i++)
    {
        imx385_write_register(ViPipe, g_astimx385[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_astimx385[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
    }
    g_astimx385[ViPipe]->bInit = HI_TRUE;
    return;
}

void imx385_exit(VI_PIPE ViPipe)
{
    imx385_i2c_exit(ViPipe);

    return;
}

// int sensor_imx385_write_register_i2c_TO_spi(VI_PIPE ViPipe, int addr, int data)
// {
//     int ispaddr = 0;

//     if ((addr & 0xf00) == 0x200)
//     {
//         ispaddr = 0x3000 + (addr & 0xff);
//     }
//     else if ((addr & 0xf00) == 0x300)
//     {
//         ispaddr = 0x3100 + (addr & 0xff);
//     }
//     else if ((addr & 0xf00) == 0x400)
//     {
//         ispaddr = 0x3200 + (addr & 0xff);
//     }
//     else if ((addr & 0xf00) == 0x500)
//     {
//         ispaddr = 0x3300 + (addr & 0xff);
//     }
//     else
//     {
//         printf("sensor_imx385_write_register_i2c_TO_spi error.....\n");
//     }
//     imx385_write_register(ViPipe, ispaddr, data);
//     return 0;
// }

/* 1080p_30fps and 1080p25fps */
void imx385_linear_1080p30_init(VI_PIPE ViPipe)
{
    /* imx385 1080p30 */
    imx385_write_register(ViPipe, 0x3000, 0x01); /* standby */
    imx385_write_register(ViPipe, 0x3005, 0x01); /* ADBIT=1(12-bit), STD12EN=0 */
    imx385_write_register(ViPipe, 0x3007, 0x0);  /* MODE: All-pix scan */
    imx385_write_register(ViPipe, 0x3009, 0x02); /* 30 fps */
    imx385_write_register(ViPipe, 0x3018, 0x65); /* VMAX[7:0] */
    imx385_write_register(ViPipe, 0x3019, 0x04); /* VMAX[15:8] */
    imx385_write_register(ViPipe, 0x301a, 0x00); /* VMAX[16] */
    imx385_write_register(ViPipe, 0x301b, 0x30); /* HMAX[7:0] */
    imx385_write_register(ViPipe, 0x301c, 0x11); /* HMAX[15:8] */
    imx385_write_register(ViPipe, 0x3044, 0xE1); /* ODBIT=1, OPORTSEL=0xE :CSI-2 */

    imx385_write_register(ViPipe, 0x3012, 0x2c);
    imx385_write_register(ViPipe, 0x3013, 0x01);
    imx385_write_register(ViPipe, 0x3054, 0x66);

    imx385_write_register(ViPipe, 0x310b, 0x07);
    imx385_write_register(ViPipe, 0x3110, 0x12);

    imx385_write_register(ViPipe, 0x3344, 0x10); /* REPETITION=1  */
    imx385_write_register(ViPipe, 0x3346, 0x03); /* PHYSICAL Lane NUM 3h:4 lanes; 2h:2 lanes */
    imx385_write_register(ViPipe, 0x3353, 0x0E); /* OB_SIZE_V[5:0] */

    imx385_write_register(ViPipe, 0x3357, 0x49); /* PIC_SIZE_V[7:0] */
    imx385_write_register(ViPipe, 0x3358, 0x04); /* PIC_SIZE_V[11:8] */

    imx385_write_register(ViPipe, 0x336b, 0x3f); /* THSEXIT: Global Timing Setting30 */
    imx385_write_register(ViPipe, 0x336b, 0x1f); /* TCLKPRE: Global Timing Setting */

    imx385_write_register(ViPipe, 0x337d, 0x0c); /* CSI_DT_FMT[7:0]*/ /* CSI_DT_FMT=0x0c0c 12bit CSI_DT_FMT=0x0a0a 10bit */
    imx385_write_register(ViPipe, 0x337e, 0x0c);                      /* CSI_DT_FMT[15:8] */
    imx385_write_register(ViPipe, 0x337f, 0x03);                      /* CSI_LANE_MODE */
    imx385_write_register(ViPipe, 0x3382, 0x67);                      /* TCLK_POST */

    imx385_write_register(ViPipe, 0x3338, 0xD4);
    imx385_write_register(ViPipe, 0x3339, 0x40);
    imx385_write_register(ViPipe, 0x333A, 0x10);
    imx385_write_register(ViPipe, 0x333B, 0x00);
    imx385_write_register(ViPipe, 0x333C, 0xD4);
    imx385_write_register(ViPipe, 0x333D, 0x40);
    imx385_write_register(ViPipe, 0x333E, 0x10);
    imx385_write_register(ViPipe, 0x333F, 0x00);

    /* INCK , CSI-2 Serial output (INCK=37.125MHz) */
    imx385_write_register(ViPipe, 0x305C, 0x18); /* INCKSEL1 */
    imx385_write_register(ViPipe, 0x305D, 0x00); /* INCKSEL2 */
    imx385_write_register(ViPipe, 0x305E, 0x20); /* INCKSEL3 */
    imx385_write_register(ViPipe, 0x305F, 0x00); /* INCKSEL4 */
    imx385_write_register(ViPipe, 0x3380, 0x20); /* INCK_FREQ[7:0] */
    imx385_write_register(ViPipe, 0x3381, 0x25); /* INCK_FREQ[15:8] */
    imx385_write_register(ViPipe, 0x338d, 0xb4); /* INCK_FREQ2[7:0]b4 */
    imx385_write_register(ViPipe, 0x338e, 0x01); /* INCK_FREQ2[10:8]01 */

    imx385_write_register(ViPipe, 0x336b, 0x2f);
    imx385_write_register(ViPipe, 0x3382, 0x5f);
    imx385_write_register(ViPipe, 0x3383, 0x17);
    imx385_write_register(ViPipe, 0x3384, 0x2f);
    imx385_write_register(ViPipe, 0x3385, 0x17);
    imx385_write_register(ViPipe, 0x3386, 0x17);
    imx385_write_register(ViPipe, 0x3387, 0x00);
    imx385_write_register(ViPipe, 0x3388, 0x4f);

    imx385_write_register(ViPipe, 0x31ed, 0x38);

    /*gain, black level, exposure, etc.*/
    imx385_write_register(ViPipe, 0x300A, 0xF0); /* BLKLEVEL[7:0] */
    imx385_write_register(ViPipe, 0x300B, 0x00); /* BLKLEVEL[8] */

    /*gain, black level, exposure, etc.*/
#if 0
    imx385_write_register (ViPipe, 0x300A, 0xF0); /* BLKLEVEL[7:0]*/
    imx385_write_register (ViPipe, 0x300B, 0x00); /* BLKLEVEL[8]*/
    imx385_write_register (ViPipe, 0x3020, 0x0A); /* SHS1[7:0]*/
    imx385_write_register (ViPipe, 0x3021, 0x00); /* SHS1[15:8]*/
    imx385_write_register (ViPipe, 0x3022, 0x00); /* SHS1[16]*/
    imx385_write_register (ViPipe, 0x3014, 0x34); /* GAIN*/
#endif

    imx385_write_register(ViPipe, 0x3000, 0x00); /* standby */
    imx385_write_register(ViPipe, 0x3002, 0x00); /* master mode start */
    imx385_write_register(ViPipe, 0x3049, 0x0A); /* XVSOUTSEL XHSOUTSEL */

    printf("-------Sony IMX385 Sensor 1080p30 Initial OK!-------\n");
    return;
}

/* 1080p_60fps and 1080p_50fps */
void imx385_linear_1080p60_init(VI_PIPE ViPipe)
{
    /* imx385 1080p60 */
    imx385_write_register(ViPipe, 0x3000, 0x01); /* standby */
    imx385_write_register(ViPipe, 0x3005, 0x01); /* ADBIT=1(12-bit), STD12EN=0 */
    imx385_write_register(ViPipe, 0x3007, 0x00); /* WINMODE: All-pix scan */
    imx385_write_register(ViPipe, 0x3009, 0x01); /* 60 fps */
    imx385_write_register(ViPipe, 0x3018, 0x65); /* VMAX[7:0] */
    imx385_write_register(ViPipe, 0x3019, 0x04); /* VMAX[15:8] */
    imx385_write_register(ViPipe, 0x301a, 0x00); /* VMAX[16] */
    imx385_write_register(ViPipe, 0x301b, 0x98); /* HMAX[7:0] */
    imx385_write_register(ViPipe, 0x301c, 0x08); /* HMAX[15:8] */
    imx385_write_register(ViPipe, 0x3044, 0x01); /* ODBIT=1, OPORTSEL=0x0 :CSI-2 */

    imx385_write_register(ViPipe, 0x3012, 0x2c);
    imx385_write_register(ViPipe, 0x3013, 0x01);
    imx385_write_register(ViPipe, 0x3054, 0x66);

    imx385_write_register(ViPipe, 0x310b, 0x07);
    imx385_write_register(ViPipe, 0x3110, 0x12);

    imx385_write_register(ViPipe, 0x3344, 0x01); /* REPETITION = 1  */
    imx385_write_register(ViPipe, 0x3346, 0x03); /* PHYSICAL_LANE_NUM 3h:4 lanes; 2h:2 lanes */
    imx385_write_register(ViPipe, 0x3353, 0x0E); /* OB_SIZE_V[5:0] */

    imx385_write_register(ViPipe, 0x3357, 0x49); /* PIC_SIZE_V[7:0] */
    imx385_write_register(ViPipe, 0x3358, 0x04); /* PIC_SIZE_V[11:8] */

    imx385_write_register(ViPipe, 0x336B, 0x3F); /* THSEXIT: Global Timing Setting30 */
    imx385_write_register(ViPipe, 0x336C, 0x1F); /* TCLKPRE: Global Timing Setting */

    imx385_write_register(ViPipe, 0x337D, 0x0C); /* CSI_DT_FMT[7:0]*/ /* CSI_DT_FMT=0x0c0c 12bit CSI_DT_FMT=0x0a0a 10bit */
    imx385_write_register(ViPipe, 0x337E, 0x0C);                      /* CSI_DT_FMT[15:8] */

    imx385_write_register(ViPipe, 0x337F, 0x03); /* 1h: 2 lane; 3h: 4lane */

    imx385_write_register(ViPipe, 0x3338, 0xD4);
    imx385_write_register(ViPipe, 0x3339, 0x40);
    imx385_write_register(ViPipe, 0x333A, 0x10);
    imx385_write_register(ViPipe, 0x333B, 0x00);
    imx385_write_register(ViPipe, 0x333C, 0xD4);
    imx385_write_register(ViPipe, 0x333D, 0x40);
    imx385_write_register(ViPipe, 0x333E, 0x10);
    imx385_write_register(ViPipe, 0x333F, 0x00);

    imx385_write_register(ViPipe, 0x3382, 0x67);
    imx385_write_register(ViPipe, 0x3383, 0x1f);
    imx385_write_register(ViPipe, 0x3384, 0x3f);
    imx385_write_register(ViPipe, 0x3385, 0x27);
    imx385_write_register(ViPipe, 0x3386, 0x1f);
    imx385_write_register(ViPipe, 0x3387, 0x17);
    imx385_write_register(ViPipe, 0x3388, 0x77);
    imx385_write_register(ViPipe, 0x3389, 0x27);

    /* INCK , CSI-2 Serial output (INCK=37.125MHz) */
    imx385_write_register(ViPipe, 0x305C, 0x18); /* INCKSEL1 */
    imx385_write_register(ViPipe, 0x305D, 0x00); /* INCKSEL2 */
    imx385_write_register(ViPipe, 0x305E, 0x20); /* INCKSEL3 */
    imx385_write_register(ViPipe, 0x305F, 0x00); /* INCKSEL4 */
    imx385_write_register(ViPipe, 0x3380, 0x20); /* INCK_FREQ[7:0] */
    imx385_write_register(ViPipe, 0x3381, 0x25); /* INCK_FREQ[15:8] */
    imx385_write_register(ViPipe, 0x338d, 0xb4); /* INCK_FREQ2[7:0]b4 */
    imx385_write_register(ViPipe, 0x338e, 0x01); /* INCK_FREQ2[10:8]01 */

    /*gain, black level, exposure, etc.*/
    imx385_write_register(ViPipe, 0x300A, 0xF0); /* BLKLEVEL[7:0] */
    imx385_write_register(ViPipe, 0x300B, 0x00); /* BLKLEVEL[8] */
    imx385_write_register(ViPipe, 0x3020, 0x0A); /* SHS1[7:0] */
    imx385_write_register(ViPipe, 0x3021, 0x00); /* SHS1[15:8] */
    imx385_write_register(ViPipe, 0x3022, 0x00); /* SHS1[16] */
    imx385_write_register(ViPipe, 0x3014, 0x34); /* GAIN */

    /* registers must be changed */
    // imx385_write_register (ViPipe, 0x3203, 0xC8);
    // imx385_write_register (ViPipe, 0x3207, 0x54);
    // imx385_write_register (ViPipe, 0x3213, 0x16);
    // imx385_write_register (ViPipe, 0x3215, 0xF6);
    // imx385_write_register (ViPipe, 0x321A, 0x14);
    // imx385_write_register (ViPipe, 0x321B, 0x51);
    // imx385_write_register (ViPipe, 0x3229, 0xE7);
    // imx385_write_register (ViPipe, 0x322A, 0xF0);
    // imx385_write_register (ViPipe, 0x322B, 0x10);
    // imx385_write_register (ViPipe, 0x3231, 0xE7);
    // imx385_write_register (ViPipe, 0x3232, 0xF0);
    // imx385_write_register (ViPipe, 0x3233, 0x10);
    // imx385_write_register (ViPipe, 0x323C, 0xE8);
    // imx385_write_register (ViPipe, 0x323D, 0x70);
    // imx385_write_register (ViPipe, 0x3243, 0x08);
    // imx385_write_register (ViPipe, 0x3244, 0xE1);
    // imx385_write_register (ViPipe, 0x3245, 0x10);
    // imx385_write_register (ViPipe, 0x3247, 0xE7);
    // imx385_write_register (ViPipe, 0x3248, 0x60);
    // imx385_write_register (ViPipe, 0x3249, 0x1E);
    // imx385_write_register (ViPipe, 0x324B, 0x00);
    // imx385_write_register (ViPipe, 0x324C, 0x41);
    // imx385_write_register (ViPipe, 0x3250, 0x30);
    // imx385_write_register (ViPipe, 0x3251, 0x0A);
    // imx385_write_register (ViPipe, 0x3252, 0xFF);
    // imx385_write_register (ViPipe, 0x3253, 0xFF);
    // imx385_write_register (ViPipe, 0x3254, 0xFF);
    // imx385_write_register (ViPipe, 0x3255, 0x02);
    // imx385_write_register (ViPipe, 0x3257, 0xF0);
    // imx385_write_register (ViPipe, 0x325A, 0xA6);
    // imx385_write_register (ViPipe, 0x325D, 0x14);
    // imx385_write_register (ViPipe, 0x325E, 0x51);
    // imx385_write_register (ViPipe, 0x3260, 0x00);
    // imx385_write_register (ViPipe, 0x3261, 0x61);
    // imx385_write_register (ViPipe, 0x3266, 0x30);
    // imx385_write_register (ViPipe, 0x3267, 0x05);
    // imx385_write_register (ViPipe, 0x3275, 0xE7);
    // imx385_write_register (ViPipe, 0x3281, 0xEA);
    // imx385_write_register (ViPipe, 0x3282, 0x70);
    // imx385_write_register (ViPipe, 0x3285, 0xFF);
    // imx385_write_register (ViPipe, 0x328A, 0xF0);
    // imx385_write_register (ViPipe, 0x328D, 0xB6);
    // imx385_write_register (ViPipe, 0x328E, 0x40);
    // imx385_write_register (ViPipe, 0x3290, 0x42);
    // imx385_write_register (ViPipe, 0x3291, 0x51);
    // imx385_write_register (ViPipe, 0x3292, 0x1E);
    // imx385_write_register (ViPipe, 0x3294, 0xC4);
    // imx385_write_register (ViPipe, 0x3295, 0x20);
    // imx385_write_register (ViPipe, 0x3297, 0x50);
    // imx385_write_register (ViPipe, 0x3298, 0x31);
    // imx385_write_register (ViPipe, 0x3299, 0x1F);
    // imx385_write_register (ViPipe, 0x329B, 0xC0);
    // imx385_write_register (ViPipe, 0x329C, 0x60);
    // imx385_write_register (ViPipe, 0x329E, 0x4C);
    // imx385_write_register (ViPipe, 0x329F, 0x71);
    // imx385_write_register (ViPipe, 0x32A0, 0x1F);
    // imx385_write_register (ViPipe, 0x32A2, 0xB6);
    // imx385_write_register (ViPipe, 0x32A3, 0xC0);
    // imx385_write_register (ViPipe, 0x32A4, 0x0B);
    // imx385_write_register (ViPipe, 0x32A9, 0x24);
    // imx385_write_register (ViPipe, 0x32AA, 0x41);
    // imx385_write_register (ViPipe, 0x32B0, 0x25);
    // imx385_write_register (ViPipe, 0x32B1, 0x51);
    // imx385_write_register (ViPipe, 0x32B7, 0x1C);
    // imx385_write_register (ViPipe, 0x32B8, 0xC1);
    // imx385_write_register (ViPipe, 0x32B9, 0x12);
    // imx385_write_register (ViPipe, 0x32BE, 0x1D);
    // imx385_write_register (ViPipe, 0x32BF, 0xD1);
    // imx385_write_register (ViPipe, 0x32C0, 0x12);
    // imx385_write_register (ViPipe, 0x32C2, 0xA8);
    // imx385_write_register (ViPipe, 0x32C3, 0xC0);
    // imx385_write_register (ViPipe, 0x32C4, 0x0A);
    // imx385_write_register (ViPipe, 0x32C5, 0x1E);
    // imx385_write_register (ViPipe, 0x32C6, 0x21);
    // imx385_write_register (ViPipe, 0x32C9, 0xB0);
    // imx385_write_register (ViPipe, 0x32CA, 0x40);
    // imx385_write_register (ViPipe, 0x32CC, 0x26);
    // imx385_write_register (ViPipe, 0x32CD, 0xA1);
    // imx385_write_register (ViPipe, 0x32D0, 0xB6);
    // imx385_write_register (ViPipe, 0x32D1, 0xC0);
    // imx385_write_register (ViPipe, 0x32D2, 0x0B);
    // imx385_write_register (ViPipe, 0x32D4, 0xE2);
    // imx385_write_register (ViPipe, 0x32D5, 0x40);
    // imx385_write_register (ViPipe, 0x32D8, 0x4E);
    // imx385_write_register (ViPipe, 0x32D9, 0xA1);
    // imx385_write_register (ViPipe, 0x32EC, 0xF0);

    imx385_write_register(ViPipe, 0x3000, 0x00); /* standby */
    imx385_write_register(ViPipe, 0x3002, 0x00); /* master mode start */
    imx385_write_register(ViPipe, 0x3049, 0x0A); /* XVSOUTSEL XHSOUTSEL */

    printf("-------Sony IMX385 Sensor 1080p60 Initial OK!-------\n");
}

void imx385_wdr_1080p30_2to1_init(VI_PIPE ViPipe)
{

    printf("-------Sony IMX385 Sensor Built-in WDR 1080p30 Initial OK!-------\n");

    return;
}

void imx385_wdr_1080p60_2to1_init(VI_PIPE ViPipe)
{

    return;
}

void imx385_wdr_1080p30_3to1_init(VI_PIPE ViPipe)
{

    printf("===Imx385 imx385 1080P15fps 12bit 3to1 WDR(30fps->7p5fps) init success!=====\n");

    return;
}

void imx385_wdr_720p60_2to1_init(VI_PIPE ViPipe)
{

    return;
}

void imx385_wdr_720p60_3to1_init(VI_PIPE ViPipe)
{

    printf("===Imx385 imx385 720P15fps 12bit 3to1 WDR(60fps->15fps) init success!=====\n");
    return;
}

void imx385_wdr_1080p120_2to1_init(VI_PIPE ViPipe)
{

    printf("===Imx385 imx385 1080P60fps 10bit 2to1 WDR(120fps->60fps) init success!=====\n");
    return;
}

void imx385_wdr_1080p120_3to1_init(VI_PIPE ViPipe)
{

    printf("===Imx385 imx385 1080P30fps 10bit 3to1 WDR(120fps->30fps) init success!=====\n");

    return;
}
