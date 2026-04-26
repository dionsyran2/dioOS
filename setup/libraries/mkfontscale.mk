LIB_PACKAGES += build_mkfontscale


MKFONTSCALE_VERSION = 1.2.2
MKFONTSCALE_URL = https://www.x.org/pub/individual/app/mkfontscale-$(MKFONTSCALE_VERSION).tar.xz

build_mkfontscale:
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_MKFONTSCALE),y)
	$(MAKE) make_mkfontscale
endif

else
	$(MAKE) make_mkfontscale
endif
endif

make_mkfontscale:
	@if [ ! -f $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION).tar.xz ]; then \
		wget -P $(TMPDIR) $(MKFONTSCALE_URL); \
	fi

	@if [ ! -d $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION).tar.xz; \
	fi
	
	cd $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION) && \
	./configure --prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
		PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)"


	$(MAKE) -C $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/mkfontscale-$(MKFONTSCALE_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install