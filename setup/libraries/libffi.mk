LIB_PACKAGES += build_libffi

LIBFFI_GIT = https://github.com/libffi/libffi.git
LIBFFI_PATH = $(TMPDIR)/libffi

build_libffi: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBFFI),y)
	$(MAKE) make_libffi
endif

else
	$(MAKE) make_libffi
endif

make_libffi:
	@echo "Building libffi..."
	@if [ ! -d $(LIBFFI_PATH) ]; then \
		git clone $(LIBFFI_GIT) $(LIBFFI_PATH); \
	fi

	cd $(LIBFFI_PATH) && \
	./autogen.sh && \
	./configure	\
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--with-gcc-arch=x86_64 \
	--with-pkgconfigdir=/usr/share/pkgconfig


	$(MAKE) -C $(LIBFFI_PATH)
	$(MAKE) -C $(LIBFFI_PATH) DESTDIR=$(DISKDIR) pkgconfigdir=/usr/share/pkgconfig install