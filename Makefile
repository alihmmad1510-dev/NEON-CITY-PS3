# ============================================================
#  NEON CITY - Makefile with > prefix (no TAB needed!)
# ============================================================
.RECIPEPREFIX = >

TARGET  := neoncity
SRC     := src/main.c
OBJ     := src/main.o
PS3DEV  := $(CURDIR)/ps3dev
export PSL1GHT := $(CURDIR)/ps3dev
export PS3DEV  := $(CURDIR)/ps3dev

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

$(OBJ): $(SRC)
> @mkdir -p src
> $(PPU_GCC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
> $(PPU_GCC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).self: $(TARGET).elf
> @if [ -n "$(MAKE_SELF)" ]; then \
>   $(MAKE_SELF) $(TARGET).elf $(TARGET).self; \
> else \
>   cp $(TARGET).elf $(TARGET).self; \
> fi

clean:
> rm -f $(OBJ) $(TARGET).elf $(TARGET).self $(TARGET).fself $(TARGET).pkg
> rm -rf pkgbuild

.PHONY: all clean