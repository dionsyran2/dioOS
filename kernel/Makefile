
OSNAME = dioOS


OSDIR = ../
LDS = kernel.ld
CC = gcc
ASMC = nasm
LD = ld

CFLAGS = -g -Wno-write-strings -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-omit-frame-pointer -fno-rtti $(INCLUDE_FLAGS)
ASMFLAGS = 
LDFLAGS = -T $(LDS) -static -Bsymbolic -nostdlib

SRCDIR := src
OBJDIR := lib
BUILDDIR = bin

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

clean:
	rm -rf lib
