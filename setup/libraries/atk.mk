LIB_PACKAGES += build_atk

ATK_VERSION = atk-2.20.0
ATK_TARBALL = $(ATK_VERSION).tar.xz
ATK_URL = https://download.gnome.org/sources/atk/2.20/$(ATK_TARBALL)

build_atk: build_glib
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_ATK),y)
	$(MAKE) make_atk
endif

else
	$(MAKE) make_atk
endif
endif

make_atk: 
	@echo "Building ATK..."
	@if [ ! -f $(TMPDIR)/$(ATK_TARBALL) ]; then wget -P $(TMPDIR) $(ATK_URL); fi
	@if [ ! -d $(TMPDIR)/$(ATK_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(ATK_TARBALL); fi

	find $(DISKDIR) -name "*.la" -delete

	cd $(TMPDIR)/$(ATK_VERSION) && \
	./configure --prefix=/usr \
		--host=$(TARGET_ARCH) \
		--build=x86_64-pc-linux-gnu \
		--with-sysroot=$(DISKDIR) \
		--disable-introspection \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
		PKG_CONFIG_LIBDIR="$(DISKDIR)/usr/share/pkgconfig" \
		CC="$(CROSS_GCC) --sysroot=$(DISKDIR)" \
		LDFLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu -Wl,-rpath-link,$(DISKDIR)/usr/lib"
		
	$(MAKE) -C $(TMPDIR)/$(ATK_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(ATK_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install
