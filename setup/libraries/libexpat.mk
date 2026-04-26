LIB_PACKAGES += build_libexpat

LIBEXPAT_GIT = https://github.com/libexpat/libexpat.git
LIBEXPAT_PATH = $(TMPDIR)/libexpat

build_libexpat: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_EXPAT),y)
	$(MAKE) make_libexpat
endif

else
	$(MAKE) make_libexpat
endif

make_libexpat:
	@echo "Building libexpat..."
	@if [ ! -d $(LIBEXPAT_PATH) ]; then \
		git clone $(LIBEXPAT_GIT) $(LIBEXPAT_PATH); \
	fi

	cd $(LIBEXPAT_PATH)/expat && \
	./buildconf.sh && \
	./configure \
	--prefix=/usr \
	--libdir=/lib \
	--with-pkgconfigdir=/usr/share/pkgconfig \
	--without-docbook
	CPPFLAGS=-DXML_LARGE_SIZE

	$(MAKE) -C $(LIBEXPAT_PATH)/expat
	$(MAKE) -C $(LIBEXPAT_PATH)/expat DESTDIR=$(DISKDIR) pkgconfigdir=/usr/share/pkgconfig install