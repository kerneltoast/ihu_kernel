// SPDX-License-Identifier: GPL-2.0
/*
 * MFD core driver for IDT PMIC P91E0A
 *
 * Driver based on MFD core driver for Intel Broxton Whiskey Cove PMIC
 * (intel_soc_pmic_bxtwc.c).
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/mfd/idt_p91e0a.h>
#include <asm/intel_pmc_ipc.h>

#define IDT_PMIC_DPS4                     0x23
#define IDT_PMIC_DPS4_DAMPB             BIT(6)
#define IDT_PMIC_DPS4_GM                BIT(5)
#define IDT_PMIC_DPS4_BW_MASK    GENMASK(4, 2)
#define IDT_PMIC_DPS4_BW(x)           (x << 2)

#define IDT_PMIC_DPS4_ACTIVE            (IDT_PMIC_DPS4_GM | IDT_PMIC_DPS4_BW(4))
#define IDT_PMIC_DPS4_SLEEP                                (IDT_PMIC_DPS4_BW(1))

#define IDT_PMIC_FPWM                     0xAD

#define IDT_PMIC_FPWM_ACTIVE            BIT(4)
#define IDT_PMIC_FPWM_SLEEP               0x00

static bool dcd4_wa;
module_param(dcd4_wa, bool, 0444);
MODULE_PARM_DESC(dcd4_wa, "Apply workaround for DCD4 shutdown issue");

static int regmap_ipc_byte_reg_read(void *context, unsigned int reg,
	unsigned int *val)
{
	int ret;
	u8 ipc_in[2];
	u8 ipc_out[4];
	struct idt_p91e0a *pmic = context;

	if (!pmic)
		return -EINVAL;

	ipc_in[0] = reg;
	ipc_in[1] = pmic->i2c_addr;
	ret = intel_pmc_ipc_command(PMC_IPC_PMIC_ACCESS,
		PMC_IPC_PMIC_ACCESS_READ,
		ipc_in, sizeof(ipc_in), (u32 *)ipc_out, 1);
	if (ret) {
		dev_err(pmic->dev, "Failed to read reg 0x%02X from PMIC at 0x%02hhX\n",
			reg, pmic->i2c_addr);
		return ret;
	}
	*val = ipc_out[0];

	return 0;
}

static int regmap_ipc_byte_reg_write(void *context, unsigned int reg,
	unsigned int val)
{
	int ret;
	u8 ipc_in[3];
	struct idt_p91e0a *pmic = context;

	if (!pmic)
		return -EINVAL;

	ipc_in[0] = reg;
	ipc_in[1] = pmic->i2c_addr;
	ipc_in[2] = val;
	ret = intel_pmc_ipc_command(PMC_IPC_PMIC_ACCESS,
		PMC_IPC_PMIC_ACCESS_WRITE,
		ipc_in, sizeof(ipc_in), NULL, 0);
	if (ret) {
		dev_err(pmic->dev, "Failed to write 0x%02X to reg 0x%02X of PMIC at 0x%02hhX\n",
			val, reg, pmic->i2c_addr);
		return ret;
	}

	return 0;
}

static const struct regmap_config idt_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.reg_write = regmap_ipc_byte_reg_write,
	.reg_read = regmap_ipc_byte_reg_read,
	.max_register = 0xFF,
	.cache_type = REGCACHE_NONE,
};

static int idt_probe(struct platform_device *pdev)
{
	int ret;
	acpi_handle handle;
	acpi_status status;
	unsigned long long i2c_addr;
	struct idt_p91e0a *pmic;

	handle = ACPI_HANDLE(&pdev->dev);
	status = acpi_evaluate_integer(handle, "_ADR", NULL, &i2c_addr);
	if (ACPI_FAILURE(status)) {
		dev_err(&pdev->dev, "Failed to get PMIC address\n");
		return -ENODEV;
	}

	if (i2c_addr > 0x7F) {
		dev_err(&pdev->dev, "Invalid PMIC address\n");
		return -EINVAL;
	}

	pmic = devm_kzalloc(&pdev->dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	platform_set_drvdata(pdev, pmic);

	pmic->dev = &pdev->dev;
	pmic->i2c_addr = (u8) i2c_addr;

	pmic->regmap = devm_regmap_init(&pdev->dev, NULL, pmic,
					&idt_regmap_config);
	if (IS_ERR(pmic->regmap)) {
		ret = PTR_ERR(pmic->regmap);
		dev_err(&pdev->dev, "Failed to initialise regmap: %d\n", ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int idt_suspend_dcd4_wa(struct idt_p91e0a *pmic)
{
	struct device *dev = pmic->dev;
	unsigned int fpwm;
	unsigned int dps4;
	int ret;

	/* set GM = 0 and BW = 1 */
	ret = regmap_write(pmic->regmap, IDT_PMIC_DPS4, IDT_PMIC_DPS4_SLEEP);
	if (ret)
		return ret;

	/* disable forced PWM mode */
	ret = regmap_write(pmic->regmap, IDT_PMIC_FPWM, IDT_PMIC_FPWM_SLEEP);
	if (ret)
		return ret;

	ret = regmap_read(pmic->regmap, IDT_PMIC_FPWM, &fpwm);
	if (ret)
		return ret;

	ret = regmap_read(pmic->regmap, IDT_PMIC_DPS4, &dps4);
	if (ret)
		return ret;

	if (fpwm != IDT_PMIC_FPWM_SLEEP || dps4 != IDT_PMIC_DPS4_SLEEP) {
		dev_err(dev, "PMIC: readback shows unexpected value after disabling FPWM mode (0x%02X: 0x%02X, 0x%02X: 0x%02X), expected (0x%02X, 0x%02X)\n",
			IDT_PMIC_FPWM, fpwm, IDT_PMIC_DPS4, dps4,
			IDT_PMIC_FPWM_SLEEP, IDT_PMIC_DPS4_SLEEP);

		return -EIO;
	}

	dev_info(dev, "PMIC: FPWM mode disabled (0x%02X: 0x%02X, 0x%02X: 0x%02X)\n",
		IDT_PMIC_FPWM, fpwm, IDT_PMIC_DPS4, dps4);

	return 0;
}

