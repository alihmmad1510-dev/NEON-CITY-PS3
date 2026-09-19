# =====================================================================
#  FILE: Makefile
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Links 60MB of embedded textures into the binary
# =====================================================================

.RECIPEPREFIX = >

TARGET   := neoncity
PS3DEV   := $(CURDIR)/ps3dev
export PSL1GHT := $(CURDIR)/ps3dev
export PS3DEV  := $(CURDIR)/ps3dev

# Sources
SRC_MAIN   := src/main.c
SRC_ASSETS := src/assets.c
OBJ_MAIN   := src/main.o
OBJ_ASSETS := src/assets.o

# Textures
TEXTURE_PNGS := $(wildcard texture*.png)
TEXTURE_OBJS := $(TEXTURE_PNGS:.png=.o)

# Toolchain
PPU_GCC     := $(shell find $(PS3DEV) -name "ppu-gcc" -type f 2>/dev/null | head -1)
ifeq ($(strip $(PPU_GCC)),)
  PPU_GCC := $(shell find $(PS3DEV) -name "powerpc64-ps3-elf-gcc" -type f 2>/dev/null | head -1)
endif
PPU_OBJCOPY := $(shell find $(PS3DEV) -name "ppu-objcopy" -type f 2>/dev/null | head -1)
ifeq ($(strip $(PPU_OBJCOPY)),)
  PPU_OBJCOPY := $(shell find $(PS3DEV) -name "powerpc64-ps3-elf-objcopy" -type f 2>/dev/null | head -1)
endif
MAKE_SELF   := $(shell find $(PS3DEV) -name "make_self" -type f 2>/dev/null | head -1)

PS3_INC  := $(shell find $(PS3DEV) -type d -path "*/ppu/include" 2>/dev/null | grep -v portlibs | head -1)
PORT_INC := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/include" 2>/dev/null | head -1)
PS3_LIB  := $(shell find $(PS3DEV) -type d -path "*/ppu/lib" 2>/dev/null | grep -v portlibs | head -1)
PORT_LIB := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/lib" 2>/dev/null | head -1)

CFLAGS  := -O2 -mhard-float -Wno-implicit-function-declaration -I$(PS3_INC) -I$(PORT_INC)
LDFLAGS := -L$(PS3_LIB) -L$(PORT_LIB)
LIBS    := -ltiny3d -lrsx -lgcm_sys -lio -lsysutil -lsysmodule -lfont3d -lm -lnet -lrt -llv2

all: $(TARGET).self

# Convert each PNG to an object file
%.o: %.png
> @echo "  OBJCOPY $<"
> $(PPU_OBJCOPY) -I binary -O elf64-powerpc -B powerpc:64 \
>   --rename-section .data=.rodata,alloc,load,readonly,data,contents \
>   $< $@

$(OBJ_MAIN): $(SRC_MAIN)
> @mkdir -p src
> $(PPU_GCC) $(CFLAGS) -c $< -o $@

$(OBJ_ASSETS): $(SRC_ASSETS)
> @mkdir -p src
> $(PPU_GCC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS)
> @echo "=== Linking all objects ==="
> $(PPU_GCC) $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS) $(LDFLAGS) $(LIBS) -o $@
> ls -lh $@

$(TARGET).self: $(TARGET).elf
> @if [ -n "$(MAKE_SELF)" ]; then \
>   $(MAKE_SELF) $(TARGET).elf $(TARGET).self; \
> else \
>   cp $(TARGET).elf $(TARGET).self; \
> fi
> ls -lh $@

clean:
> rm -f $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS)
> rm -f $(TARGET).elf $(TARGET).self $(TARGET).fself $(TARGET).pkg
> rm -f texture*.png
> rm -rf pkgbuild

.PHONY: all clean