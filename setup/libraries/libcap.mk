LIB_PACKAGES += build_libcap

LIBCAP_VERSION = libcap-2.71
LIBCAP_TARBALL = $(LIBCAP_VERSION).tar.xz
LIBCAP_URL = https://www.kernel.org/pub/linux/libs/security/linux-privs/libcap2/$(LIBCAP_TARBALL)


build_libcap: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBCAP),y)
	$(MAKE) make_libcap
endif

else
	$(MAKE) make_libcap
endif

make_libcap:
	@echo "Building libcap..."
	@if [ ! -f $(TMPDIR)/$(LIBCAP_TARBALL) ]; then \
		wget -P $(TMPDIR) $(LIBCAP_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(LIBCAP_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBCAP_TARBALL); \
	fi

	$(MAKE) -C $(TMPDIR)/$(LIBCAP_VERSION) \
		AR="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar" \
		RANLIB="$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ranlib" \
		prefix=/usr \
		lib=/lib/ \
		SBINDIR=/usr/sbin \
		PAM_CAP=no \
		GOLANG=no \
		-j$(NPROC)

	$(MAKE) -C $(TMPDIR)/$(LIBCAP_VERSION) \
		prefix=/usr \
		lib=/lib/ \
		SBINDIR=/usr/sbin \
		DESTDIR=$(DISKDIR) \
		PKGCONFIGDIR=/usr/share/pkgconfig \
		install