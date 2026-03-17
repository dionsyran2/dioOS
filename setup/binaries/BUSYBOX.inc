BUSYBOX_GIT = https://github.com/mirror/busybox
BUSYBOX_PATH := $(TMPDIR)/busybox


build_busybox:
ifeq ($(CONFIG_BUSYBOX),y)
	@if [ ! -d $(BUSYBOX_PATH) ]; then \
		git clone $(BUSYBOX_GIT) $(BUSYBOX_PATH); \
	fi

	cd $(BUSYBOX_PATH); \
	make menuconfig; \
	$(MAKE) CC="$(CC) $(TARGET_FLAGS)" \
	LDFLAGS="$(LDFLAGS_FLAGS)" -j$(NPROC); \
	$(MAKE) CONFIG_PREFIX=$(DISKDIR) install

	ln -s /bin/sh $(DISKDIR)/usr/bin/bash
endif