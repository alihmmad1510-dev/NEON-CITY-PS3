TARGET := neoncity
OBJS   := src/main.o

# لو PSL1GHT مش معرّف، نستخدم مسار افتراضي
PSL1GHT ?= $(CURDIR)/ps3dev/psl1ght

# محاولة استخدام ppu_rules لو موجود
PPU_RULES := $(PSL1GHT)/ppu_rules
ifeq ($(wildcard $(PPU_RULES)),)
  $(info ⚠️  ppu_rules not found at $(PPU_RULES))
  $(info PSL1GHT=$(PSL1GHT))
endif

-include $(PPU_RULES)

all: check build

check:
	@if [ ! -f "$(PPU_RULES)" ]; then \
		echo "❌ ppu_rules NOT FOUND: $(PPU_RULES)"; \
		echo "📂 Searching for it..."; \
		find $(CURDIR) -name "ppu_rules" -type f 2>/dev/null | head; \
		exit 1; \
	fi
	@echo "✅ ppu_rules found"

build: $(TARGET).self

clean:
	rm -f $(OBJS) $(TARGET).self $(TARGET).elf

.PHONY: all build check clean