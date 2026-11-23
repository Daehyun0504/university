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

#define DSI_TX_CORE_CFG               0x00
#define DSI_TX_PROTOCOL_CFG           0x04
#define DSI_TX_GLO_INT_EN             0x20
#define DSI_TX_INT_ST                 0x24
#define DSI_TX_INT_EN                 0x28
#define DSI_TX_STATUS_REG             0x2C
#define DSI_TX_COMMAND_Q_PACKET       0x30
#define DSI_TX_DATA_FIFO_REG          0x34
#define DSI_TX_TIMING_REG_1           0x50
#define DSI_TX_TIMING_REG_2           0x54
#define DSI_TX_TIMING_REG_3           0x58
#define DSI_TX_TIMING_REG_4           0x5C
#define DSI_TX_LINE_TIME              0x60
#define DSI_TX_BLLP_TIME              0x64
#define DSI_TX_DPHY_LP_SH_OFFSET      0x68
#define DSI_TX_TIMING_REG_5           0x6C

struct AliceDsi {
	void __iomem *regs;
	u32 HSize;
	u32 VSize;
	u32 HBP;
	u32 HFP;
	u32 VSA;
	u32 VBP;
	u32 VFP;
};

static int alice_dsi_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
        struct AliceDsi *xdev;
	u32 status;

        printk("ALICE : alice dsi Probe \n");

        xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
        if(!xdev)
                return -ENOMEM;

	if (of_property_read_u32(node, "HSize", &xdev->HSize))
                xdev->HSize = 1920 * 3;
        printk("ALICE : HSize : %d\n", xdev->HSize );

	if (of_property_read_u32(node, "VSize", &xdev->VSize))
                xdev->VSize = 1080;
        printk("ALICE : VSize : %d\n", xdev->VSize );

	if (of_property_read_u32(node, "HBP", &xdev->HBP))
                xdev->HBP = 180 * 3;
        printk("ALICE : HBP : %d\n", xdev->HBP );

	if (of_property_read_u32(node, "HFP", &xdev->HFP))
                xdev->HFP = 100 * 3;
        printk("ALICE : HFP : %d\n", xdev->HFP );

	if (of_property_read_u32(node, "VSA", &xdev->VSA))
                xdev->VSA = 15;
        printk("ALICE : VSA : %d\n", xdev->VSA );

	if (of_property_read_u32(node, "VBP", &xdev->VBP))
                xdev->VBP = 15;
        printk("ALICE : VBP : %d\n", xdev->VBP );

        if (of_property_read_u32(node, "VFP", &xdev->VFP))
                xdev->VFP = 15;
        printk("ALICE : VFP : %d\n", xdev->VFP );

        xdev->regs = devm_platform_ioremap_resource(pdev, 0);
        if(IS_ERR(xdev->regs))
        {
                dev_err(&pdev->dev, "Failed to ioremap memory resource\n");
                return PTR_ERR(xdev->regs);
        }

	status = ioread32(xdev->regs + DSI_TX_PROTOCOL_CFG);
	status |= ((1 << 13) | (1 << 3));
	printk("ALICE DSI_TX_PROTOCOL_CFG : %04X\n", status);
        iowrite32(status, xdev->regs + DSI_TX_PROTOCOL_CFG);

	status = ((xdev->HSize << 16) & 0xFFFF0000) | (xdev->VSize & 0x0000FFFF);
	printk("ALICE DSI_TX_TIMING_REG_2 : %04X\n", status);
        iowrite32(status, xdev->regs + DSI_TX_TIMING_REG_2);

	status = ((xdev->HBP << 16) & 0xFFFF0000) | (xdev->HFP & 0x0000FFFF);
	printk("ALICE DSI_TX_TIMING_REG_3 : %04X\n", status);
	iowrite32(status, xdev->regs + DSI_TX_TIMING_REG_3);

	status = ((xdev->VSA << 16) & 0xFFFF0000) | 
		((xdev->VBP << 8) & 0x0000FF00) |
	       	(xdev->VFP & 0x000000FF); 
	printk("ALICE DSI_TX_TIMING_REG_4 : %04X\n", status);
	iowrite32(status, xdev->regs + DSI_TX_TIMING_REG_4);

	while((ioread32(xdev->regs + DSI_TX_CORE_CFG) & 0x04) != 0x04);

	status = ioread32(xdev->regs + DSI_TX_CORE_CFG);
	status |= 0x1;
	printk("ALICE DSI_TX_CORE_CFG : %04X\n", status);
	iowrite32(status, xdev->regs + DSI_TX_CORE_CFG);

	printk("ALICE DSI Init OK\n");
	
	return 0;
}

static int alice_dsi_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id alice_dsi_of_match[] = {
	{ .compatible = "alice,dsi", },
	{ /* end of list */ },
};

static struct platform_driver alice_dsi_driver = {
	.driver = {
		.name = "alice-dsi",
		.of_match_table = alice_dsi_of_match,
	},
	.probe = alice_dsi_probe,
	.remove = alice_dsi_remove,
};

module_platform_driver(alice_dsi_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice DSI driver");
MODULE_LICENSE("GPL v2");
