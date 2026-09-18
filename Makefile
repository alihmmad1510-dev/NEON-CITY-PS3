# ============================================================
#  NEON CITY — Standalone Makefile
#  مستقل تمامًا — يكتشف الـ Toolchain تلقائيًا
# ============================================================

TARGET  := neoncity
SRC     := src/main.c
OBJ     := src/main.o
PS3DEV  := $(CURDIR)/ps3dev

# ============================================================
# اكتشاف المترجم
# ============================================================
PPU_GCC := $(shell find $(PS3DEV) -name "ppu-gcc" -type f 2>/dev/null | head -1)
ifeq ($(strip $(PPU_GCC)),)
  PPU_GCC := $(shell find $(PS3DEV) -name "powerpc64-ps3-elf-gcc" -type f 2>/dev/null | head -1)
endif
ifeq ($(strip $(PPU_GCC)),)
  PPU_GCC := $(shell find $(PS3DEV) -name "*ps3*gcc" -type f 2>/dev/null | head -1)
endif
ifeq ($(strip $(PPU_GCC)),)
  PPU_GCC := $(shell find $(PS3DEV) -name "*-gcc" -type f 2>/dev/null | head -1)
endif

# ============================================================
# اكتشاف make_self
# ============================================================
MAKE_SELF := $(shell find $(PS3DEV) -name "make_self" -type f 2>/dev/null | head -1)

# ============================================================
# اكتشاف المسارات
# ============================================================
TINY3D_H    := $(shell find $(PS3DEV) -name "tiny3d.h" -type f 2>/dev/null | head -1)
INCLUDE_DIR := $(dir $(TINY3D_H))

LIB_TINY3D  := $(shell find $(PS3DEV) -name "libtiny3d.a" -type f 2>/dev/null | head -1)
LIB_FONT3D  := $(shell find $(PS3DEV) -name "libfont3d.a" -type f 2>/dev/null | head -1)

LIB_DIR_1   := $(dir $(LIB_TINY3D))
LIB_DIR_2   := $(dir $(LIB_FONT3D))
PORTLIB_DIR := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/lib" 2>/dev/null | head -1)
PPU_LIB_DIR := $(shell find $(PS3DEV) -type d -path "*/ppu/lib" 2>/dev/null | grep -v portlibs | head -1)

# ============================================================
# تشخيص — هيظهر في اللوج
# ============================================================
$(info ============================================)
$(info PPU_GCC    = $(PPU_GCC))
$(info INCLUDE    = $(INCLUDE_DIR))
$(info LIB_1      = $(LIB_DIR_1))
$(info LIB_2      = $(LIB_DIR_2))
$(info PORTLIB    = $(PORTLIB_DIR))
$(info PPU_LIB    = $(PPU_LIB_DIR))
$(info MAKE_SELF  = $(MAKE_SELF))
$(info ============================================)

# ============================================================
# Flags
# ============================================================
CFLAGS  := -O2 -Wall -mhard-float -I$(INCLUDE_DIR)
LDFLAGS := -L$(LIB_DIR_1) -L$(LIB_DIR_2) -L$(PORTLIB_DIR) -L$(PPU_LIB_DIR)

LIBS    := -ltiny3d -lrsx -lgcm_sys -lio -lsysutil -lsysmodule \
           -laudio -lfont3d -lm -lnet -lrt -llv2

# ============================================================
# Targets
# ============================================================
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