// SPDX-License-Identifier: GPL-2.0
/*
 * dhy_dynclk.c - Linux driver for Digilent axi_dynclk core
 *
 * Ported from bare-metal driver (fw/DynClk/dynclk.c) by Sam Bobrowicz
 * Original: (c) 2015 Digilent Incorporated, BSD 3-clause license
 *
 * This driver configures the axi_dynclk IP to generate a pixel clock
 * for HDMI output. On probe(), it reads the desired frequency from DTS
 * (default: 108 MHz for 1280x1024@60Hz) and starts the clock automatically.
 *
 * Reference: Xilinx XAPP888 (MMCM/PLL reconfiguration)
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/delay.h>

// Register Offsets
#define OFST_DYNCLK_CTRL          0x00
#define OFST_DYNCLK_STATUS        0x04
#define OFST_DYNCLK_CLK_L         0x08
#define OFST_DYNCLK_FB_L          0x0C
#define OFST_DYNCLK_FB_H_CLK_H    0x10
#define OFST_DYNCLK_DIV           0x14
#define OFST_DYNCLK_LOCK_L        0x18
#define OFST_DYNCLK_FLTR_LOCK_H   0x1C

#define BIT_DYNCLK_START          0
#define BIT_DYNCLK_RUNNING        0

/* ------------------------------------------------------------ */
/*                Clock Divider Constants                        */
/* ------------------------------------------------------------ */
#define CLK_BIT_WEDGE             13
#define CLK_BIT_NOCOUNT           12
#define ERR_CLKDIVIDER            (1 << CLK_BIT_WEDGE | 1 << CLK_BIT_NOCOUNT)
#define ERR_CLKCOUNTCALC          0xFFFFFFFF

/* Reference clock = 100 MHz (Zynq FCLK to axi_dynclk) */
#define DYNCLK_REF_CLK_MHZ       100

/* Default pixel clock for 1920x1080@60Hz */
#define DEFAULT_FREQ_KHZ          148500

/* ------------------------------------------------------------ */
/*                Data Structures                                */
/* ------------------------------------------------------------ */
struct dynclk_config {
	u32 clk0L;
	u32 clkFBL;
	u32 clkFBH_clk0H;
	u32 divclk;
	u32 lockL;
	u32 fltr_lockH;
};

struct dynclk_mode {
	u32 freq_khz;    /* desired frequency in kHz */
	u32 fbmult;
	u32 clkdiv;
	u32 maindiv;
};

struct dynclk_dev {
	void __iomem *regs;
	struct device *dev;
	u32 freq_khz;
};

/* ------------------------------------------------------------ */
/*     Lookup Tables (from dynclk.h, converted to u64/u32)      */
/* ------------------------------------------------------------ */
static const u64 lock_lookup[64] = {
	/* index  0- 3 */ 0x00CF967F4801ULL, 0x00CF967F4801ULL, 0x0109E7F4801ULL, 0x016BFF4801ULL,
	/* index  4- 7 */ 0x01CE77F4801ULL, 0x02319F4801ULL, 0x027CF7F4801ULL, 0x02DAD7F4801ULL,
	/* index  8-11 */ 0x033CE7F4801ULL, 0x039CE7F4801ULL, 0x03FFC27F4801ULL, 0x03FF9E7F4801ULL,
	/* index 12-15 */ 0x03FF77BF4801ULL, 0x03FF6DE7F4801ULL, 0x03FF62BF4801ULL, 0x03FF5C7F4801ULL,
	/* index 16-19 */ 0x03FF4FFF4801ULL, 0x03FF49BF4801ULL, 0x03FF437F4801ULL, 0x03FF3D3F4801ULL,
	/* index 20-23 */ 0x03FF36FF4801ULL, 0x03FF30BF4801ULL, 0x03FF2A7F4801ULL, 0x03FF243F4801ULL,
	/* index 24-27 */ 0x03FF243F4801ULL, 0x03FF1DFF4801ULL, 0x03FF17BF4801ULL, 0x03FF17BF4801ULL,
	/* index 28-31 */ 0x03FF117F4801ULL, 0x03FF117F4801ULL, 0x03FF0B3F4801ULL, 0x03FF0B3F4801ULL,
	/* index 32-35 */ 0x03FF0B3F4801ULL, 0x03FF04FF4801ULL, 0x03FF04FF4801ULL, 0x03FF04FF4801ULL,
	/* index 36-39 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 40-43 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 44-47 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 48-51 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 52-55 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 56-59 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
	/* index 60-63 */ 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL, 0x03FEFEBF4801ULL,
};

