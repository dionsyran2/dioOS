PACKAGES += build_busybox

BUSYBOX_GIT = https://github.com/mirror/busybox
BUSYBOX_PATH := $(TMPDIR)/busybox


build_busybox:
ifeq ($(CONFIG_BUSYBOX),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_BUSYBOX),y)
	$(MAKE) make_busybox
endif

else
	$(MAKE) make_busybox
endif
endif

make_busybox:
	@if [ ! -d $(BUSYBOX_PATH) ]; then \
		git clone $(BUSYBOX_GIT) $(BUSYBOX_PATH); \
	fi

	$(MAKE) -C $(BUSYBOX_PATH) defconfig
	
	# Disable MTD/Flash hardware utilities (these cause the mtd-user.h errors)
	sed -i 's/^CONFIG_NANDWRITE=y/# CONFIG_NANDWRITE is not set/' $(BUSYBOX_PATH)/.config
	sed -i 's/^CONFIG_NANDDUMP=y/# CONFIG_NANDDUMP is not set/' $(BUSYBOX_PATH)/.config
	sed -i 's/^CONFIG_FLASHCP=y/# CONFIG_FLASHCP is not set/' $(BUSYBOX_PATH)/.config
	sed -i 's/^CONFIG_FLASH_ERASEALL=y/# CONFIG_FLASH_ERASEALL is not set/' $(BUSYBOX_PATH)/.config
	sed -i 's/^CONFIG_FLASH_LOCK=y/# CONFIG_FLASH_LOCK is not set/' $(BUSYBOX_PATH)/.config
	sed -i 's/^CONFIG_FLASH_UNLOCK=y/# CONFIG_FLASH_UNLOCK is not set/' $(BUSYBOX_PATH)/.config
	
	# Disable UBI hardware tools (often grouped with Flash tools)
	sed -i 's/^\(CONFIG_UBI[A-Z_]*\)=y/# \1 is not set/' $(BUSYBOX_PATH)/.config
	
	# Disable the legacy traffic controller (this caused the TCA_CBQ_MAX error)
	sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' $(BUSYBOX_PATH)/.config
	
	sed -i 's|^CONFIG_EXTRA_CFLAGS=.*|CONFIG_EXTRA_CFLAGS="--sysroot=$(DISKDIR) -I$(DISKDIR)/usr/include"|' $(BUSYBOX_PATH)/.config
	
	$(MAKE) -C $(BUSYBOX_PATH) CC="$(CROSS_GCC) -I$(DISKDIR)/usr/include" \
		EXTRA_CFLAGS="--sysroot=$(DISKDIR)" \
		EXTRA_LDFLAGS="--sysroot=$(DISKDIR) -L$(DISKDIR)/lib/x86_64-dioOS-gnu \
		-L$(DISKDIR)/lib -L$(DISKDIR)/usr/lib -Wl,-rpath-link=$(DISKDIR)/lib/x86_64-dioOS-gnu" \
		-j$(NPROC); \
	$(MAKE) -C $(BUSYBOX_PATH) CONFIG_PREFIX=$(DISKDIR) install

	if [ ! -f $(DISKDIR)/usr/bin/bash ]; then \
		ln -s /bin/sh $(DISKDIR)/usr/bin/bash; \
	fi