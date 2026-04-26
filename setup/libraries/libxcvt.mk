LIB_PACKAGES += build_libxcvt

LIBXCVT_VERSION = libxcvt-0.1.2
LIBXCVT_URL = https://www.x.org/releases/individual/lib/$(LIBXCVT_VERSION).tar.xz


build_libxcvt: build_libx11
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_LIBXCVT),y)
	$(MAKE) make_libxcvt
endif

else
	$(MAKE) make_libxcvt
endif
endif

make_libxcvt:
	@echo "Building libxcvt..."
	@if [ ! -f $(TMPDIR)/$(LIBXCVT_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(LIBXCVT_URL); fi
	@if [ ! -d $(TMPDIR)/$(LIBXCVT_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(LIBXCVT_VERSION).tar.xz; fi
	
	@echo "[binaries]" > $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c = '$(CROSS_GCC)'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpp = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-g++'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "ar = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "strip = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-strip'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "pkgconfig = 'pkg-config'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[built-in options]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c_args = ['--sysroot=$(DISKDIR)', '-I$(DISKDIR)/usr/include']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpp_args = ['--sysroot=$(DISKDIR)', '-I$(DISKDIR)/usr/include']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "c_link_args = ['--sysroot=$(DISKDIR)', '-L$(DISKDIR)/lib/x86_64-dioOS-gnu', '-L$(DISKDIR)/usr/lib', '-lffi', '-Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu', '-Wl,-rpath-link,$(DISKDIR)/usr/lib']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpp_link_args = ['--sysroot=$(DISKDIR)', '-L$(DISKDIR)/lib/x86_64-dioOS-gnu', '-L$(DISKDIR)/usr/lib', '-lffi', '-Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu', '-Wl,-rpath-link,$(DISKDIR)/usr/lib']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[properties]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "sys_root = '$(DISKDIR)'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "pkg_config_libdir = ['$(DISKDIR)/usr/share/pkgconfig']" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "needs_exe_wrapper = true" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "[host_machine]" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "system = 'linux'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpu_family = 'x86_64'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "cpu = 'x86_64'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt
	@echo "endian = 'little'" >> $(TMPDIR)/$(LIBXCVT_VERSION)/cross_file.txt

	cd $(TMPDIR)/$(LIBXCVT_VERSION) && \
	PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
	meson setup build --cross-file cross_file.txt --prefix=/usr --libdir=lib/
	
	ninja -C $(TMPDIR)/$(LIBXCVT_VERSION)/build
	DESTDIR=$(DISKDIR) ninja -C $(TMPDIR)/$(LIBXCVT_VERSION)/build install

	
	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true