static const u32 filter_lookup_low[64] = {
	0x05F, 0x057, 0x07B, 0x05B, 0x06B, 0x073, 0x073, 0x073,
	0x073, 0x04B, 0x04B, 0x04B, 0x0B3, 0x053, 0x053, 0x053,
	0x053, 0x053, 0x053, 0x053, 0x053, 0x053, 0x053, 0x063,
	0x063, 0x063, 0x063, 0x063, 0x063, 0x063, 0x063, 0x063,
	0x063, 0x063, 0x063, 0x063, 0x063, 0x093, 0x093, 0x093,
	0x093, 0x093, 0x093, 0x093, 0x093, 0x093, 0x093, 0x0A3,
	0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3,
	0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3, 0x0A3,
};

/* ------------------------------------------------------------ */
/*            Clock Calculation Functions (from dynclk.c)        */
/* ------------------------------------------------------------ */

/**
 * dynclk_divider - Calculate divider register value
 * Ported from bare-metal ClkDivider()
 */
static u32 dynclk_divider(u32 divide)
{
	u32 output = 0;
	u32 highTime, lowTime;

	if (divide < 1 || divide > 128)
		return ERR_CLKDIVIDER;

	if (divide == 1)
		return 0x1041;

	highTime = divide / 2;
	if (divide & 1) {
		lowTime = highTime + 1;
		output = 1 << CLK_BIT_WEDGE;
	} else {
		lowTime = highTime;
	}

	output |= 0x03F & lowTime;
	output |= 0xFC0 & (highTime << 6);
	return output;
}

/**
 * dynclk_count_calc - Calculate clock count register value
 * Ported from bare-metal ClkCountCalc()
 */
static u32 dynclk_count_calc(u32 divide)
{
	u32 divCalc;

	divCalc = dynclk_divider(divide);
	if (divCalc == ERR_CLKDIVIDER)
		return ERR_CLKCOUNTCALC;

	return (0xFFF & divCalc) | ((divCalc << 10) & 0x00C00000);
}

/**
 * dynclk_find_reg - Calculate register values for given clock parameters
 * Ported from bare-metal ClkFindReg()
 */
static int dynclk_find_reg(struct dynclk_config *reg, struct dynclk_mode *mode)
{
	if (mode->fbmult < 2 || mode->fbmult > 64)
		return -EINVAL;

	reg->clk0L = dynclk_count_calc(mode->clkdiv);
	if (reg->clk0L == ERR_CLKCOUNTCALC)
		return -EINVAL;

	reg->clkFBL = dynclk_count_calc(mode->fbmult);
	if (reg->clkFBL == ERR_CLKCOUNTCALC)
		return -EINVAL;

	reg->clkFBH_clk0H = 0;

	reg->divclk = dynclk_divider(mode->maindiv);
	if (reg->divclk == ERR_CLKDIVIDER)
		return -EINVAL;

	reg->lockL = (u32)(lock_lookup[mode->fbmult - 1] & 0xFFFFFFFF);

	reg->fltr_lockH = (u32)((lock_lookup[mode->fbmult - 1] >> 32) & 0xFF);
	reg->fltr_lockH |= ((filter_lookup_low[mode->fbmult - 1] << 16) & 0x03FF0000);

	return 0;
}

/**
 * dynclk_find_params - Find best MMCM parameters for desired frequency
 * Ported from bare-metal ClkFindParams()
 *
 * @freq_khz: desired pixel clock in kHz
 * @mode: output - best matching parameters
 *
 * Note: axi_dynclk generates 5x pixel clock internally, then BUFR divides by 5.
 * All calculations use kHz (integer) instead of double to avoid kernel FP.
 */
