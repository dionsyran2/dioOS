LIB_PACKAGES += build_libjpeg

LIBJPEG_VERSION = 3.0.1
LIBJPEG_URL = https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$(LIBJPEG_VERSION).tar.gz

build_libjpeg: build_glibc
ifeq ($(DISABLE_ALL),y)
ifeq ($(BUILD_LIBJPEG),y)
	$(MAKE) make_libjpeg
endif
else
	$(MAKE) make_libjpeg
endif

make_libjpeg:
	@echo "Building libjpeg-turbo..."
	@if [ ! -f $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION).tar.gz ]; then \
		wget -O $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION).tar.gz $(LIBJPEG_URL); \
	fi
	@if [ ! -d $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION).tar.gz; \
	fi
	
	@mkdir -p $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION)/build
	cd $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION)/build && \
	cmake .. \
		-DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBDIR=/lib/x86_64-dioOS-gnu \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
        -DCMAKE_C_COMPILER="$(CROSS_GCC)" \
        -DCMAKE_SYSROOT=$(DISKDIR) \
        -DCMAKE_FIND_ROOT_PATH=$(DISKDIR) \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_C_FLAGS="--sysroot=$(DISKDIR) -I$(DISKDIR)/usr/include" \
        -DCMAKE_SHARED_LINKER_FLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu" \
        -DCMAKE_EXE_LINKER_FLAGS="-L$(DISKDIR)/lib/x86_64-dioOS-gnu -L$(DISKDIR)/usr/lib -Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu" \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DWITH_JPEG8=1 \
        -DCMAKE_INSTALL_DEFAULT_PKGCONFIGDIR=/usr/share/pkgconfig

	$(MAKE) -C $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION)/build -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/libjpeg-turbo-$(LIBJPEG_VERSION)/build DESTDIR=$(DISKDIR) install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true