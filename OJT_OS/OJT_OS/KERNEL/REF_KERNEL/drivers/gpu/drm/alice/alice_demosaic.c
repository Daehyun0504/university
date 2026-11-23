#include <linux/bitops.h>
#include <linux/dmapool.h>
#include <linux/dma/xilinx_dma.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_dma.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io-64-nonatomic-lo-hi.h>


#define DEMOSAIC_CONTROL                                0x00
#define DEMOSAIC_GLOBAL_INTERRUPT_ENABLE                0x04
#define DEMOSAIC_INTERRUPT_ENABLE                       0x08
#define DEMOSAIC_INTERRUPT_STATUS                       0x0C
#define DEMOSAIC_WIDTH_SIZE                             0x10
#define DEMOSAIC_HEIGTH_SIZE                            0x18
#define DEMOSAIC_BAYER_PHASE                            0x28

enum eDemosaic_BAYER {
	eRGGB  = 0,
	eGRBG,
	eGBRG,
	eBGGR,
};

struct demosaic_dev {
	void __iomem *regs;
	u32 bayer;
	u32 HSize;
	u32 VSize;
};

/*
 * DEMOSAIC_CONTROL
 * bit    name
 * 0      ap_start
 * 1      ap_done
 * 2      ap_idle
 * 3      ap_ready
 * 4      reserved
 * 5      reserved
 * 6      reserved
 * 7      auto_restart
 */
void demosaic_SetCtrl(struct demosaic_dev *pdev, 
		int ap_start, 
		int ap_done, 
		int ap_idle, 
		int ap_ready, 
		int auto_restart)
{
	u32 status;
	if(pdev != NULL)
	{
		status = ioread32(pdev->regs + DEMOSAIC_CONTROL);
		if(ap_start)      status |= 0x01;
		if(ap_done)       status |= 0x02;
		if(ap_idle)       status |= 0x04;
		if(ap_ready)      status |= 0x08;
		if(auto_restart)  status |= 0x80;
		iowrite32(status, pdev->regs + DEMOSAIC_CONTROL);
	}
}

/*
 * DEMOSAIC_GLOBAL_INTERRUPT_ENABLE
 * bit     name
 * 0       Global Interrupt Enable
 */
void demosaic_SetGIntr(struct demosaic_dev *pdev, int enable)
{
	u32 status = 0x0;
       	if(pdev != NULL)
       	{
	       if(enable) status |= 0x1;
	       iowrite32(status, pdev->regs + DEMOSAIC_GLOBAL_INTERRUPT_ENABLE);
	}	       
}

/*
 * DEMOSAIC_INTERRUPT_ENABLE
 * bit     name
 * 0       ap_done
 * 1       ap_ready
 */
void demosaic_SetIntr(struct demosaic_dev *pdev, int ap_done, int ap_ready)
{
	u32 status = 0x0;
	if(pdev != NULL)
	{
		if(ap_done)   status |= 0x1;
		if(ap_ready)  status |= 0x2;
		iowrite32(status, pdev->regs + DEMOSAIC_INTERRUPT_ENABLE);
	}
}

/*
 * DEMOSAIC_INTERRUPT_STATUS
 * bit     name
 * 0       ap_done
 * 1       ap_ready
 */
void demosaic_SetIntrStatus(struct demosaic_dev *pdev, int ap_done, int ap_ready)
{
        u32 status = 0x0;
        if(pdev != NULL)
        {
                if(ap_done)   status |= 0x1;
                if(ap_ready)  status |= 0x2;
                iowrite32(status, pdev->regs + DEMOSAIC_INTERRUPT_STATUS);
        }
}

/*
 * DEMOSAIC_WIDTH_SIZE
 */
void demosaic_SetHSize(struct demosaic_dev *pdev, u32 size)
{
	if(pdev != NULL)
	{
		iowrite32(size, pdev->regs + DEMOSAIC_WIDTH_SIZE);
	}
}

/*
 * DEMOSAIC_HEIGTH_SIZE
 */
void demosaic_SetVSize(struct demosaic_dev *pdev, u32 size)
{
        if(pdev != NULL)
        {
                iowrite32(size, pdev->regs + DEMOSAIC_HEIGTH_SIZE);
        }
}

/*
 * DEMOSAIC_BAYER_PHASE
 */
void demosaic_SetBayer(struct demosaic_dev *pdev, u32 bayer)
{
	if(pdev != NULL)
	{
		iowrite32(bayer, pdev->regs + DEMOSAIC_BAYER_PHASE);
	}
}

static int alice_demosaic_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct demosaic_dev *xdev;

	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if(!xdev)
		return -ENOMEM;

	printk("ALICE demosaic Start!\n");
	if (of_property_read_u32(node, "bayer", &xdev->bayer))
                xdev->bayer = eBGGR;
        printk("ALICE : bayer : %d\n", xdev->bayer );

	if (of_property_read_u32(node, "HSize", &xdev->HSize))
                xdev->HSize = 1920;
        printk("ALICE : HSize : %d\n", xdev->HSize );

	if (of_property_read_u32(node, "VSize", &xdev->VSize))
                xdev->VSize = 1080;
        printk("ALICE : VSize : %d\n", xdev->VSize );

	xdev->regs = devm_platform_ioremap_resource(pdev, 0);
        if (IS_ERR(xdev->regs)) {
                dev_err(&pdev->dev, "failed to ioremap memory resource\n");
                return PTR_ERR(xdev->regs);
        }
        printk("ALICE : REG : %X\n", (unsigned int)&xdev->regs);

	/* Setting */
	demosaic_SetCtrl(xdev, 0, 0, 0, 0, 0);
	demosaic_SetHSize(xdev, xdev->HSize);
	demosaic_SetVSize(xdev, xdev->VSize);
	demosaic_SetBayer(xdev, xdev->bayer);
	demosaic_SetIntr(xdev, 1, 1);
	demosaic_SetCtrl(xdev, 0, 0, 0, 0, 1);
	udelay(1000);
	demosaic_SetCtrl(xdev, 1, 0, 0, 0, 1);
	return 0;
}

static int alice_demosaic_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id alice_demosaic_of_match[] = {
	{ .compatible = "alice,demosaic", },
	{ /* end of list */ },
};

static struct platform_driver alice_demosaic_driver = {
	.driver = {
		.name = "alice-demosaic",
		.of_match_table = alice_demosaic_of_match,
	},
	.probe = alice_demosaic_probe,
	.remove = alice_demosaic_remove,
};
module_platform_driver(alice_demosaic_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice DEMOSAIC driver");
MODULE_LICENSE("GPL v2");
