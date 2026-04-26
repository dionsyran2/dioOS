LIB_PACKAGES += build_libpciaccess

LIBPCIACCESS_VERSION = libpciaccess-0.17
LIBPCIACCESS_URL = https://www.x.org/releases/individual/lib/$(LIBPCIACCESS_VERSION).tar.xz

build_libpciaccess:
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBPCIACCESS),y)
	$(MAKE) make_libpciaccess
endif

else
	$(MAKE) make_libpciaccess
endif

make_libpciaccess:
	@echo "Building libpciaccess..."
	@if [ ! -f $(TMPDIR)/$(LIBPCIACCESS_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBPCIACCESS_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBPCIACCESS_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBPCIACCESS_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBPCIACCESS_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBPCIACCESS_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBPCIACCESS_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install