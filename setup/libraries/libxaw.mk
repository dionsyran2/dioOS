LIB_PACKAGES += build_libxaw

LIBXAW_VERSION = libXaw-1.0.16
LIBXAW_URL = https://www.x.org/archive/individual/lib/$(LIBXAW_VERSION).tar.xz

build_libxaw: build_glibc build_libxt build_libxmu build_libxpm build_libxext
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXAW),y)
	$(MAKE) make_libxaw
endif

else
	$(MAKE) make_libxaw
endif
endif

make_libxaw:
	@echo "Building libXaw7..."
	@if [ ! -f $(TMPDIR)/$(LIBXAW_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXAW_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXAW_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXAW_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXAW_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		CFLAGS="-O2 -I$(DISKDIR)/usr/include" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu"

	$(MAKE) -C $(TMPDIR)/$(LIBXAW_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXAW_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install