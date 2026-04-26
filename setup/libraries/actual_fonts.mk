# These are the actual fonts!
LIB_PACKAGES += build_actual_fonts

XORG_FONT_URL = https://www.x.org/archive/individual/font

build_actual_fonts:
ifeq ($(CONFIG_X11),y)
ifeq ($(DISABLE_ALL),y)

ifeq ($(BUILD_ACTUAL_FONTS),y)
	$(MAKE) make_actual_fonts
endif

else
	$(MAKE) make_actual_fonts
endif
endif

make_actual_fonts: 
	$(MAKE) make_font_misc make_font_cursor_misc make_font_adobe_75dpi install_dejavu


#font-misc-misc
FONT_MISC_VERSION = font-misc-misc-1.1.3
FONT_MISC_ARCHIVE := $(FONT_MISC_VERSION).tar.xz
FONT_MISC_URL := $(XORG_FONT_URL)/$(FONT_MISC_ARCHIVE)

make_font_misc:
	@if [ ! -f $(TMPDIR)/$(FONT_MISC_ARCHIVE) ]; then wget -P $(TMPDIR) $(FONT_MISC_URL); fi
	@if [ ! -d $(TMPDIR)/$(FONT_MISC_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FONT_MISC_ARCHIVE); fi

	cd $(TMPDIR)/$(FONT_MISC_VERSION) && \
	./configure \
		--prefix=/usr \
		--with-fontrootdir=/usr/share/fonts/X11 \
		MAPFILES_PATH=$(DISKDIR)/usr/share/fonts/X11/util

	$(MAKE) -C $(TMPDIR)/$(FONT_MISC_VERSION) DESTDIR=$(DISKDIR) install.



FONT_CURSOR_MISC_VERSION = font-cursor-misc-1.0.4
FONT_CURSOR_MISC_ARCHIVE := $(FONT_CURSOR_MISC_VERSION).tar.xz
FONT_CURSOR_MISC_URL := $(XORG_FONT_URL)/$(FONT_CURSOR_MISC_ARCHIVE)

make_font_cursor_misc:
	@if [ ! -f $(TMPDIR)/$(FONT_CURSOR_MISC_ARCHIVE) ]; then wget -P $(TMPDIR) $(FONT_CURSOR_MISC_URL); fi
	@if [ ! -d $(TMPDIR)/$(FONT_CURSOR_MISC_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FONT_CURSOR_MISC_ARCHIVE); fi

	cd $(TMPDIR)/$(FONT_CURSOR_MISC_VERSION) && \
	./configure \
		--prefix=/usr \
		--with-fontrootdir=/usr/share/fonts/X11 \
		MAPFILES_PATH=$(DISKDIR)/usr/share/fonts/X11/util

	$(MAKE) -C $(TMPDIR)/$(FONT_CURSOR_MISC_VERSION) DESTDIR=$(DISKDIR) install


FONT_ADOBE_75dpi_VERSION = font-adobe-75dpi-1.0.4
FONT_ADOBE_75dpi_ARCHIVE := $(FONT_ADOBE_75dpi_VERSION).tar.xz
FONT_ADOBE_75dpi_URL := $(XORG_FONT_URL)/$(FONT_ADOBE_75dpi_ARCHIVE)

make_font_adobe_75dpi:
	@if [ ! -f $(TMPDIR)/$(FONT_ADOBE_75dpi_ARCHIVE) ]; then wget -P $(TMPDIR) $(FONT_ADOBE_75dpi_URL); fi
	@if [ ! -d $(TMPDIR)/$(FONT_ADOBE_75dpi_VERSION) ]; then tar -C $(TMPDIR) -xf $(TMPDIR)/$(FONT_ADOBE_75dpi_ARCHIVE); fi

	cd $(TMPDIR)/$(FONT_ADOBE_75dpi_VERSION) && \
	./configure \
		--prefix=/usr \
		--with-fontrootdir=/usr/share/fonts/X11 \
		MAPFILES_PATH=$(DISKDIR)/usr/share/fonts/X11/util

	$(MAKE) -C $(TMPDIR)/$(FONT_ADOBE_75dpi_VERSION) DESTDIR=$(DISKDIR) install


DEJAVU_URL = https://sourceforge.net/projects/dejavu/files/dejavu/2.37/dejavu-fonts-ttf-2.37.tar.bz2/download
install_dejavu:
	@echo "Installing DejaVu TrueType fonts..."
	@mkdir -p $(DISKDIR)/usr/share/fonts/truetype/dejavu
	@if [ ! -f $(TMPDIR)/dejavu.tar.bz2 ]; then \
		wget -O $(TMPDIR)/dejavu.tar.bz2 $(DEJAVU_URL); \
	fi
	tar -C $(DISKDIR)/usr/share/fonts/truetype/dejavu \
		--strip-components=2 \
		-xjf $(TMPDIR)/dejavu.tar.bz2 \
		"dejavu-fonts-ttf-2.37/ttf/"
	
	mkfontscale $(DISKDIR)/usr/share/fonts/truetype/dejavu
	mkfontdir $(DISKDIR)/usr/share/fonts/truetype/dejavu