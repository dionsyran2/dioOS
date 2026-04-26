LIB_PACKAGES += build_fontconfig

FONTCONFIG_VERSION = fontconfig-2.14.2
FONTCONFIG_URL = https://www.freedesktop.org/software/fontconfig/release/$(FONTCONFIG_VERSION).tar.gz

build_fontconfig: build_xorg build_libexpat
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_FONTCONFIG),y)
	$(MAKE) make_fontconfig
endif

else
	$(MAKE) make_fontconfig
endif
endif

make_fontconfig:
	@if [ ! -f $(TMPDIR)/$(FONTCONFIG_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(FONTCONFIG_URL); fi
	@if [ ! -d $(TMPDIR)/$(FONTCONFIG_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FONTCONFIG_VERSION).tar.gz; fi

	cd $(TMPDIR)/$(FONTCONFIG_VERSION) && \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) \
	--disable-docs --disable-nls --libdir=/lib/x86_64-dioOS-gnu --with-pkgconfigdir=/usr/share/pkgconfig
	$(MAKE) -C $(TMPDIR)/$(FONTCONFIG_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install