static u32 dynclk_find_params(u32 freq_khz, struct dynclk_mode *mode)
{
	u32 best_error = 2000000; /* kHz * 5, large initial value */
	u32 cur_error;
	u32 cur_freq;
	u32 cur_div, cur_fb, cur_clkdiv;
	u32 min_fb, max_fb;
	u32 freq5 = freq_khz * 5; /* MMCM runs at 5x pixel clock */

	mode->freq_khz = 0;

	for (cur_div = 1; cur_div <= 10; cur_div++) {
		/* VCO range: 600MHz ~ 1200MHz → fbmult range */
		min_fb = cur_div * 6;
		max_fb = cur_div * 12;
		if (max_fb > 64)
			max_fb = 64;

		for (cur_fb = min_fb; cur_fb <= max_fb; cur_fb++) {
			/*
			 * cur_clkdiv = round((ref_clk / cur_div) * cur_fb / freq5)
			 * Using integer math: (ref_mhz * 1000 * cur_fb) / (cur_div * freq5)
			 */
			cur_clkdiv = (DYNCLK_REF_CLK_MHZ * 1000 * cur_fb + (cur_div * freq5 / 2))
				     / (cur_div * freq5);
			if (cur_clkdiv < 1)
				cur_clkdiv = 1;
			if (cur_clkdiv > 128)
				continue;

			/* cur_freq = (ref_clk / cur_div) * cur_fb / cur_clkdiv (in kHz) */
			cur_freq = (DYNCLK_REF_CLK_MHZ * 1000 / cur_div) * cur_fb / cur_clkdiv;

			cur_error = (cur_freq > freq5) ? (cur_freq - freq5) : (freq5 - cur_freq);

			if (cur_error < best_error) {
				best_error = cur_error;
				mode->clkdiv = cur_clkdiv;
				mode->fbmult = cur_fb;
				mode->maindiv = cur_div;
				mode->freq_khz = cur_freq / 5;
			}
		}
	}

	return best_error / 5;
}

/* ------------------------------------------------------------ */
/*              Hardware Register Access Functions               */
/* ------------------------------------------------------------ */

/**
 * dynclk_write_reg - Write configuration to hardware
 * Ported from bare-metal ClkWriteReg()
 */
static void dynclk_write_reg(struct dynclk_dev *dclk, struct dynclk_config *reg)
{
	iowrite32(reg->clk0L,          dclk->regs + OFST_DYNCLK_CLK_L);
	iowrite32(reg->clkFBL,         dclk->regs + OFST_DYNCLK_FB_L);
	iowrite32(reg->clkFBH_clk0H,  dclk->regs + OFST_DYNCLK_FB_H_CLK_H);
	iowrite32(reg->divclk,         dclk->regs + OFST_DYNCLK_DIV);
	iowrite32(reg->lockL,          dclk->regs + OFST_DYNCLK_LOCK_L);
	iowrite32(reg->fltr_lockH,     dclk->regs + OFST_DYNCLK_FLTR_LOCK_H);
}

/**
 * dynclk_start - Start the clock output
 * Ported from bare-metal ClkStart()
 */
