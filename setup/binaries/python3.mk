PACKAGES += build_python3

PYTHON3_VERSION = 3.12.0
PYTHON3_TARBALL = Python-$(PYTHON3_VERSION).tar.xz
PYTHON3_URL = https://www.python.org/ftp/python/$(PYTHON3_VERSION)/$(PYTHON3_TARBALL)


build_python3:
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_PYTHON3),y)
	$(MAKE) make_python3
endif

else
	$(MAKE) make_python3
endif

make_python3: build_openssl
	@echo "--- Building Python $(PYTHON3_VERSION) ---"
	@if [ ! -f $(TMPDIR)/$(PYTHON3_TARBALL) ]; then wget -P $(TMPDIR) $(PYTHON3_URL); fi
	@if [ ! -d $(TMPDIR)/Python-$(PYTHON3_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(PYTHON3_TARBALL); fi

	cd $(TMPDIR)/Python-$(PYTHON3_VERSION) && \
	./configure \
		--host=$(TARGET_ARCH) \
		--build=x86_64-pc-linux-gnu \
		--prefix=/usr \
		--enable-shared \
		--with-build-python=python3 \
		--disable-ipv6 \
		--with-openssl=$(DISKDIR)/usr \
		--with-openssl-rpath=no \
		--with-system-ffi \
		ac_cv_file__dev_ptmx=yes \
		ac_cv_file__dev_ptc=no \
		ac_cv_have_long_long_format=yes \
		PKG_CONFIG_PATH="$(DISKDIR)/usr/share/pkgconfig"

	$(MAKE) -C $(TMPDIR)/Python-$(PYTHON3_VERSION) -j$(NPROC)
	$(MAKE) -C $(TMPDIR)/Python-$(PYTHON3_VERSION) DESTDIR=$(DISKDIR) install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/x86_64-dioOS-gnu/pkgconfig/ 2>/dev/null || true