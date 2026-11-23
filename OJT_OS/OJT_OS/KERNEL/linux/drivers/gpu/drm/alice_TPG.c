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
#include <linux/miscdevice.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define TPG_CONTROL                       0x00
#define TPG_GLO_INT_EN                    0x04
#define TPG_INT_EN                        0x08
#define TPG_INT_STAUTS                    0x0C
#define TPG_HEIGHT                        0x10
#define TPG_WIDTH                         0x18
#define TPG_BACKGROUND_PATTERN_ID         0x20
#define TPG_OVERLAY_ID                    0x28
#define TPG_MASK_ID                       0x30
#define TPG_MOTION_SPEED                  0x38
#define TPG_COLOR_FORMAT                  0x40
#define TPG_CROSS_HAIR_HORIZONTAL         0x48
#define TPG_CROSS_HAIR_VERTICAL           0x50
#define TPG_ZPLATE_H_STARTING_POINT       0x58
#define TPG_ZPLATE_H_DELTA                0x60
#define TPG_ZPLATE_V_STARTING_POINT       0x68
#define TPG_ZPLATE_V_DELTA                0x70
#define TPG_BOX_SIZE                      0x78
#define TPG_BOX_R_AND_Y                   0x80
#define TPG_BOX_G_AND_U                   0x88
#define TPG_BOX_B_AND_V                   0x90
#define TPG_ENABLE_INPUT                  0x98
#define TPG_PASS_THROUGH_L_BOUNDARY       0xA0
#define TPG_PASS_THROUGH_R_BOUNDARY       0xA8
#define TPG_PASS_THROUGH_UPPER_BOUNDARY   0xB0
#define TPG_PASS_THROUGH_LOWER_BOUNDARY   0xB8
#define TPG_DISPLAY_PORT_DYN_CHANGE       0xC0
#define TPG_DISPLAY_PORT_YUV_COE          0xC8
#define TPG_FIELD_ID                      0xD0
#define TPG_BACK_MOTION_EN                0xD8

#define ALICE_TPG_DEVICE_NAME             "vTPG"
#define ALICE_MAJOR                       240

/* Dev Funcs  */

static int alice_setTpg(u32 mode, u32 format);

int alice_tpg_device_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int alice_tpg_device_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t alice_tpg_device_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	u32 mode = 0;
	unsigned long format = 0;
	char kbuf[16] = {0};
	if (count > 0) {
		if (count > 15) count = 15;
		if (copy_from_user(kbuf, buf, count))
			return -EFAULT;
		printk("ALICE : TPG %s\n", kbuf);
		switch (kbuf[0] - '0') {
			case 0:
				mode = 1;
				break;
			default:
				mode = 0;
				if (kstrtol(kbuf, 0, &format))
					return -EINVAL;
				format -= 1;
				break;
		}
		printk("ALICE : TPG [%d:%ld]\n", mode, format);
		alice_setTpg(mode, (u32)format);
	}
	return count;
}

ssize_t alice_tpg_device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	return 0;
}

static struct file_operations vd_fops = {
	.read = alice_tpg_device_read,
	.write = alice_tpg_device_write,
	.open = alice_tpg_device_open,
	.release = alice_tpg_device_release
};

// miscdevice 구조체 선언
static struct miscdevice alice_tpg_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "vTPG",
	.fops = &vd_fops,
};

struct AliceTPG {
	void __iomem *regs;
	u32 VSize;
	u32 HSize;
};

struct AliceTPG * _tmpTPG;

static int alice_set_TPGColor(u32 format)
{
	iowrite32(format, _tmpTPG->regs + TPG_BACKGROUND_PATTERN_ID);
	return 0;
}

/*
 * 0 : TPG
 * 1 : INPUT Data
 */
static int alice_set_EnableInput(u32 mode)
{
	iowrite32(mode, _tmpTPG->regs + TPG_ENABLE_INPUT);
        return 0;
}

