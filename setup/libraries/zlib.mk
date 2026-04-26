LIB_PACKAGES += build_zlib

ZLIB_VERSION = zlib-1.3.2
ZLIB_URL = https://zlib.net/$(ZLIB_VERSION).tar.gz

build_zlib: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_ZLIB),y)
	$(MAKE) make_zlib
endif

else
	$(MAKE) make_zlib
endif

make_zlib:
	@echo "Building zlib..."
	@if [ ! -f $(TMPDIR)/$(ZLIB_VERSION).tar.gz ]; then wget -P $(TMPDIR) $(ZLIB_URL); fi
	@if [ ! -d $(TMPDIR)/zlib ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(ZLIB_VERSION).tar.gz; fi
	
	cd $(TMPDIR)/$(ZLIB_VERSION) && \
	./configure \
		--prefix=/usr \
		--libdir=/lib/x86_64-dioOS-gnu \
		--with-pkgconfigdir=/usr/share/pkgconfig \
		--shared

	$(MAKE) -C $(TMPDIR)/$(ZLIB_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/$(ZLIB_VERSION) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/share/pkgconfig install