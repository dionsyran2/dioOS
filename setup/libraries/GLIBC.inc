GLIBC_VERSION = glibc-2.41
GLIBC_TARBALL := $(GLIBC_VERSION).tar.xz
GLIBC_URL := $(GNU_MIRROR)/gnu/glibc/$(GLIBC_TARBALL)

build_glibc:
	@echo "Building glibc..."
	@if [ ! -f $(TMPDIR)/$(GLIBC_TARBALL) ]; then \
		wget -P $(TMPDIR) $(GLIBC_URL); \
	fi

	@if [ ! -d $(TMPDIR)/$(GLIBC_VERSION) ]; then \
		tar -C $(TMPDIR) -xf $(TMPDIR)/$(GLIBC_TARBALL); \
	fi

	cd $(TMPDIR)/$(GLIBC_VERSION); \
	rm -rf build; \
	mkdir build; \
	cd build; \
	../configure \
	libc_cv_slibdir=/lib/x86_64-dioOS-gnu \
    --prefix=/usr \
    --disable-werror \
    --libdir=/lib/x86_64-dioOS-gnu \
    --host=x86_64-pc-linux-gnu; \
	$(MAKE) -j$(NPROC); \
	$(MAKE) DESTDIR=$(DISKDIR) install
	
	@if [ ! -f $(DISKDIR)/lib64 ]; then \
		mkdir -p $(DISKDIR)/lib64; \
	fi

	cp $(DISKDIR)/lib/x86_64-dioOS-gnu/ld-linux-x86-64.so.2 $(DISKDIR)/lib64
	cp $(DISKDIR)/lib/x86_64-dioOS-gnu/ld-linux-x86-64.so.2 $(DISKDIR)/lib
	cp $(DISKDIR)/lib/x86_64-dioOS-gnu//crt1.o $(DISKDIR)/lib64
	cp $(DISKDIR)/lib/x86_64-dioOS-gnu//crti.o $(DISKDIR)/lib64
	cp $(DISKDIR)/lib/x86_64-dioOS-gnu//crtn.o $(DISKDIR)/lib64