PACKAGES += build_util

UTIL_LINUX_GIT = https://github.com/util-linux/util-linux.git
UTIL_LINUX_PATH := $(TMPDIR)/util-linux


build_util:
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_UTIL),y)
	$(MAKE) make_util
endif

else
	$(MAKE) make_util
endif

make_util:
	@if [ ! -d $(UTIL_LINUX_PATH) ]; then \
		git clone -b stable/v2.39 $(UTIL_LINUX_GIT) $(UTIL_LINUX_PATH); \
	fi
	cd $(UTIL_LINUX_PATH) && \
		./autogen.sh

	@mkdir -p $(UTIL_LINUX_PATH)/build
	cd $(UTIL_LINUX_PATH)/build && \
	../configure \
		--host=$(TARGET_ARCH) \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--with-sysroot=$(DISKDIR) \
		--disable-use-tty-group \
		--disable-makeinstall-chown \
		--disable-makeinstall-setuid \
		--disable-nls \
		--without-python \
		--without-systemd \
		--without-udev \
		--without-utmp \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
		PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -lcrypto"

	$(MAKE) -C $(UTIL_LINUX_PATH)/build -j$(NPROC)
	$(MAKE) -C $(UTIL_LINUX_PATH)/build DESTDIR=$(DISKDIR) install