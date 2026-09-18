# ============================================================
#  NEON CITY — Standalone Makefile (نسخة نهائية)
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

MAKE_SELF := $(shell find $(PS3DEV) -name "make_self" -type f 2>/dev/null | head -1)

# ============================================================
# اكتشاف كل مجلدات الـ include المهمة
# ============================================================
# مجلد الـ PS3 SDK الأساسي (فيه ppu-types.h, sysutil.h, io/pad.h ...)
PS3_INC     := $(shell find $(PS3DEV) -type d -path "*/ppu/include" 2>/dev/null | grep -v portlibs | head -1)
# مجلد مكتبات طرف ثالث (فيه tiny3d.h, libfont.h)
PORT_INC    := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/include" 2>/dev/null | head -1)
# مجلد ps3dev/psl1ght (لو موجود)
PSL_INC     := $(shell find $(PS3DEV) -type d -name "psl1ght" 2>/dev/null | head -1)
PSL_PPU_INC := $(shell find $(PS3DEV) -type d -path "*psl1ght/ppu/include" 2>/dev/null | head -1)

# ============================================================
# اكتشاف مجلدات المكتبات
# ============================================================
PS3_LIB     := $(shell find $(PS3DEV) -type d -path "*/ppu/lib" 2>/dev/null | grep -v portlibs | head -1)
PORT_LIB    := $(shell find $(PS3DEV) -type d -path "*portlibs/ppu/lib" 2>/dev/null | head -1)

# ============================================================
# تشخيص
# ============================================================
$(info ============================================)
$(info PPU_GCC      = $(PPU_GCC))
$(info PS3_INC      = $(PS3_INC))
$(info PORT_INC     = $(PORT_INC))
$(info PSL_INC      = $(PSL_INC))
$(info PSL_PPU_INC  = $(PSL_PPU_INC))
$(info PS3_LIB      = $(PS3_LIB))
$(info PORT_LIB     = $(PORT_LIB))
$(info MAKE_SELF    = $(MAKE_SELF))
$(info ============================================)

# ============================================================
# Flags
# ============================================================
INC_FLAGS := -I$(PS3_INC) -I$(PORT_INC) -I$(PS3DEV)/ppu/include

LDFLAGS := -L$(PS3_LIB) -L$(PORT_LIB)

LIBS    := -ltiny3d -lrsx -lgcm_sys -lio -lsysutil -lsysmodule \
           -laudio -lfont3d -lm -lnet -lrt -llv2

CFLAGS  := -O2 -Wall -mhard-float $(INC_FLAGS)

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