static int idt_suspend(struct device *dev)
{
	struct idt_p91e0a *pmic = dev_get_drvdata(dev);
	int ret = 0;

	if (dcd4_wa) {
		ret = idt_suspend_dcd4_wa(pmic);
		if (ret)
			dev_crit(dev, "Failed to switch PMIC to power saving mode. Aborting suspend\n");
	}

	return ret;
}

static int idt_resume_dcd4_wa(struct idt_p91e0a *pmic)
{
	struct device *dev = pmic->dev;
	unsigned int fpwm;
	unsigned int dps4;
	int ret;

	ret = regmap_read(pmic->regmap, IDT_PMIC_FPWM, &fpwm);
	if (ret)
		return ret;

	if (likely(fpwm == IDT_PMIC_FPWM_ACTIVE)) {
		dev_info(dev, "PMIC: FPWM mode already enabled\n");
		return 0;
	}

	/* enable forced PWM mode */
	ret = regmap_write(pmic->regmap, IDT_PMIC_FPWM, IDT_PMIC_FPWM_ACTIVE);
	if (ret)
		return ret;

	/* set GM = 1 and BW = 4 */
	ret = regmap_write(pmic->regmap, IDT_PMIC_DPS4, IDT_PMIC_DPS4_ACTIVE);
	if (ret)
		return ret;

	ret = regmap_read(pmic->regmap, IDT_PMIC_FPWM, &fpwm);
	if (ret)
		return ret;

	ret = regmap_read(pmic->regmap, IDT_PMIC_DPS4, &dps4);
	if (ret)
		return ret;

	if (fpwm != IDT_PMIC_FPWM_ACTIVE || dps4 != IDT_PMIC_DPS4_ACTIVE) {
		dev_err(dev, "PMIC: readback shows unexpected value after enabling FPWM mode (0x%02X: 0x%02X, 0x%02X: 0x%02X), expected (0x%02X, 0x%02X)\n",
			IDT_PMIC_FPWM, fpwm, IDT_PMIC_DPS4, dps4,
			IDT_PMIC_FPWM_ACTIVE, IDT_PMIC_DPS4_ACTIVE);

		return -EIO;
	}

	dev_info(dev, "PMIC: FPWM mode enabled (0x%02X: 0x%02X, 0x%02X: 0x%02X)\n",
		IDT_PMIC_FPWM, fpwm, IDT_PMIC_DPS4, dps4);

	return 0;
}

static int idt_resume(struct device *dev)
{
	struct idt_p91e0a *pmic = dev_get_drvdata(dev);

	if (dcd4_wa)
		idt_resume_dcd4_wa(pmic);

	return 0;
}
#endif

static const struct dev_pm_ops idt_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(idt_suspend, idt_resume)
};

static const struct acpi_device_id idt_p91e0a_acpi_ids[] = {
	{ "IDP91E0", },
	{ }
};
MODULE_DEVICE_TABLE(acpi, idt_p91e0a_acpi_ids);

static struct platform_driver idt_p91e0a_driver = {
	.probe = idt_probe,
	.driver = {
		.name = "IDT PMIC P91E0A",
		.pm = &idt_pm_ops,
		.acpi_match_table = ACPI_PTR(idt_p91e0a_acpi_ids),
	},
};

module_platform_driver(idt_p91e0a_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Oliver Barta <oliver.barta@aptiv.com>");
MODULE_DESCRIPTION("Driver for IDT PMIC P91E0A");
