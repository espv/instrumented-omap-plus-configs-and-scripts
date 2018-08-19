BUILD_ROOT := $(PWD)
BUILD_DIR := $(BUILD_ROOT)/build
KERNEL_DIR := $(PWD)/../omap
PUBLIC_DIR := $(PWD)/public

export HOST_ARCH=$(ARCH)
export CC=$(CC)

export ARCH := arm
export SUBARCH := arm
export CC := arm-eabi-
export CROSS_COMPILE := arm-eabi-
# Non-ccache path
export PATH := $(PWD)/public/arm-eabi-4.6/bin:$(PWD)/public/bootimg-tools/mkbootimg/:$(PATH)
# Example path for use with ccache
# export PATH := /usr/lib/ccache/bin:$(PWD)/bin:$(PWD)/public/arm-eabi-4.6/bin:$(PATH)

.PHONY: run $(BUILD_DIR)/kernel all public_sources

all: $(BUILD_DIR) $(PUBLIC_DIR) boot.img

$(PUBLIC_DIR):
	mkdir $(PUBLIC_DIR)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(PUBLIC_DIR)/yakju-jwr66y: $(PUBLIC_DIR)
	wget -O $(PUBLIC_DIR)/yakju-jwr66y-factory-09207065.tgz \
		https://dl.google.com/dl/android/aosp/yakju-jwr66y-factory-09207065.tgz
	tar -xvf $(PUBLIC_DIR)/yakju-jwr66y-factory-09207065.tgz -C $(PUBLIC_DIR)
	unzip -d $(PUBLIC_DIR)/yakju-jwr66y $(PUBLIC_DIR)/yakju-jwr66y/image-yakju-jwr66y.zip

$(PUBLIC_DIR)/bootimg-tools:
$(PUBLIC_DIR)/bootimg-tools: $(PUBLIC_DIR)
	git clone https://github.com/pbatard/bootimg-tools.git $@
	cd $(PUBLIC_DIR)/bootimg-tools; \
		CC="gcc" ARCH="" make mkbootimg/mkbootimg mkbootimg/unmkbootimg; \
		cd $(BUILD_ROOT);

$(PUBLIC_DIR)/arm-eabi-4.6: $(PUBLIC_DIR)
	git clone "https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6" $@

# omap:
# git clone --branch android-omap-tuna-3.0-jb-mr2 "https://android.googlesource.com/kernel/omap.git $@

public_sources: $(PUBLIC_DIR)/yakju-jwr66y \
	$(PUBLIC_DIR)/bootimg-tools \
	$(PUBLIC_DIR)/arm-eabi-4.6 \

boot.img: $(BUILD_DIR)/kernel $(BUILD_DIR)/ramdisk.cpio.gz \
	$(BUILD_DIR)/image_rebuild_cmd
	$(shell cat $(BUILD_DIR)/image_rebuild_cmd) --kernel $(BUILD_DIR)/kernel \
		--ramdisk $(BUILD_DIR)/ramdisk.cpio.gz -o $@

$(BUILD_DIR)/original.img: $(PUBLIC_DIR)/yakju-jwr66y/boot.img
	cp $^ $@

$(BUILD_DIR)/ramdisk.cpio.gz: $(BUILD_DIR)/original.img
	unmkbootimg --kernel /dev/null -i $^ | tail -1 | \
		sed 's/^ *//' | cut -d ' ' -f 1-13 > $(BUILD_DIR)/image_rebuild_cmd
	mv $(PWD)/ramdisk.cpio.gz $@

$(BUILD_DIR)/image_rebuild_cmd: $(BUILD_DIR)/ramdisk.cpio.gz
	# Depends on the rule above

$(BUILD_DIR)/kernel:
	cd $(KERNEL_DIR); \
		stat .config > /dev/null || make tuna_defconfig; \
		make -j8; cd $(BUILD_ROOT)
	cp $(KERNEL_DIR)/arch/arm/boot/zImage $@

run: boot.img
	adb reboot bootloader
	fastboot boot boot.img

# Do not replace ./build/ with $(BUILD_DIR)
clean:
	rm -rf ./build/ 
	rm -f boot.img

kclean: clean
	cd $(KERNEL_DIR); make clean;

mrproper:
	cd $(KERNEL_DIR); make mrproper;
