# ============================================================
#  NEON CITY — Standalone Makefile (بدون ppu_rules)
# ============================================================

TARGET  := neoncity
SRC     := src/main.c
OBJ     := src/main.o

PS3DEV  := $(CURDIR)/ps3dev

# اكتشاف الأدوات تلقائيًا
PPU_GCC      := $(shell find $(PS3DEV) -name "ppu-gcc" -type f 2>/dev/null | head -1)
PPU_BIN      := $(dir $(PPU_GCC))
TINY3D_H     := $(shell find $(PS3DEV) -name "tiny3d.h" -type f 2>/dev/null | head -1)
LIBTINY3D    := $(shell find $(PS3DEV) -name "libtiny3d.a" -type f 2>/dev/null | head -1)
LIBFONT3D    := $(shell find $(PS3DEV) -name "libfont3d.a" -type f 2>/dev/null | head -1)
MAKE_SELF    := $(shell find $(PS3DEV) -name "make_self" -type f 2>/dev/null | head -1)

INCLUDE_DIR  := $(dir $(TINY3D_H))
LIB_DIR      := $(dir $(LIBTINY3D))
PPU_LIB_DIR  := $(dir $(LIBFONT3D))

# تشخيص
$(info ============================================)
$(info PPU_GCC    = $(PPU_GCC))
$(info INCLUDE    = $(INCLUDE_DIR))
$(info LIB_DIR    = $(LIB_DIR))
$(info PPU_LIB    = $(PPU_LIB_DIR))
$(info MAKE_SELF  = $(MAKE_SELF))
$(info ============================================)

CFLAGS  := -O2 -Wall -mhard-float -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/tiny3d
LDFLAGS := -L$(LIB_DIR) -L$(PPU_LIB_DIR)

LIBS    := -ltiny3d -lrsx -lgcm_sys -lio -lsysutil -lsysmodule \
           -laudio -lfont3d -lm -lnet -lrt -llv2

all: $(TARGET).self

$(OBJ): $(SRC)
	@mkdir -p src
	$(PPU_GCC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(PPU_GCC) $(OBJ) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).self: $(TARGET).elf
	@if [ -n "$(MAKE_SELF)" ]; then \
		$(MAKE_SELF) $(TARGET).elf $(TARGET).self; \
	else \
		echo "make_self not found — copying elf"; \
		cp $(TARGET).elf $(TARGET).self; \
	fi

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).self $(TARGET).map

.PHONY: all clean