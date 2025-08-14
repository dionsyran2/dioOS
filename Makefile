OSNAME = dioOS
OSDIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
DISK := $(OSDIR)/$(OSNAME).img
BOOTDIR := $(OSDIR)/bootloader
KERNELDIR := $(OSDIR)/kernel
BUILDDIR := $(KERNELDIR)/bin
DISKDIR := $(OSDIR)/disk
APPDIR := $(OSDIR)/applications
APPBINDIR := $(APPDIR)/bin
DISK_SIZE_MB=4096
ESP_SIZE_MB=64

LIMINE_VERSION := 9.5.1
LIMINE_TARBALL := limine-$(LIMINE_VERSION).tar.gz
LIMINE_DIR := limine-$(LIMINE_VERSION)
LIMINE_URL := https://github.com/limine-bootloader/limine/releases/download/v$(LIMINE_VERSION)/$(LIMINE_TARBALL)
BOOTEFI := $(BOOTDIR)/$(LIMINE_DIR)/bin/BOOTX64.EFI

KERNEL_FONT_FILE := $(DISKDIR)/fonts/vga16.psf

.SILENT: all
all:
	@echo "Compiling applications"
	$(MAKE) -C $(APPDIR)
	
	@echo "Compiling the kernel"
	$(MAKE) -C $(KERNELDIR)
	$(MAKE) buildimg

.SILENT: buildimg
buildimg:
	@echo "Creating disk image..."
	dd if=/dev/zero of=$(DISK) bs=1M count=$(DISK_SIZE_MB)

	@echo "Creating GPT partition table..."
	parted -s $(DISK) mklabel gpt
	parted -s $(DISK) mkpart ESP fat32 1MiB $(ESP_SIZE_MB)MiB
	parted -s $(DISK) set 1 esp on
	parted -s $(DISK) mkpart OS ext2 $(ESP_SIZE_MB)MiB 100%

	@echo "Mapping to loop device..."
	LOOP=$$(sudo losetup --show -Pf $(DISK)); \
	echo "Loop device: $$LOOP"; \
	sleep 1; \
	ESP_PART=$${LOOP}p1; \
	EXT_PART=$${LOOP}p2; \
	\
	echo "Formatting ESP (FAT32)..."; \
	sudo mkfs.vfat -F32 $$ESP_PART; \
	\
	echo "Formatting ext2 root partition..."; \
	sudo mkfs.ext2 -F $$EXT_PART; \
	\
	echo "Mounting partitions..."; \
	mkdir -p mnt/esp mnt/ext2; \
	sudo mount $$ESP_PART mnt/esp; \
	sudo mount $$EXT_PART mnt/ext2; \
	\
	echo "Installing EFI boot files..."; \
	sudo mkdir -p mnt/esp/EFI/BOOT; \
	sudo mkdir -p mnt/esp/boot; \
	sudo mkdir -p mnt/esp/boot/limine; \
	sudo cp $(BOOTEFI) mnt/esp/EFI/BOOT; \
	sudo cp $(OSDIR)/startup.nsh mnt/esp/ || echo "Missing startup.nsh"; \
	sudo cp $(OSDIR)/limine.conf mnt/esp/boot/limine; \
	[ -f $(BUILDDIR)/kernel.elf ] && sudo cp $(BUILDDIR)/kernel.elf mnt/esp/boot/ || echo "Missing kernel.elf"; \
	[ -f $(KERNEL_FONT_FILE) ] && sudo cp $(KERNEL_FONT_FILE) mnt/esp/ || echo "Missing kernel font file!"; \
	\
	echo "Installing OS files..."; \
	sudo mkdir mnt/ext2/applications; \
	[ -d $(DISKDIR) ] && sudo cp -r $(DISKDIR)/* mnt/ext2/ || echo "Missing $(DISKDIR)"; \
	[ -d $(APPBINDIR) ] && sudo cp -r $(APPBINDIR)/* mnt/ext2/applications/ || echo "Missing $(APPBINDIR)"; \
	\
	echo "Sync and cleanup..."; \
	sync; \
	sudo umount mnt/esp || echo "ESP not mounted"; \
	sudo umount mnt/ext2 || echo "EXT2 not mounted"; \
	sudo losetup -d $$LOOP; \
	rm -rf mnt/esp mnt/ext2 \
	@echo "Converting image to .vmdk"
	@qemu-img convert -f raw -O vmdk $(DISK) $(OSDIR)/$(OSNAME).vmdk
	@parted $(DISK) print

setup:
	@if [ ! -d $(BUILDDIR) ]; then \
		mkdir -p $(BUILDDIR); \
	fi

	@if [ ! -d $(BOOTDIR) ]; then \
		mkdir -p $(BOOTDIR); \
	fi
	
	@if [ ! -f $(BOOTDIR)/$(LIMINE_TARBALL) ]; then \
		echo "Downloading LIMINE source..."; \
		wget -P $(BOOTDIR) $(LIMINE_URL); \
	fi

	@if [ ! -d $(BOOTDIR)/$(LIMINE_DIR) ]; then \
		echo "Extracting LIMINE..."; \
		tar -C $(BOOTDIR) -xf $(BOOTDIR)/$(LIMINE_TARBALL); \
	fi
	cd $(BOOTDIR)/$(LIMINE_DIR); ./bootstrap; ./configure --enable-uefi-x86-64; make;
	


clean:
	@$(MAKE) -C $(KERNELDIR) clean

cleanall:
	rm -rf $(BOOTDIR)/$(LIMINE_DIR)
	rm -rf $(BOOTDIR)/$(LIMINE_TARBALL)
	@$(MAKE) clean
