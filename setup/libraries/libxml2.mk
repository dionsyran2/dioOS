LIB_PACKAGES += build_libxml2


LIBXML2_VERSION = libxml2-2.12.6
LIBXML2_TARBALL := $(LIBXML2_VERSION).tar.xz
LIBXML2_URL := https://download.gnome.org/sources/libxml2/2.12/$(LIBXML2_TARBALL)
LIBXML2_PATH := $(TMPDIR)/$(LIBXML2_VERSION)

build_libxml2: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXML2),y)
	$(MAKE) make_libxml2
endif

else
	$(MAKE) make_libxml2
endif

make_libxml2:
	@echo "Building libxml ..."
	@if [ ! -f $(TMPDIR)/$(LIBXML2_TARBALL) ]; then \
		wget -P $(TMPDIR) $(LIBXML2_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(LIBXML2_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXML2_TARBALL); \
	fi

	cd $(LIBXML2_PATH) && ./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		--disable-static \
		--without-python
	$(MAKE) -C $(LIBXML2_PATH) -j$(NPROC)
	$(MAKE) -C $(LIBXML2_PATH) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/share/pkgconfig install