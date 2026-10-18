# Makefile — organizor
#
#
#   make              build ./organizor
#   make install      install to $(HOME)/.local/bin/organizor
#   make uninstall    remove the installed binary
#   make clean        remove build artifacts
#   make rebuild      clean + build
#   make strip        belt-and-suspenders strip(1) on the binary
#
# Overrides:
#   make CXX=clang++ OPT="-O2" PREFIX=/usr/local install
#   make DESTDIR=/tmp/stage install    (for packaging)

APP       := organizor
SRC       := main.cpp
OBJ       := $(SRC:.cpp=.o)
DEP       := $(SRC:.cpp=.d)

# --- install location (XDG user-local by default)
PREFIX    ?= $(HOME)/.local
BINDIR    ?= $(PREFIX)/bin

# --- toolchain (override with `make CXX=clang++`)
CXX       ?= g++
STRIP     ?= strip

# --- flags
CXXSTD    := -std=c++20
WARN      := -Wall -Wextra -Wpedantic -Wshadow
OPT       := -O3 -march=native -mtune=native
TUNE      := -flto -fno-plt -fno-rtti -fno-semantic-interposition \
             -fvisibility=hidden -ffunction-sections -fdata-sections \
             -fomit-frame-pointer -fstack-protector-strong \
             -fstack-clash-protection -DNDEBUG
DEPFLAGS  := -MMD -MP

CXXFLAGS  := $(CXXSTD) $(WARN) $(OPT) $(TUNE) $(DEPFLAGS)
# -s           : strip symbols at link time
# -Wl,--gc-sections : drop unreferenced code (paired with -ffunction-sections)
# -Wl,-O1      : linker-side optimization pass
# -Wl,--as-needed : drop unused DT_NEEDED entries
LDFLAGS   := -flto -Wl,--gc-sections -Wl,-O1 -Wl,--as-needed -s
LDLIBS    :=

# --- targets
.PHONY: all clean install uninstall rebuild strip

all: $(APP)

$(APP): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# -s at link time normally suffices; this is a safety net for toolchains
# that ignore -s under LTO (rare, but cheap to run).
strip: $(APP)
	$(STRIP) --strip-all $(APP)

install: $(APP)
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 $(APP) $(DESTDIR)$(BINDIR)/$(APP)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(APP)

clean:
	rm -f $(APP) $(OBJ) $(DEP)

rebuild: clean all

-include $(DEP)
