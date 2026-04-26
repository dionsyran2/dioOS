
LIB_PACKAGES += build_libxdmcp


LIBXDMCP_VERSION = libXdmcp-1.1.4
LIBXDMCP_URL = https://www.x.org/archive/individual/lib/$(LIBXDMCP_VERSION).tar.xz

build_libxdmcp: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXDMCP),y)
	$(MAKE) make_libxdmcp
endif

else
	$(MAKE) make_libxdmcp
endif
endif

make_libxdmcp:
	@echo "Building libXdmcp..."
	@if [ ! -f $(TMPDIR)/$(LIBXDMCP_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXDMCP_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXDMCP_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXDMCP_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXDMCP_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--disable-docs \
		--with-pkgconfigdir=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/$(LIBXDMCP_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXDMCP_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install