static int dynclk_start(struct dynclk_dev *dclk)
{
	u32 timeout = 100000;

	iowrite32(1 << BIT_DYNCLK_START, dclk->regs + OFST_DYNCLK_CTRL);

	while (!(ioread32(dclk->regs + OFST_DYNCLK_STATUS) & (1 << BIT_DYNCLK_RUNNING))) {
		if (--timeout == 0) {
			dev_err(dclk->dev, "Timeout waiting for clock to start\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	return 0;
}

/**
 * dynclk_stop - Stop the clock output
 * Ported from bare-metal ClkStop()
 */
static int dynclk_stop(struct dynclk_dev *dclk)
{
	u32 timeout = 100000;

	iowrite32(0, dclk->regs + OFST_DYNCLK_CTRL);

	while (ioread32(dclk->regs + OFST_DYNCLK_STATUS) & (1 << BIT_DYNCLK_RUNNING)) {
		if (--timeout == 0) {
			dev_err(dclk->dev, "Timeout waiting for clock to stop\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	return 0;
}

/**
 * dynclk_set_freq - Configure and start clock at desired frequency
 * Ported from bare-metal DynClk_Setting()
 */
static int dynclk_set_freq(struct dynclk_dev *dclk, u32 freq_khz)
{
	struct dynclk_config reg;
	struct dynclk_mode mode;
	u32 error;
	int ret;

	/* Step 1: Find best MMCM parameters (ClkFindParams) */
	error = dynclk_find_params(freq_khz, &mode);
	dev_info(dclk->dev, "Requested: %u kHz, Actual: %u kHz, Error: %u kHz\n",
		 freq_khz, mode.freq_khz, error);
	dev_info(dclk->dev, "Params: fbmult=%u, clkdiv=%u, maindiv=%u\n",
		 mode.fbmult, mode.clkdiv, mode.maindiv);

	/* Step 2: Calculate register values (ClkFindReg) */
	ret = dynclk_find_reg(&reg, &mode);
	if (ret) {
		dev_err(dclk->dev, "Failed to calculate register values\n");
		return ret;
	}

	/* Step 3: Write registers (ClkWriteReg) */
	dynclk_write_reg(dclk, &reg);

	/* Step 4: Start clock (ClkStart) */
	ret = dynclk_start(dclk);
	if (ret)
		return ret;

	dclk->freq_khz = mode.freq_khz;
	dev_info(dclk->dev, "Clock started at %u kHz\n", mode.freq_khz);

	return 0;
}

/* ------------------------------------------------------------ */
/*              Platform Driver Functions                        */
/* ------------------------------------------------------------ */

static int dhy_dynclk_probe(struct platform_device *pdev)
{
	struct dynclk_dev *dclk;
	struct resource *res;
	u32 freq_khz;
	int ret;

	dclk = devm_kzalloc(&pdev->dev, sizeof(*dclk), GFP_KERNEL);
	if (!dclk)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dclk->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dclk->regs))
		return PTR_ERR(dclk->regs);

	dclk->dev = &pdev->dev;
	platform_set_drvdata(pdev, dclk);

	/* DTS에서 주파수 읽기, 없으면 108000 kHz (1280x1024@60Hz) */
	if (of_property_read_u32(pdev->dev.of_node, "freq_khz", &freq_khz))
		freq_khz = DEFAULT_FREQ_KHZ;

	dev_info(&pdev->dev, "Dynamic Clock probed at %pa\n", &res->start);

	/*
	 * bare-metal DynClk_Setting() 순서 그대로:
	 *   1. ClkFindParams(freq)  → dynclk_find_params()   // 원하는 주파수에 가장 근접한 MMCM 파라미터(fbmult, clkdiv, maindiv) 탐색
	 *   2. ClkFindReg()         → dynclk_find_reg()      // 위 파라미터로 실제 MMCM 레지스터 값 계산
	 *   3. ClkWriteReg()        → dynclk_write_reg()     // 계산된 레지스터 값을 하드웨어에 기록
	 *   4. ClkStart()           → dynclk_start()         // MMCM/PLL 동작 시작(클럭 출력 개시)
	 */
	ret = dynclk_set_freq(dclk, freq_khz);
	if (ret) {
		dev_err(&pdev->dev, "Failed to set clock frequency\n");
		return ret;
	}

	return 0;
}

static int dhy_dynclk_remove(struct platform_device *pdev)
{
	struct dynclk_dev *dclk = platform_get_drvdata(pdev);

	dynclk_stop(dclk);
	dev_info(&pdev->dev, "Dynamic Clock stopped and removed\n");

	return 0;
}

static const struct of_device_id dhy_dynclk_of_match[] = {
	{ .compatible = "dhy,dynclk", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, dhy_dynclk_of_match);

static struct platform_driver dhy_dynclk_driver = {
	.probe  = dhy_dynclk_probe,
	.remove = dhy_dynclk_remove,
	.driver = {
		.name = "dhy-dynclk",
		.of_match_table = dhy_dynclk_of_match,
	},
};
module_platform_driver(dhy_dynclk_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ported from Digilent bare-metal driver by Sam Bobrowicz");
MODULE_DESCRIPTION("Digilent axi_dynclk Dynamic Clock Generator driver");

