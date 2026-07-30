MODULE_NAME := edge_camera

# Define build directory
BUILD_DIR := $(PWD)/build

obj-m := $(MODULE_NAME).o

.PHONY: all clean

all:
	@mkdir -p $(BUILD_DIR)
	@cp $(PWD)/$(MODULE_NAME).c $(BUILD_DIR)/
	make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) src=$(PWD) modules
	@cp $(BUILD_DIR)/$(MODULE_NAME).ko $(PWD)/

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)
	rm -f $(PWD)/$(MODULE_NAME).ko
