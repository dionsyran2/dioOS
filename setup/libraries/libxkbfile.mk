LIB_PACKAGES += build_libxkbfile

LIBXKBFILE_VERSION = libxkbfile-1.1.2
LIBXKBFILE_URL = https://www.x.org/archive/individual/lib/$(LIBXKBFILE_VERSION).tar.xz

build_libxkbfile: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXKBFILE),y)
	$(MAKE) make_libxkbfile
endif

else
	$(MAKE) make_libxkbfile
endif
endif

make_libxkbfile:
	@echo "Building libxkbfile..."
	@if [ ! -f $(TMPDIR)/$(LIBXKBFILE_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXKBFILE_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXKBFILE_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXKBFILE_VERSION).tar.xz; fi
	
	cd $(TMPDIR)/$(LIBXKBFILE_VERSION) && \
	PKG_CONFIG_PATH="$(DISKDIR)/usr/lib/pkgconfig:$(DISKDIR)/usr/share/pkgconfig:$(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig" \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
	CFLAGS="-O2 -I$(DISKDIR)/usr/include/X11/fonts" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib" \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--host=$(TARGET_ARCH) \
		--with-sysroot=$(DISKDIR) 
    
	$(MAKE) -C $(TMPDIR)/$(LIBXKBFILE_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LIBXKBFILE_VERSION) DESTDIR=$(DISKDIR) install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true