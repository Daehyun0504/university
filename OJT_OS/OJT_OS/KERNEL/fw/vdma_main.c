/*
 * Alice 190815
 */
#include "xaxivdma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "VDMA/vdma_api.h"
#include "xgpio.h"
#include "xscugic.h"
#include <stdio.h>
#include "pthread.h"
#include "xil_io.h"
#include "TPG/tpg.h"
#include "xv_tpg.h"
#include "DynClk/dynclk.h"
#include "xvtc.h"
#include "vga_modes.h"

/*
 * Def
 */
#define NUMBER_OF_FRAMES	1

#define PS_DDR_BASE_ADDR	XPAR_PS7_DDR_0_S_AXI_BASEADDR //XPAR_PS7_DDR_0_S_AXI_BASEADDR	//XPAR_PL_DDR3_MEM_BLOCK_MIG_7SERIES_0_BASEADDR
#define PS_MEM_BASE_ADDR	(PS_DDR_BASE_ADDR + 0x01000000)
#define BUF_ARR				(PS_MEM_BASE_ADDR + 0x02200000)

#define MAX_VDMA_CNT		XPAR_XAXIVDMA_NUM_INSTANCES
#define DYN_CLK_ADDR		XPAR_ZYNQ_SYSTEM_AXI_DYNCLK_BASEADDR
#define VTC_ID				XPAR_HDMI_OUT_V_TC_DEVICE_ID
#define TPG_ID				XPAR_XV_TPG_0_DEVICE_ID

#define SW_ADDR				XPAR_PL_GPIO_PL_SWITCH_BASEADDR
#define BTN_ADDR			XPAR_PL_GPIO_PL_BUTTON_BASEADDR
#define LED_ADDR			XPAR_PL_GPIO_PL_LED_BASEADDR
#define RGB_ADDR			XPAR_PL_GPIO_PL_RGB_LED_BASEADDR

#define GPIO_CHAN			1

#define GPIO_SET(a)			XGpio_WriteReg((a), ((GPIO_CHAN - 1) * XGPIO_CHAN_OFFSET) + XGPIO_TRI_OFFSET, (0x00));
#define GPIO_WRITE(a,b)		XGpio_WriteReg((a), ((GPIO_CHAN - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA_OFFSET, b);
#define GPIO_READ(a)		XGpio_ReadReg(a, ((GPIO_CHAN - 1) * XGPIO_CHAN_OFFSET) + XGPIO_DATA_OFFSET);

/*
 * Device instance definitions
 */
static XAxiVdma AxiVdma[XPAR_XAXIVDMA_NUM_INSTANCES];
static XVtc_Timing vtcTiming;
static XVtc_SourceSelect SourceSelect;
static XVtc vtc;

void VTC_Setting(VideoMode *mode);
void DynClk_Setting(VideoMode *mode);
void TPG_Setting(VideoMode *mode, u32 dpMode);
void VDMA_Setting(VideoMode *mode);

void Set_Video_Size(VideoMode *mode, u32 dpMode)
{
	TPG_Setting(mode, dpMode);
	DynClk_Setting(mode);
	VTC_Setting(mode);
	VDMA_Setting(mode);
}

int main(void)
{
	u32 dpMode[20] = {
			XTPG_BKGND_H_RAMP,
			XTPG_BKGND_V_RAMP,
			XTPG_BKGND_TEMPORAL_RAMP,
			XTPG_BKGND_SOLID_RED,
			XTPG_BKGND_SOLID_GREEN,
			XTPG_BKGND_SOLID_BLUE,
			XTPG_BKGND_SOLID_BLACK,
			XTPG_BKGND_SOLID_WHITE,
			XTPG_BKGND_COLOR_BARS,
			XTPG_BKGND_ZONE_PLATE,
			XTPG_BKGND_TARTAN_COLOR_BARS,
			XTPG_BKGND_CROSS_HATCH,
			XTPG_BKGND_RAINBOW_COLOR,
			XTPG_BKGND_HV_RAMP,
			XTPG_BKGND_CHECKER_BOARD,
			XTPG_BKGND_PBRS,
			XTPG_BKGND_DP_COLOR_RAMP,
			XTPG_BKGND_DP_BW_VERTICAL_LINE,
			XTPG_BKGND_DP_COLOR_SQUARE,
			XTPG_BKGND_LAST
	};
	VideoMode *mode[6] = {
			&VMODE_1920x1080,
			&VMODE_1280x720,
			&VMODE_1280x1024,
			&VMODE_1024x768,
			&VMODE_800x600,
			&VMODE_640x480
	};
	VideoMode *setMode;
	u32 u32Btn = 0x00;
	u32 u32SW = 0x00;
	u32 u32Led = 0x00;
	u32 u32Rgb = 0x00;
	u32 rgbCnt  = 0;
	int i=0;

	GPIO_SET(LED_ADDR);
	GPIO_SET(RGB_ADDR);
	GPIO_SET(BTN_ADDR);
	GPIO_SET(SW_ADDR);

	GPIO_WRITE(LED_ADDR, u32Led);
	GPIO_WRITE(RGB_ADDR, u32Rgb);

	u32Btn = GPIO_READ(BTN_ADDR);

//	setMode = mode[3]; // 1024*768
	setMode = mode[2]; // 1280*720

	/*
	for(i=0; i<4; i++){
		if((u32Btn >> i) & 0x1){
			setMode = mode[i+1];
			break;
		}
	}
	*/

	u32SW = GPIO_READ(SW_ADDR);
	Set_Video_Size(setMode, dpMode[u32SW]);

	for(;;usleep(1)){
		u32SW = GPIO_READ(SW_ADDR);
		if(u32Led != u32SW){
			u32Led = u32SW;
			GPIO_WRITE(LED_ADDR, u32Led);
			TPG_Setting(setMode, dpMode[u32SW]);
		}

		// RGB LED SET
		if(rgbCnt > 100000){
			rgbCnt = 0;
			u32Rgb = u32Rgb + 0x01;
			GPIO_WRITE(RGB_ADDR, u32Rgb);
			if(u32Rgb >= 0x7)
				u32Rgb = 0x00;
		}
		rgbCnt++;
	}
	return XST_SUCCESS;
}

