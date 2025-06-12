#ifndef SMI130_GYRO_H_
#define SMI130_GYRO_H_

extern const struct dev_pm_ops smi130_gyro_pm_ops;

int smi130_gyro_core_probe(struct device *dev, struct regmap *regmap, int irq,
		      const char *name);
void smi130_gyro_core_remove(struct device *dev);

#endif  /* SMI130_GYRO_H_ */
