TARGET := neoncity
OFILES := src/main.o

# لو PSL1GHT مش معرّف أو غلط، ابحث عنه تلقائيًا
ifeq ($(wildcard $(PSL1GHT)/ppu_rules),)
  PSL1GHT := $(shell find $(CURDIR) -name ppu_rules -type f 2>/dev/null | head -1 | xargs dirname 2>/dev/null)
  export PSL1GHT
endif

$(info >>> PSL1GHT = $(PSL1GHT))
$(info >>> ppu_rules = $(wildcard $(PSL1GHT)/ppu_rules))

include $(PSL1GHT)/ppu_rules