static int alice_setTpg(u32 mode, u32 format)
{
	 /* TPG Disable Auto Restart */
        iowrite32(0x0, _tmpTPG->regs + TPG_CONTROL);

        /* TPG Height set */
        iowrite32(_tmpTPG->VSize, _tmpTPG->regs + TPG_HEIGHT);
        /* TPG Width set */
        iowrite32(_tmpTPG->HSize, _tmpTPG->regs + TPG_WIDTH);

        iowrite32(_tmpTPG->HSize, _tmpTPG->regs + TPG_PASS_THROUGH_UPPER_BOUNDARY);
        iowrite32(_tmpTPG->VSize, _tmpTPG->regs + TPG_PASS_THROUGH_LOWER_BOUNDARY);
        /* iowrite32(0, _tmpTPG->regs + TPG_BACKGROUND_PATTERN_ID); */

        alice_set_TPGColor(format);
        /* iowrite32(1, xdev->regs + TPG_ENABLE_INPUT); */
        alice_set_EnableInput(mode);
        iowrite32(0, _tmpTPG->regs + TPG_PASS_THROUGH_L_BOUNDARY);
        iowrite32(0, _tmpTPG->regs + TPG_PASS_THROUGH_R_BOUNDARY);
        iowrite32(0x81, _tmpTPG->regs + TPG_CONTROL);
	return 0;
}
/* EXPORT_SYMBOL(alice_setTpg); */ 

static int alice_tpg_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct AliceTPG *xdev;
	int ret;

	printk("ALICE : alice TPG Probe \n");

	xdev = devm_kzalloc(&pdev->dev, sizeof(*xdev), GFP_KERNEL);
	if(!xdev)
		return -ENOMEM;

	if (of_property_read_u32(node, "VSize", &xdev->VSize))
		xdev->VSize = 1080;
	printk("ALICE : VSize : %d\n", xdev->VSize );

	if (of_property_read_u32(node, "HSize", &xdev->HSize))
		xdev->HSize = 1920;
	printk("ALICE : HSize : %d\n", xdev->HSize );

	xdev->regs = devm_platform_ioremap_resource(pdev, 0);
	if(IS_ERR(xdev->regs)) {
		dev_err(&pdev->dev, "Failed to ioremap memory resource\n");
		return PTR_ERR(xdev->regs);
	}

	_tmpTPG = xdev;

	// 문자 디바이스 등록
	ret = misc_register(&alice_tpg_miscdev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register misc device\n");
		return ret;
	}

	/* TPG Disable Auto Restart */
	iowrite32(0x0, xdev->regs + TPG_CONTROL);
	/* TPG Height set */
	iowrite32(xdev->VSize, xdev->regs + TPG_HEIGHT);
	/* TPG Width set */
	iowrite32(xdev->HSize, xdev->regs + TPG_WIDTH);
	iowrite32(xdev->HSize, xdev->regs + TPG_PASS_THROUGH_UPPER_BOUNDARY);
	iowrite32(xdev->VSize, xdev->regs + TPG_PASS_THROUGH_LOWER_BOUNDARY);
	alice_set_TPGColor(0);
	alice_set_EnableInput(1);
	iowrite32(0, xdev->regs + TPG_PASS_THROUGH_L_BOUNDARY);
	iowrite32(0, xdev->regs + TPG_PASS_THROUGH_R_BOUNDARY);
	iowrite32(0x81, xdev->regs + TPG_CONTROL);

	return 0;
}

static int alice_tpg_remove(struct platform_device *pdev)
{
	misc_deregister(&alice_tpg_miscdev);
	return 0;
}

static const struct of_device_id alice_tpg_of_match[] = {
	{ .compatible = "alice,tpg", },
	{ /* end of list */ },
};

static struct platform_driver alice_tpg_driver = {
	.driver = {
		.name = "alice-tpg",
		.of_match_table = alice_tpg_of_match,
	},
	.probe = alice_tpg_probe,
	.remove = alice_tpg_remove,
};

module_platform_driver(alice_tpg_driver);

MODULE_AUTHOR("Alice, Inc.");
MODULE_DESCRIPTION("Alice TPG driver");
MODULE_LICENSE("GPL v2");
