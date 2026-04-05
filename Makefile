OSNAME = dioOS
OSDIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
DISK := $(OSDIR)/$(OSNAME).img
BOOTDIR := $(OSDIR)/bootloader
KERNELDIR := $(OSDIR)/kernel
BUILDDIR := $(KERNELDIR)/bin
SETUPDIR := $(OSDIR)/setup
DISKDIR := $(OSDIR)/disk
TMPDIR := $(OSDIR)/setup/tmp
DISK_SIZE_MB=2048
ESP_SIZE_MB=64


BOOTEFI := $(BOOTDIR)/BOOTX64.EFI
KERNEL_FONT_FILE := $(DISKDIR)/fonts/vga16.psf


.SILENT: all
all:
	@echo "Compiling the kernel"
	$(MAKE) -C $(KERNELDIR)
	$(MAKE) buildimg

include setup/Makefile.inc

.SILENT: buildimg
buildimg:
	@printf "\e[1;33mCreating disk image...\e[0m\n"
	@dd if=/dev/zero of=$(DISK) bs=1M count=$(DISK_SIZE_MB)

	@printf "\e[1;33mCreating GPT partition table...\e[0m\n"
	@parted -s $(DISK) mklabel gpt
	@parted -s $(DISK) mkpart ESP fat32 1MB $(ESP_SIZE_MB)MB
	@parted -s $(DISK) set 1 esp on
	@parted -s $(DISK) mkpart OS ext2 $(ESP_SIZE_MB)MB 100%

	@echo "Mapping to loop device..."
	@LOOP=$$(sudo losetup --show -Pf $(DISK)); \
	printf "\e[1;33mLoop device: $$LOOP\e[0m\n"; \
	sleep 1; \
	ESP_PART=$${LOOP}p1; \
	EXT_PART=$${LOOP}p2; \
	\
	printf "\e[1;33mFormatting ESP (FAT32)...\e[0m\n"; \
	sudo mkfs.vfat -F32 $$ESP_PART; \
	\
	printf "\e[1;33mFormatting ext2 root partition...\e[0m\n"; \
	sudo mkfs.ext2 -F $$EXT_PART; \
	\
	printf "\e[1;33mMounting partitions...\e[0m\n"; \
	mkdir -p mnt/esp mnt/ext2; \
	sudo mount $$ESP_PART mnt/esp; \
	sudo mount $$EXT_PART mnt/ext2; \
	\
	printf "\e[1;33mInstalling EFI boot files...\e[0m\n"; \
	sudo mkdir -p mnt/esp/EFI/BOOT; \
	sudo mkdir -p mnt/esp/boot; \
	sudo mkdir -p mnt/esp/boot/limine; \
	sudo cp $(BOOTEFI) mnt/esp/EFI/BOOT; \
	sudo cp $(OSDIR)/startup.nsh mnt/esp/ || echo "Missing startup.nsh"; \
	sudo cp $(OSDIR)/limine.conf mnt/esp/boot/limine; \
	[ -f $(BUILDDIR)/kernel.elf ] && sudo cp $(BUILDDIR)/kernel.elf mnt/esp/boot/ || echo "Missing kernel.elf"; \
	[ -f $(KERNEL_FONT_FILE) ] && sudo cp $(KERNEL_FONT_FILE) mnt/esp/ || echo "Missing kernel font file!"; \
	\
	printf "\e[1;33mInstalling OS files...\e[0m\n"; \
	[ -d $(DISKDIR) ] && sudo cp -r $(DISKDIR)/* mnt/ext2/ || echo "Missing $(DISKDIR)"; \
	\
	printf "\e[1;33mSync and cleanup...\e[0m\n"; \
	sync; \
	sudo umount mnt/esp || echo "ESP not mounted"; \
	sudo umount mnt/ext2 || echo "EXT2 not mounted"; \
	sudo losetup -d $$LOOP; \
	sudo rm -rf mnt/esp mnt/ext2;

	@printf "\e[1;33mConverting image to .vmdk\e[0m\n"
	@qemu-img convert -f raw -O vmdk $(DISK) $(OSDIR)/$(OSNAME).vmdk
	@parted $(DISK) print


clean:
	@$(MAKE) -C $(KERNELDIR) clean

cleanall:
	rm -rf $(BOOTDIR)/$(LIMINE_DIR)
	rm -rf $(BOOTDIR)/$(LIMINE_TARBALL)
	@$(MAKE) clean

run:
	qemu-system-x86_64 \
	-machine q35 \
	-m 4G \
	-smp cores=1 \
	-cpu max \
	-drive file=$(DISK) \
	-drive if=pflash,format=raw,unit=0,file="/OVMFbin/OVMF_CODE-pure-efi.fd",readonly=on \
	-drive if=pflash,format=raw,unit=1,file="/OVMFbin/OVMF_VARS-pure-efi.fd" \
	-device intel-hda,debug=0 \
	-device hda-output,audiodev=snd0 \
	-audiodev dsound,id=snd0 \
	-device qemu-xhci,id=xhci \
	-nic user,model=e1000e \
	-monitor stdio \
	-trace usb_xhci_ \
	-d guest_errors \
	--no-reboot \
	--no-shutdown
