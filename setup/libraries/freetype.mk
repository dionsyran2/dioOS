LIB_PACKAGES += build_freetype

FREETYPE_VERSION = freetype-2.13.2
FREETYPE_URL = https://download.savannah.gnu.org/releases/freetype/$(FREETYPE_VERSION).tar.xz

build_freetype: build_libx11 build_libpng
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_FREETYPE),y)
	$(MAKE) make_freetype
endif

else
	$(MAKE) make_freetype
endif
endif

make_freetype:
	@echo "Building freetype..."
	@if [ ! -f $(TMPDIR)/$(FREETYPE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(FREETYPE_URL); fi
	@if [ ! -d $(TMPDIR)/$(FREETYPE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FREETYPE_VERSION).tar.xz; fi
	
	sudo find $(DISKDIR) -name "*.la" -delete

	cd $(TMPDIR)/$(FREETYPE_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) \
		--with-brotli=no \
		--with-bzip2=no

	$(MAKE) -C $(TMPDIR)/$(FREETYPE_VERSION) V=1 -j$(NPROC)

	$(MAKE) -C $(TMPDIR)/$(FREETYPE_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true