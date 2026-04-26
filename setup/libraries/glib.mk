LIB_PACKAGES += build_glib

GLIB_VERSION =  glib-2.70.5
GLIB_TARBALL = $(GLIB_VERSION).tar.xz
GLIB_URL = https://download.gnome.org/sources/glib/2.70/$(GLIB_TARBALL)

build_glib: build_glibc
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_GLIB),y)
	$(MAKE) make_glib
endif

else
	$(MAKE) make_glib
endif

make_glib: build_libffi
	@echo "Building glib ..."
	@if [ ! -f $(TMPDIR)/$(GLIB_VERSION).tar.xz ]; then wget -P $(TMPDIR) $(GLIB_URL); fi
	@if [ ! -d $(TMPDIR)/$(GLIB_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(GLIB_TARBALL); fi
	
	@echo "[binaries]" > $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "c = '$(CROSS_GCC)'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "cpp = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-g++'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "ar = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-ar'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "strip = '$(TOOLCHAIN_DIR)/bin/$(TARGET_ARCH)-strip'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "pkgconfig = 'pkg-config'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "[built-in options]" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "c_args = ['--sysroot=$(DISKDIR)', '-I$(DISKDIR)/usr/include']" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "cpp_args = ['--sysroot=$(DISKDIR)', '-I$(DISKDIR)/usr/include']" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "c_link_args = ['--sysroot=$(DISKDIR)', '-L$(DISKDIR)/lib/x86_64-dioOS-gnu', '-L$(DISKDIR)/usr/lib', '-lffi', '-Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu', '-Wl,-rpath-link,$(DISKDIR)/usr/lib']" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "cpp_link_args = ['--sysroot=$(DISKDIR)', '-L$(DISKDIR)/lib/x86_64-dioOS-gnu', '-L$(DISKDIR)/usr/lib', '-lffi', '-Wl,-rpath-link,$(DISKDIR)/lib/x86_64-dioOS-gnu', '-Wl,-rpath-link,$(DISKDIR)/usr/lib']" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "[properties]" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "sys_root = '$(DISKDIR)'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "pkg_config_libdir = ['$(DISKDIR)/usr/share/pkgconfig']" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "needs_exe_wrapper = true" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "[host_machine]" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "system = 'linux'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "cpu_family = 'x86_64'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "cpu = 'x86_64'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	@echo "endian = 'little'" >> $(TMPDIR)/$(GLIB_VERSION)/cross_file.txt
	
	cd $(TMPDIR)/$(GLIB_VERSION) && \
    PKG_CONFIG_SYSROOT_DIR="$(DISKDIR)" \
    meson setup -Dtests=false -Diconv=auto --cross-file cross_file.txt --prefix=/usr build . && \
    ninja -C build && \
    DESTDIR=$(DISKDIR) ninja -C build install

	@mkdir -p $(DISKDIR)/usr/share/pkgconfig
	@find $(DISKDIR)/usr/lib/pkgconfig/ -name "*.pc" -exec mv -t $(DISKDIR)/usr/share/pkgconfig/ {} +
	@rmdir --ignore-fail-on-non-empty $(DISKDIR)/usr/lib/pkgconfig/ 2>/dev/null || true