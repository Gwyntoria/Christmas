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

const unsigned char imx385_i2c_addr     =    0x34;        /* I2C Address of IMX385 */
const unsigned int  imx385_addr_byte    =    2;
const unsigned int  imx385_data_byte    =    1;
static int g_fd[ISP_MAX_PIPE_NUM] = { [0 ...(ISP_MAX_PIPE_NUM - 1)] = -1 };

extern ISP_SNS_STATE_S              *g_astimx385[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U      g_aunImx385BusInfo[];


//sensor fps mode
#define IMX385_SENSOR_1080P_30FPS_LINEAR_MODE      (1)
#define IMX385_SENSOR_1080P_30FPS_3t1_WDR_MODE     (2)
#define IMX385_SENSOR_1080P_30FPS_2t1_WDR_MODE     (3)

int imx385_i2c_init(VI_PIPE ViPipe)
{
    char acDevFile[16];
    HI_U8 u8DevNum;
    
    if(g_fd[ViPipe] >= 0)
    {
        return 0;
    }    
#ifdef HI_GPIO_I2C
    int ret;

    g_fd[ViPipe] = open("/dev/gpioi2c_ex", 0);
    if(g_fd[ViPipe] < 0)
    {
        printf("Open gpioi2c_ex error!\n");
        return -1;
    }
#else
    int ret;

    u8DevNum = g_aunImx385BusInfo[ViPipe].s8I2cDev;
    snprintf_s(acDevFile, sizeof(acDevFile), sizeof(acDevFile)-1, "/dev/i2c-%d", u8DevNum);
    
    g_fd[ViPipe] = open(acDevFile, O_RDWR);
    if(g_fd[ViPipe] < 0)
    {
        printf("Open /dev/i2c-%d error!\n", ViPipe);
        return -1;
    }

    ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, (imx385_i2c_addr>>1));
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

int imx385_read_register(VI_PIPE ViPipe,int addr)
{
    // TODO: 
    
    return 0;
}

int imx385_write_register(VI_PIPE ViPipe,int addr, int data)
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
    if(ret < 0)
    {
        printf("I2C_WRITE error!\n");
        return -1;
    }

#endif
    return 0;
}

static void delay_ms(int ms) { 
    usleep(ms*1000);
}

