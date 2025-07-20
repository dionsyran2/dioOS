OSNAME = dioOS
OSDIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
DISK := $(OSDIR)/$(OSNAME).img
BOOTDIR := $(OSDIR)/bootloader
BOOTEFI := $(BOOTDIR)/BOOTX64.EFI
KERNELDIR := $(OSDIR)/kernel
BUILDDIR := $(KERNELDIR)/bin
DISKDIR := $(OSDIR)/disk
APPDIR := $(OSDIR)/applications
APPBINDIR := $(APPDIR)/bin
DISK_SIZE_MB=512
ESP_SIZE_MB=64

GRUB_VERSION := 2.06
GRUB_TARBALL := grub-$(GRUB_VERSION).tar.gz
GRUB_URL := https://ftp.gnu.org/gnu/grub/$(GRUB_TARBALL)
GRUB_DIR := grub-$(GRUB_VERSION)

.SILENT: all
all:
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
	sudo mkdir -p mnt/esp/boot/grub; \
	sudo cp $(BOOTEFI) mnt/esp/EFI/BOOT; \
	sudo cp $(OSDIR)/startup.nsh mnt/esp/ || echo "Missing startup.nsh"; \
	sudo cp $(OSDIR)/grub.cfg mnt/esp/boot/grub; \
	\
	echo "Installing OS files..."; \
	sudo mkdir -p mnt/ext2/boot; \
	sudo mkdir mnt/ext2/applications; \
	[ -f $(BUILDDIR)/kernel.elf ] && sudo cp $(BUILDDIR)/kernel.elf mnt/ext2/boot/ || echo "Missing kernel.elf"; \
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
	
	@if [ ! -f $(BOOTDIR)/$(GRUB_TARBALL) ]; then \
		echo "Downloading GRUB source..."; \
		wget -P $(BOOTDIR) $(GRUB_URL); \
	fi

	@if [ ! -d $(BOOTDIR)/$(GRUB_DIR) ]; then \
		echo "Extracting GRUB..."; \
		tar -C $(BOOTDIR) -xf $(BOOTDIR)/$(GRUB_TARBALL); \
	fi

	cd $(BOOTDIR)/$(GRUB_DIR); \
	./bootstrap; \
	mkdir -p build; \
	cd build; \
	../configure --target=x86_64 --with-platform=efi --disable-werror; \
	make -j$(nproc); \
	./grub-mkstandalone -O x86_64-efi --directory=./grub-core -o $(BOOTEFI) --modules="part_gpt part_msdos ext2 fat normal efi_gop efi_uga chain configfile linux" "boot/grub/grub.cfg=$(BOOTDIR)/grub.cfg"


clean:
	@$(MAKE) -C $(KERNELDIR) clean

cleanall:
	rm -rf $(BOOTDIR)/$(GRUB_DIR)
	rm -rf $(BOOTDIR)/$(GRUB_TARBALL)
	@$(MAKE) clean