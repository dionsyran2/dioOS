LIB_PACKAGES += build_util_macros

UTIL_MACROS_VERSION = util-macros-1.20.0
UTIL_MACROS_URL = https://www.x.org/archive/individual/util/$(UTIL_MACROS_VERSION).tar.xz

build_util_macros:
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_UTIL_MACROS),y)
	$(MAKE) make_util_macros
endif

else
	$(MAKE) make_util_macros
endif
endif

make_util_macros:
	@echo "Building util-macros..."
	@if [ ! -f $(TMPDIR)/$(UTIL_MACROS_VERSION).tar.xz ]; then \
		wget -P $(TMPDIR) $(UTIL_MACROS_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(UTIL_MACROS_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(UTIL_MACROS_VERSION).tar.xz; \
	fi

	cd $(TMPDIR)/$(UTIL_MACROS_VERSION) && \
	./configure \
		--prefix=/usr \
		--host=$(TARGET_ARCH) \
		--with-pkgconfigdir=/usr/share/pkgconfig


	$(MAKE) -C $(TMPDIR)/$(UTIL_MACROS_VERSION)
	$(MAKE) -C $(TMPDIR)/$(UTIL_MACROS_VERSION) pkgconfigdir=/usr/share/pkgconfig DESTDIR=$(DISKDIR) install