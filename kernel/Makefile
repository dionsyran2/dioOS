
OSNAME = CustomOS

OVMFDIR = ../OVMFbin
APPDIR = ../applications/bin
LDS = kernel.ld
CC = gcc
ASMC = nasm
LD = ld

CFLAGS = -g -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-omit-frame-pointer -fno-rtti $(INCLUDE_FLAGS)
ASMFLAGS = 
LDFLAGS = -T $(LDS) -static -Bsymbolic -nostdlib

SRCDIR := src
OBJDIR := lib
BUILDDIR = bin
BOOTEFI := ../bootloader/other/BOOTX64.EFI

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

INCLUDE_DIRS = $(SRCDIR)/uACPI/include $(SRCDIR)/../include
INCLUDE_FLAGS = $(addprefix -I,$(INCLUDE_DIRS))
SRC = $(call rwildcard,$(SRCDIR),*.cpp)
CSRC = $(call rwildcard,$(SRCDIR),*.c)
ASMSRC = $(call rwildcard,$(SRCDIR),*.asm) 
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CSRC))
OBJS += $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_asm.o, $(ASMSRC))
DIRS = $(wildcard $(SRCDIR)/*)


kernel: $(OBJS) link
$(OBJDIR)/interrupts/interrupts.o: $(SRCDIR)/interrupts/interrupts.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) -g -fno-exceptions -fno-rtti -mno-red-zone -mgeneral-regs-only -ffreestanding $(INCLUDE_FLAGS) -c $^ -o $@


$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@
	
$(OBJDIR)/%_asm.o: $(SRCDIR)/%.asm
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@
	 
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@

link:
	@ echo !==== LINKING
	$(LD) $(LDFLAGS) -o $(BUILDDIR)/kernel.elf $(OBJS)

setup:
	@mkdir $(BUILDDIR)
	@mkdir $(SRCDIR)
	@mkdir $(OBJDIR)

buildimg:
	dd if=/dev/zero of=$(BUILDDIR)/$(OSNAME).img bs=1024 count=324350
	mformat -i $(BUILDDIR)/$(OSNAME).img -t 64 -s 32 -h 255 -F ::
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/EFI
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/EFI/BOOT
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/boot
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/boot/grub
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/applications
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/images
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/fonts
	mmd -i $(BUILDDIR)/$(OSNAME).img ::/lib

	mcopy -i $(BUILDDIR)/$(OSNAME).img grub.cfg ::/boot/grub
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BOOTEFI) ::/EFI/BOOT
	mcopy -i $(BUILDDIR)/$(OSNAME).img startup.nsh ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/kernel.elf ::/boot
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/grubbg.jpg ::/boot/grub
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/fonts/* ::/fonts
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/lib/* ::/lib

	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/files/sys_registry.sys ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/images/* ::/images

	mcopy -i $(BUILDDIR)/$(OSNAME).img $(APPDIR)/* ::/applications

	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/test.pcm ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/track01.mp3 ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/track02.mp3 ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/track03.mp3 ::
	mcopy -i $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/track04.mp3 ::

	qemu-img convert -f raw -O vmdk $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/$(OSNAME).vmdk
	qemu-img convert -f raw -O vpc $(BUILDDIR)/$(OSNAME).img $(BUILDDIR)/$(OSNAME).vhd
	readelf -h $(BUILDDIR)/kernel.elf

clean:
	rm -rf lib

run:
	qemu-system-x86_64 -drive file=$(BUILDDIR)/$(OSNAME).img -m 256M -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="$(OVMFDIR)/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="$(OVMFDIR)/OVMF_VARS-pure-efi.fd" -net none
