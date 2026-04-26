LIB_PACKAGES += build_openssl

OPENSSL_GIT = https://github.com/openssl/openssl.git
OPENSSL_PATH = $(TMPDIR)/openssl

build_openssl: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_OPENSSL),y)
	$(MAKE) make_openssl
endif

else
	$(MAKE) make_openssl
endif

make_openssl:
	@echo "Building openssl..."
	@if [ ! -d $(OPENSSL_PATH) ]; then \
		git clone -b openssl-3.2 $(OPENSSL_GIT) $(OPENSSL_PATH); \
	fi

	cd $(OPENSSL_PATH) && \
	./Configure \
		linux-x86_64 \
		--prefix=/ \
		--libdir=lib/x86_64-dioOS-gnu \
		--openssldir=/etc/ssl \
		shared \
		zlib \
		--with-zlib-include=$(DISKDIR)/usr/include \
		--with-zlib-lib=$(DISKDIR)/usr/lib \
		--sysroot=$(DISKDIR) \
		-DOPENSSL_NO_CAPIENG

	$(MAKE) -C $(OPENSSL_PATH) -j$(NPROC)
	
	$(MAKE) -C $(OPENSSL_PATH) DESTDIR=$(DISKDIR) pcdir=/usr/share/pkgconfig install_sw

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true