
OSNAME = dioOS

OSDIR = ../
CC = gcc
LDS = kernel.ld
ASMC = nasm
LD = ld
GAS = gcc

CFLAGS = -g -fno-builtin -Wno-write-strings -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-omit-frame-pointer -O0 $(INCLUDE_FLAGS)
CPPFLAGS = -g -fno-builtin -Wno-write-strings -ffreestanding -fshort-wchar -mno-red-zone -fno-exceptions -fno-omit-frame-pointer -fno-rtti $(INCLUDE_FLAGS)
ASMFLAGS = -g
GASFLAGS = -g -c
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
GASSRC = $(call rwildcard,$(SRCDIR),*.s)

TRAMPOLINE_SRC = $(SRCDIR)/scheduling/multiprocessor/ap_trampoline.asm
ASMSRC := $(filter-out $(TRAMPOLINE_SRC), $(ASMSRC))
TRAMPOLINE_OBJ = $(OBJDIR)/trampoline_blob.o


OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(CSRC))
OBJS += $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC))
OBJS += $(patsubst $(SRCDIR)/%.asm, $(OBJDIR)/%_asm.o, $(ASMSRC))
OBJS += $(patsubst $(SRCDIR)/%.s, $(OBJDIR)/%_asm.o, $(GASSRC))
DIRS = $(wildcard $(SRCDIR)/*)

OBJS += $(TRAMPOLINE_OBJ)

.SILENT: kernel

kernel: $(OBJS) link

# Compile the Trampoline to Raw Binary
$(OBJDIR)/trampoline.bin: $(TRAMPOLINE_SRC)
	@ printf "\e[0;33mNASM\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(ASMC) -f bin $^ -o $@

# Wrap the Raw Binary into an ELF Object
# This allows the linker to include the binary data.
# We redefine the symbols to make them nice to use in C++.

$(TRAMPOLINE_OBJ): $(OBJDIR)/trampoline.bin
	@ printf "\e[0;33mOBJCOPY\e[0m\t $^\n"
	@ objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
		--redefine-sym _binary_lib_trampoline_bin_start=trampoline_blob_start \
		--redefine-sym _binary_lib_trampoline_bin_end=trampoline_blob_end \
		--redefine-sym _binary_lib_trampoline_bin_size=trampoline_blob_size \
		$< $@

$(OBJDIR)/interrupts/interrupts.o: $(SRCDIR)/interrupts/interrupts.cpp
	@ printf "\e[0;33mCC\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(CC) -g -fno-exceptions -fno-rtti -mno-red-zone -mgeneral-regs-only -ffreestanding $(INCLUDE_FLAGS) -c $^ -o $@


$(OBJDIR)/acpica/%.o: $(SRCDIR)/acpica/%.c
	@ printf "\e[0;33mCC\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(CC) $(CFLAGS) -Wno-implicit-function-declaration $(INCLUDE_FLAGS) -c $^ -o $@

$(OBJDIR)/syscalls/syscalls.o: $(SRCDIR)/syscalls/syscalls.cpp
	@ printf "\e[0;33mCC\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(CC) $(CFLAGS) -O0 $(INCLUDE_FLAGS) -c $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@ printf "\e[0;33mCC\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(CC) $(CPPFLAGS) -c $^ -o $@

$(OBJDIR)/%_asm.o: $(SRCDIR)/%.asm
	@ printf "\e[0;33mNASM\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(ASMC) $(ASMFLAGS) $^ -f elf64 -o $@

$(OBJDIR)/%_asm.o: $(SRCDIR)/%.s
	@ printf "\e[0;33mGAS\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(GAS) $(GASFLAGS) $^ -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@ printf "\e[0;33mCC\e[0m\t $^\n"
	@ mkdir -p $(@D)
	@ $(CC) $(CFLAGS) -c $^ -o $@

link:
	@ printf "\e[0;36mLinking\e[0m\n"
	@ $(LD) $(LDFLAGS) -o $(BUILDDIR)/kernel.elf $(OBJS)

setup:
	@mkdir $(BUILDDIR)
	@mkdir $(SRCDIR)
	@mkdir $(OBJDIR)

clean:
	rm -rf lib
