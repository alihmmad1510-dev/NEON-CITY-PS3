# =====================================================================
#  FILE: Makefile
#  PROJECT: NEON CITY ULTRA v4
#  DESCRIPTION: Links 60MB of textures via .incbin assembly
# =====================================================================

.RECIPEPREFIX = >

TARGET   := neoncity
PS3DEV   := $(CURDIR)/ps3dev
export PSL1GHT := $(CURDIR)/ps3dev
export PS3DEV  := $(CURDIR)/ps3dev

SRC_MAIN   := src/main.c
SRC_ASSETS := src/assets.c
OBJ_MAIN   := src/main.o
OBJ_ASSETS := src/assets.o

# Assembly files (generated in workflow) → objects
TEXTURE_SRCS := $(wildcard texture*.S)
TEXTURE_OBJS := $(TEXTURE_SRCS:.S=.o)

PPU_GCC := $(shell find $(PS3DEV) -name "ppu-gcc" -type f 2>/dev/null | head -1)
ifeq ($(strip $(PPU_GCC)),)
  PPU_GCC := $(shell find $(PS3DEV) -name "powerpc64-ps3-elf-gcc" -type f 2>/dev/null | head -1)
endif
MAKE_SELF := $(shell find $(PS3DEV) -name "make_self" -type f 2>/dev/null | head -1)

PS3_INC  := $(shell find $(PS3DEV) -type d -path "*/ppu/include" 2>/dev/null | grep -v portlibs | head -1)
PORT_INC := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/include" 2>/dev/null | head -1)
PS3_LIB  := $(shell find $(PS3DEV) -type d -path "*/ppu/lib" 2>/dev/null | grep -v portlibs | head -1)
PORT_LIB := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/lib" 2>/dev/null | head -1)

CFLAGS  := -O2 -mhard-float -Wno-implicit-function-declaration -I$(PS3_INC) -I$(PORT_INC)
LDFLAGS := -L$(PS3_LIB) -L$(PORT_LIB)
LIBS    := -ltiny3d -lrsx -lgcm_sys -lio -lsysutil -lsysmodule -lfont3d -lm -lnet -lrt -llv2

all: $(TARGET).self

# Compile .S assembly (uses .incbin to embed PNG data)
%.o: %.S
> @echo "  [ASM] $<"
> $(PPU_GCC) -c $< -o $@
> @ls -lh $@

$(OBJ_MAIN): $(SRC_MAIN)
> @mkdir -p src
> $(PPU_GCC) $(CFLAGS) -c $< -o $@

$(OBJ_ASSETS): $(SRC_ASSETS)
> @mkdir -p src
> $(PPU_GCC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS)
> @echo "=== LINKING ==="
> $(PPU_GCC) $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS) $(LDFLAGS) $(LIBS) -o $@
> @ls -lh $@

$(TARGET).self: $(TARGET).elf
> @if [ -n "$(MAKE_SELF)" ]; then \
>   $(MAKE_SELF) $(TARGET).elf $(TARGET).self; \
> else \
>   cp $(TARGET).elf $(TARGET).self; \
> fi
> @ls -lh $@

clean:
> rm -f $(OBJ_MAIN) $(OBJ_ASSETS) $(TEXTURE_OBJS)
> rm -f $(TARGET).elf $(TARGET).self $(TARGET).fself $(TARGET).pkg
> rm -f texture*.png texture*.S
> rm -rf pkgbuild

.PHONY: all clean