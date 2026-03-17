LIMINE_VERSION := 9.5.1
LIMINE_TARBALL := limine-$(LIMINE_VERSION).tar.gz
LIMINE_DIR := limine-$(LIMINE_VERSION)
LIMINE_URL := https://github.com/limine-bootloader/limine/releases/download/v$(LIMINE_VERSION)/$(LIMINE_TARBALL)

build_limine:
	@if [ ! -f $(BOOTDIR)/$(LIMINE_TARBALL) ]; then \
		echo "Downloading LIMINE source..."; \
		wget -P $(BOOTDIR) $(LIMINE_URL); \
	fi

	@if [ ! -d $(BOOTDIR)/$(LIMINE_DIR) ]; then \
		echo "Extracting LIMINE..."; \
		tar -C $(BOOTDIR) -xf $(BOOTDIR)/$(LIMINE_TARBALL); \
	fi

	cd $(BOOTDIR)/$(LIMINE_DIR); ./bootstrap; ./configure --enable-uefi-x86-64; make;
	cp $(BOOTDIR)/$(LIMINE_DIR)/bin/BOOTX64.EFI $(BOOTDIR)