void imx385_prog(VI_PIPE ViPipe,int* rom) 
{
    int i = 0;
    while (1) {
        int lookup = rom[i++];
        int addr = (lookup >> 16) & 0xFFFF;
        int data = lookup & 0xFFFF;
        if (addr == 0xFFFE) {
            delay_ms(data);
        } else if (addr == 0xFFFF) {
            return;
        } else {
            imx385_write_register(ViPipe,addr, data);
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


void imx385_init(VI_PIPE ViPipe)
{
	HI_U32           i;
    WDR_MODE_E       enWDRMode;
    HI_BOOL          bInit;
	HI_U8       u8ImgMode;
    bInit       = g_astimx385[ViPipe]->bInit;
    enWDRMode   = g_astimx385[ViPipe]->enWDRMode;
	u8ImgMode = g_astimx385[ViPipe]->u8ImgMode;

    imx385_i2c_init(ViPipe);

	printf("\n               u8ImgMode %d\n", u8ImgMode);

	imx385_linear_1080p30_init(ViPipe);

	for (i=0; i<g_astimx385[ViPipe]->astRegsInfo[0].u32RegNum; i++)
	{
		imx385_write_register(ViPipe, g_astimx385[ViPipe]->astRegsInfo[0].astI2cData[i].u32RegAddr, g_astimx385[ViPipe]->astRegsInfo[0].astI2cData[i].u32Data);
	}
    g_astimx385[ViPipe]->bInit = HI_TRUE;
    return ;
}

void imx385_exit(VI_PIPE ViPipe)
{
    imx385_i2c_exit(ViPipe);

    return;
}

int sensor_imx385_write_register_i2c_TO_spi(VI_PIPE ViPipe,int addr, int data)
{
	int ispaddr = 0;

	if((addr & 0xf00) == 0x200)
	{
		ispaddr = 0x3000 +  (addr & 0xff);
	}
	else if((addr & 0xf00) == 0x300)
	{
		ispaddr = 0x3100 +  (addr & 0xff);
	}
	else if((addr & 0xf00) == 0x400)
	{
		ispaddr = 0x3200 +  (addr & 0xff);
	}
	else if((addr & 0xf00) == 0x500)
	{
		ispaddr = 0x3300 +  (addr & 0xff);
	}
	else
	{
		printf("sensor_imx385_write_register_i2c_TO_spi error.....\n");
	}
	imx385_write_register(ViPipe, ispaddr,  data);
}


/* 1080P30 and 1080P25 */
void imx385_linear_1080p30_init(VI_PIPE ViPipe)
{
       /* imx385 1080p30 */    
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x200, 0x01); /* standby */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x205, 0x01); /* ADBIT=1(12-bit), STD12EN=0*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x207, 0x0); /* MODE: All-pix scan */
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x209, 0x02); /*30 f/s*/
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x218, 0x65);/* VMAX[7:0] */
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x219, 0x04);/* VMAX[15:8] */
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x21a, 0x00);/* VMAX[16] */
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x21b, 0x30);/* HMAX[7:0] */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x21c, 0x11); /* HMAX[15:8] */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x244, 0xE1); /* ODBIT=1, OPORTSEL=0xE :CSI-2 */ //--

	
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x212, 0x2c); 
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x213, 0x01); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x254, 0x66); 


	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x30b, 0x07);
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x310, 0x12);
	
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x544, 0x10); /* REPETITION=1  */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x546, 0x03); /* PHYSICAL Lane NUM 3h:4 lanes; 2h:2 lanes */ 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x553, 0x0E); /* OB_SIZE_V[5:0]*/

	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x557, 0x49); /* PIC_SIZE_V[7:0]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x558, 0x04); /* PIC_SIZE_V[11:8]*/

	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x56b, 0x3f); /* THSEXIT: Global Timing Setting30*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x56b, 0x1f); /* TCLKPRE: Global Timing Setting*/

	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x57d, 0x0c); /* CSI_DT_FMT[7:0]*/   /* CSI_DT_FMT=0x0c0c 12bit CSI_DT_FMT=0x0a0a 10bit*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x57e, 0x0c); /* CSI_DT_FMT[15:8]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x57f, 0x03); /* CSI_LANE_MODE*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x582, 0x67); /* TCLK_POST*/

	
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x538, 0xD4); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x539, 0x40); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53A, 0x10); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53B, 0x00); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53C, 0xD4); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53D, 0x40); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53E, 0x10); 
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x53F, 0x00); 

	
    /* INCK , CSI-2 Serial output (INCK=37.125MHz) */
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x25C, 0x18); /* INCKSEL1*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x25D, 0x00); /* INCKSEL2*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x25E, 0x20); /* INCKSEL3*/
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x25F, 0x00); /* INCKSEL4*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x580, 0x20); /* INCK_FREQ[7:0]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x581, 0x25); /* INCK_FREQ[15:8]*/
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x58d, 0xb4); /* INCK_FREQ2[7:0]b4*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x58e, 0x01); /* INCK_FREQ2[10:8]01*/

	
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x56b, 0x2f);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x582, 0x5f);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x583, 0x17);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x584, 0x2f);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x585, 0x17);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x586, 0x17);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x587, 0x00);
	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x588, 0x4f);

	 sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x3ed, 0x38);
    
	 /*gain, black level, exposure, etc.*/
	sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x20A, 0xF0); /* BLKLEVEL[7:0]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x20B, 0x00); /* BLKLEVEL[8]*/

    /*gain, black level, exposure, etc.*/
	#if 0
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x20A, 0xF0); /* BLKLEVEL[7:0]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x20B, 0x00); /* BLKLEVEL[8]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x220, 0x0A); /* SHS1[7:0]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x221, 0x00); /* SHS1[15:8]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x222, 0x00); /* SHS1[16]*/
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x214, 0x34); /* GAIN*/
	#endif
    
    
    
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x200, 0x00); /* standby */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x202, 0x00); /* master mode start */
    sensor_imx385_write_register_i2c_TO_spi (ViPipe, 0x249, 0x0A); /* XVSOUTSEL XHSOUTSEL */

    printf("-------Sony IMX385 Sensor 1080p30 Initial OK!-------\n");
    return;
}



