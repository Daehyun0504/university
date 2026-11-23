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

#define CSI_RX_CORE_CFG                      0x00
#define CSI_RX_PROTOCOL_CFG                  0x04
#define CSI_RX_CORE_STAUTS                   0x10
#define CSI_RX_GLO_INT_EN                    0x20
#define CSI_RX_INT_ST                        0x24
#define CSI_RX_INT_EN                        0x28
#define CSI_RX_VCX_SELECT                    0x2C
#define CSI_RX_GEN_SH_PKT                    0x30
#define CSI_RX_LANE_0_INFO                   0x40
#define CSI_RX_LANE_1_INFO                   0x44
#define CSI_RX_LANE_2_INFO                   0x48
#define CSI_RX_LANE_3_INFO                   0x4C

struct AliceCsi2 {
	void __iomem *regs;
	u32 lanes;
};

static int alice_csi2_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct AliceCsi2 *xdev;
	u32 status;

	printk("ALICE : alice Csi2 Probe \n");

	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if(!xdev) 
		return -ENOMEM;

	if (of_property_read_u32(node, "lanes", &xdev->lanes))
                xdev->lanes = 2;
        printk("ALICE : LANES : %d\n", xdev->lanes );

	xdev->regs = devm_platform_ioremap_resource(pdev, 0);
	if(IS_ERR(xdev->regs))
	{
		dev_err(&pdev->dev, "Failed to ioremap memory resource\n");
		return PTR_ERR(xdev->regs);
	}

	iowrite32(0x2, xdev->regs + CSI_RX_CORE_CFG);
	iowrite32(xdev->lanes - 1, xdev->regs + CSI_RX_PROTOCOL_CFG);
	udelay(1000);
	iowrite32(0x1, xdev->regs + CSI_RX_CORE_CFG);

	return 0;
}

static int alice_csi2_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id alice_csi2_of_match[] = {
	{ .compatible = "alice,csi2", },
	{ /* end of list */ },
};

static struct platform_driver alice_csi2_driver = {
	.driver = {
		.name = "alice-csi2",
		.of_match_table = alice_csi2_of_match,
	},
	.probe = alice_csi2_probe,
	.remove = alice_csi2_remove,
};

module_platform_driver(alice_csi2_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice CSI2 driver");
MODULE_LICENSE("GPL v2");
