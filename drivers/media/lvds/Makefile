ds90u-modules-$(CONFIG_SERDES_TIDS90U_I2C) += ds90u-i2c-adapter.o
ds90u-modules-$(CONFIG_SERDES_TIDS90U_GPIO) += ds90u-gpio.o
ds90u-modules-$(CONFIG_SERDES_TIDS90U_LUT) += ds90u-wbal.o
ds90u-objs := ds90u-core.o ds90u-i2c.o ds90u-init.o $(ds90u-modules-y)
obj-$(CONFIG_SERDES_TIDS90U) += ds90u.o ds90u-gpio-seq.o

ccflags-$(CONFIG_SERDES_TIDS90U_DEBUG) := -DDEBUG

