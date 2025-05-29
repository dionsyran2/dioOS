APPNAME=explorer_app
LDS = app.ld
CC = gcc
ASMC = nasm
LD = ld

INCLUDE_DIRS = ../../../stdlibs
INCLUDE_FLAGS = $(addprefix -I,$(INCLUDE_DIRS))

CFLAGS = -g -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-rtti -Wall -Wextra -Wuninitialized $(INCLUDE_FLAGS)
ASMFLAGS = 
LDFLAGS = -T $(LDS) -static -Bsymbolic -nostdlib --no-relax

SRCDIR := ./
OBJDIR := ../../lib/explorer
BUILDDIR = ../../bin

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SRCDIR),*.cpp)
LIBSRC = $(call rwildcard,$(INCLUDE_DIRS),*.cpp)

ASMSRC = $(call rwildcard,$(SRCDIR),*.asm)  
LIBASMSRC = $(call rwildcard,$(INCLUDE_DIRS),*.asm)


OBJS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_asm.o, $(ASMSRC))
OBJS += $(LIBOBJS)


LIBOBJS = $(patsubst $(INCLUDE_DIRS)/%.cpp, $(OBJDIR)/%.o, $(LIBSRC))
LIBOBJS += $(patsubst $(INCLUDE_DIRS)/%.asm, $(OBJDIR)/%_asm.o, $(LIBASMSRC))



DIRS = $(wildcard $(SRCDIR)/*)

app: $(OBJS) link
	
$(OBJDIR)/%.o: $(INCLUDE_DIRS)/%.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@
	
$(OBJDIR)/%_asm.o: $(INCLUDE_DIRS)/%.asm
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $^ -o $@
	
$(OBJDIR)/%_asm.o: $(SRCDIR)/%.asm
	@ echo !==== COMPILING $^
	@ mkdir -p $(@D)
	$(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@
	 
link:
	@ echo !==== LINKING
	$(LD) $(LDFLAGS) -o $(BUILDDIR)/$(APPNAME).elf $(OBJS)
