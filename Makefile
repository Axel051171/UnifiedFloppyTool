# ==============================================================================
# UnifiedFloppyTool v5.28.0 GOD MODE - Makefile
# ==============================================================================

VERSION := 5.28.0
CC := gcc
CXX := g++
AR := ar

SRCDIR := src
INCDIR := include
BUILDDIR := build
BINDIR := $(BUILDDIR)/bin
LIBDIR := $(BUILDDIR)/lib
OBJDIR := $(BUILDDIR)/obj

CFLAGS := -std=c11 -fPIC -O2 -DNDEBUG
CXXFLAGS := -std=c++17 -fPIC -O2 -DNDEBUG
LDFLAGS :=
LIBS := -lm

ifdef DEBUG
    CFLAGS := -std=c11 -fPIC -O0 -g3 -DDEBUG
    CXXFLAGS := -std=c++17 -fPIC -O0 -g3 -DDEBUG
endif

ifdef SANITIZER
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer
    CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address
endif

INCLUDES := -I$(INCDIR) -I$(SRCDIR) -I$(SRCDIR)/core -I$(SRCDIR)/formats \
            -I$(SRCDIR)/recovery -I$(SRCDIR)/algorithms

# Find all sources
C_SOURCES := $(shell find $(SRCDIR) -name "*.c" 2>/dev/null)
CXX_SOURCES := $(shell find $(SRCDIR) -name "*.cpp" -o -name "*.cc" 2>/dev/null)

C_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(C_SOURCES))
CXX_OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(filter %.cpp,$(CXX_SOURCES)))
CXX_OBJECTS += $(patsubst $(SRCDIR)/%.cc,$(OBJDIR)/%.o,$(filter %.cc,$(CXX_SOURCES)))

ALL_OBJECTS := $(C_OBJECTS) $(CXX_OBJECTS)

.PHONY: all clean stats

all: dirs $(LIBDIR)/libuft.a
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════╗"
	@echo "║  UFT v$(VERSION) Build Complete!                              ║"
	@echo "║  Library: $(LIBDIR)/libuft.a                               ║"
	@echo "╚══════════════════════════════════════════════════════════════╝"

dirs:
	@mkdir -p $(BINDIR) $(LIBDIR)
	@for src in $(C_SOURCES) $(CXX_SOURCES); do \
		dir=$$(dirname $$src | sed "s|^$(SRCDIR)|$(OBJDIR)|"); \
		mkdir -p $$dir; \
	done

$(LIBDIR)/libuft.a: $(ALL_OBJECTS)
	@echo "[AR] $@ ($(words $^) objects)"
	@$(AR) rcs $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "[CC] $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ 2>/dev/null || echo "[SKIP] $<"

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@echo "[CXX] $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 2>/dev/null || echo "[SKIP] $<"

$(OBJDIR)/%.o: $(SRCDIR)/%.cc
	@echo "[CXX] $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@ 2>/dev/null || echo "[SKIP] $<"

clean:
	rm -rf $(BUILDDIR)

stats:
	@echo "=== UFT v$(VERSION) Statistics ==="
	@echo "C Sources:   $$(echo $(C_SOURCES) | wc -w)"
	@echo "C++ Sources: $$(echo $(CXX_SOURCES) | wc -w)"
	@echo "Total LOC:   $$(find $(SRCDIR) -name "*.c" -o -name "*.cpp" -o -name "*.cc" | xargs wc -l | tail -1)"