void VDMA_Setting(VideoMode *mode){
	int i=0;
	// VDMA INIT
	for(i=0; i<MAX_VDMA_CNT; i++){
		run_triple_frame_buffer(&AxiVdma[i], i, mode->width,
				mode->height, BUF_ARR, BUF_ARR, NUMBER_OF_FRAMES,
				0, 0);
	}
}

void TPG_Setting(VideoMode *mode, u32 dpMode){
	tpg_init(TPG_ID);
	tpg_disable_restart();
	set_tpg_width(mode->width);
	set_tpg_height(mode->height);
	set_tpg_colorformat(0);
	set_tpg_bckgndId(dpMode);
	set_tpg_overlayId(0);
	tpg_enable_restart();
	tpg_start();
}

void DynClk_Setting(VideoMode *mode){
	ClkConfig clkReg;
	ClkMode clkMode;

	ClkFindParams(mode->freq, &clkMode);
	ClkFindReg(&clkReg, &clkMode);
	ClkWriteReg(&clkReg, DYN_CLK_ADDR);
	ClkStart(DYN_CLK_ADDR);
}

void VTC_Setting(VideoMode *mode){
	XVtc_Config *vtcConfig;

	// VTC Set
	vtcConfig = XVtc_LookupConfig(VTC_ID);
	XVtc_CfgInitialize(&vtc, vtcConfig, vtcConfig->BaseAddress);

	vtcTiming.HActiveVideo = mode->width;	/**< Horizontal Active Video Size */
	vtcTiming.HFrontPorch = mode->hps - mode->width;	/**< Horizontal Front Porch Size */
	vtcTiming.HSyncWidth = mode->hpe - mode->hps;		/**< Horizontal Sync Width */
	vtcTiming.HBackPorch = mode->hmax - mode->hpe + 1;		/**< Horizontal Back Porch Size */
	vtcTiming.HSyncPolarity = mode->hpol;	/**< Horizontal Sync Polarity */
	vtcTiming.VActiveVideo = mode->height;	/**< Vertical Active Video Size */
	vtcTiming.V0FrontPorch = mode->vps - mode->height;	/**< Vertical Front Porch Size */
	vtcTiming.V0SyncWidth = mode->vpe - mode->vps;	/**< Vertical Sync Width */
	vtcTiming.V0BackPorch = mode->vmax - mode->vpe + 1;;	/**< Horizontal Back Porch Size */
	vtcTiming.V1FrontPorch = mode->vps - mode->height;	/**< Vertical Front Porch Size */
	vtcTiming.V1SyncWidth = mode->vpe - mode->vps;	/**< Vertical Sync Width */
	vtcTiming.V1BackPorch = mode->vmax - mode->vpe + 1;;	/**< Horizontal Back Porch Size */
	vtcTiming.VSyncPolarity = mode->vpol;	/**< Vertical Sync Polarity */
	vtcTiming.Interlaced = 0;		/**< Interlaced / Progressive video */

	memset((void *)&SourceSelect, 0, sizeof(SourceSelect));
	SourceSelect.VBlankPolSrc = 1;
	SourceSelect.VSyncPolSrc = 1;
	SourceSelect.HBlankPolSrc = 1;
	SourceSelect.HSyncPolSrc = 1;
	SourceSelect.ActiveVideoPolSrc = 1;
	SourceSelect.ActiveChromaPolSrc= 1;
	SourceSelect.VChromaSrc = 1;
	SourceSelect.VActiveSrc = 1;
	SourceSelect.VBackPorchSrc = 1;
	SourceSelect.VSyncSrc = 1;
	SourceSelect.VFrontPorchSrc = 1;
	SourceSelect.VTotalSrc = 1;
	SourceSelect.HActiveSrc = 1;
	SourceSelect.HBackPorchSrc = 1;
	SourceSelect.HSyncSrc = 1;
	SourceSelect.HFrontPorchSrc = 1;
	SourceSelect.HTotalSrc = 1;

	XVtc_SelfTest(&vtc);

	XVtc_RegUpdateEnable(&vtc);
	XVtc_SetGeneratorTiming(&vtc, &vtcTiming);
	XVtc_SetSource(&vtc, &SourceSelect);
	XVtc_EnableGenerator(&vtc);
}
