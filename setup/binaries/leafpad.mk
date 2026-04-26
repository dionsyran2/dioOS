PACKAGES += build_leafpad

LEAFPAD_VERSION =  leafpad-0.8.17
LEAFPAD_TARBALL = $(LEAFPAD_VERSION).tar.gz
LEAFPAD_URL = https://download-mirror.savannah.gnu.org/releases/leafpad/$(LEAFPAD_TARBALL)

build_leafpad:
ifeq ($(CONFIG_LEAFPAD),y)

ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LEAFPAD),y)
	$(MAKE) make_leafpad
endif

else
	$(MAKE) make_leafpad
endif
endif

make_leafpad:
	@echo "Building leafpad..."
	@if [ ! -f $(TMPDIR)/$(LEAFPAD_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LEAFPAD_URL); fi
	@if [ ! -d $(TMPDIR)/$(LEAFPAD_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LEAFPAD_TARBALL); fi
	cd $(TMPDIR)/$(LEAFPAD_VERSION) && \
	./configure --prefix=/usr --host=$(TARGET_ARCH) --with-sysroot=$(DISKDIR) --disable-fribidi \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
	CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
	CFLAGS="-O2 -fpermissive -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -DGLIB_DISABLE_DEPRECATION_WARNINGS" \
	LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib"

	$(MAKE) -C $(TMPDIR)/$(LEAFPAD_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(LEAFPAD_VERSION) DESTDIR=$(DISKDIR) install