/* 1080p60 */
void imx385_linear_1080p60_init(VI_PIPE ViPipe)
{
    /* imx385 1080p60 */    
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x200, 0x01); /* standby */

     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x205, 0x01); /* ADBIT=1(12-bit), STD12EN=0*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x206, 0x00); /* MODE: All-pix scan */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x207, 0x10); /* WINMODE: HD 1080p */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x209, 0x01); /* FRSEL[1:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x218, 0x65); /* VMAX[7:0] */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x219, 0x04); /* VMAX[15:8] */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x21a, 0x00); /* VMAX[16] */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x21b, 0x4C); /* HMAX[7:0] */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x21c, 0x04); /* HMAX[15:8] */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x244, 0xE1); /* ODBIT=1, OPORTSEL=0xE :CSI-2 */

     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x503, 0x00); /* REPETITION=1  */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x505, 0x03); /* PHYSICAL Lane NUM 3h:4 lanes; 2h:2 lanes */ 
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x514, 0x08); /* OB_SIZE_V[5:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x515, 0x01); /* NULL0_SIZE_V[5:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x516, 0x04); /* NULL1_SIZE_V[5:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x517, 0x04); /* NULL2_SIZE_V[5:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x518, 0x49); /* PIC_SIZE_V[7:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x519, 0x04); /* PIC_SIZE_V[11:8]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x52c, 0x40); /* THSEXIT: Global Timing Setting30*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x52d, 0x20); /* TCLKPRE: Global Timing Setting*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x52e, 0x03); /* TLPXESC*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x53e, 0x0c); /* CSI_DT_FMT[7:0]*/   /* CSI_DT_FMT=0x0c0c 12bit CSI_DT_FMT=0x0a0a 10bit*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x53f, 0x0c); /* CSI_DT_FMT[15:8]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x540, 0x03); /* CSI_LANE_MODE*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x543, 0x68); /* TCLK_POST*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x544, 0x20); /* THS_PREPARE 10*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x545, 0x40); /* THS_ZERO_MIN 30*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x546, 0x28); /* THS_TRAIL 18*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x547, 0x20); /* TCLK_TRAIL_MIN 10*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x548, 0x18); /* TCLK_PREPARE 10*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x549, 0x78); /* TCLK_ZERO 48*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x54A, 0x28); /* TCPX*/
    
    
    /* INCK , CSI-2 Serial output (INCK=37.125MHz) */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x25C, 0x20); /* INCKSEL1*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x25D, 0x00); /* INCKSEL2*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x25E, 0x18); /* INCKSEL3*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x25F, 0x00); /* INCKSEL4*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x263, 0x74); /* INCKSEL574h*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x541, 0x20); /* INCK_FREQ[7:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x542, 0x25); /* INCK_FREQ[15:8]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x54E, 0xb4); /* INCK_FREQ2[7:0]b4*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x54F, 0x01); /* INCK_FREQ2[10:8]01*/

    /*gain, black level, exposure, etc.*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x20A, 0xF0); /* BLKLEVEL[7:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x20B, 0x00); /* BLKLEVEL[8]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x220, 0x0A); /* SHS1[7:0]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x221, 0x00); /* SHS1[15:8]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x222, 0x00); /* SHS1[16]*/
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x214, 0x34); /* GAIN*/
    
    /* registers must be changed */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x403, 0xC8);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x407, 0x54);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x413, 0x16);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x415, 0xF6);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x41A, 0x14);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x41B, 0x51);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x429, 0xE7);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x42A, 0xF0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x42B, 0x10);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x431, 0xE7);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x432, 0xF0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x433, 0x10);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x43C, 0xE8);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x43D, 0x70);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x443, 0x08);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x444, 0xE1);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x445, 0x10);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x447, 0xE7);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x448, 0x60);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x449, 0x1E);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x44B, 0x00);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x44C, 0x41);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x450, 0x30);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x451, 0x0A);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x452, 0xFF);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x453, 0xFF);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x454, 0xFF);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x455, 0x02);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x457, 0xF0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x45A, 0xA6);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x45D, 0x14);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x45E, 0x51);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x460, 0x00);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x461, 0x61);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x466, 0x30);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x467, 0x05);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x475, 0xE7);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x481, 0xEA);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x482, 0x70);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x485, 0xFF);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x48A, 0xF0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x48D, 0xB6);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x48E, 0x40);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x490, 0x42);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x491, 0x51);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x492, 0x1E);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x494, 0xC4);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x495, 0x20);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x497, 0x50);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x498, 0x31);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x499, 0x1F);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x49B, 0xC0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x49C, 0x60);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x49E, 0x4C);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x49F, 0x71);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4A0, 0x1F);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4A2, 0xB6);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4A3, 0xC0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4A4, 0x0B);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4A9, 0x24);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4AA, 0x41);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4B0, 0x25);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4B1, 0x51);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4B7, 0x1C);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4B8, 0xC1);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4B9, 0x12);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4BE, 0x1D);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4BF, 0xD1);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C0, 0x12);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C2, 0xA8);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C3, 0xC0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C4, 0x0A);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C5, 0x1E);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C6, 0x21);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4C9, 0xB0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4CA, 0x40);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4CC, 0x26);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4CD, 0xA1);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D0, 0xB6);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D1, 0xC0);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D2, 0x0B);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D4, 0xE2);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D5, 0x40);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D8, 0x4E);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4D9, 0xA1);
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x4EC, 0xF0);
    
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x200, 0x00); /* standby */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x202, 0x00); /* master mode start */
     sensor_imx385_write_register_i2c_TO_spi (ViPipe,0x249, 0x0A); /* XVSOUTSEL XHSOUTSEL */

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


