# libXmu (X Miscellaneous Utilities)

LIB_PACKAGES += build_libxmu


LIBXMU_VERSION = libXmu-1.2.1
LIBXMU_URL = https://www.x.org/releases/individual/lib/$(LIBXMU_VERSION).tar.xz

build_libxmu: build_xorg
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXMU),y)
	$(MAKE) make_libxmu
endif

else
	$(MAKE) make_libxmu
endif
endif

make_libxmu:
	@echo "Building libXmu..."
	@if [ ! -f $(TMPDIR)/$(LIBXMU_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXMU_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXMU_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXMU_VERSION).tar.xz; fi
	cd $(TMPDIR)/$(LIBXMU_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --libdir=/lib/x86_64-dioOS-gnu --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(LIBXMU_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXMU_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install