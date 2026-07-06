# Makefile wrapper for tcmake build system
# Author: tca
# Date:   2026-07-03

TCMAKE_BUILD  := ../tcmake/bin/tcmake_build.sh
TCMAKE_PREFIX ?= $(TCAMAKE_PREFIX)
RSYNC         := rsync -av
MKDIR         := mkdir -p

.PHONY: all build clean distclean test install help

all: build

build:
	@$(TCMAKE_BUILD) all
	@echo

test:
	@$(TCMAKE_BUILD) test
	@echo

install:
ifdef TCMAKE_PREFIX
	$(MKDIR) $(TCMAKE_PREFIX)/include/trinoquery
	$(MKDIR) $(TCMAKE_PREFIX)/lib
	$(RSYNC) --delete include/ $(TCAMAKE_PREFIX)/include/tcanetpp/
	$(RSYNC) lib/ $(TCAMAKE_PREFIX)/lib/
	@echo
else
	@$(RSYNC) build/bin ./
	@$(RSYNC) build/lib ./
endif

clean:
	@$(TCMAKE_BUILD) clean
	@echo

distclean:
	@$(TCMAKE_BUILD) distclean
	rm -rf bin lib
	@echo

# Reconfigure (useful after changing CMakeLists.txt)
config:
	@$(TCMAKE_BUILD) config

help:
	@echo "tcmake build wrapper - Available targets:"
	@echo ""
	@echo "  make              - Build the project (same as 'make build')"
	@echo "  make build        - Configure and build"
	@echo "  make config       - Reconfigure CMake"
	@echo "  make test         - Build and run tests"
	@echo "  make install      - Install to PREFIX (default: .)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make distclean    - Remove entire build directory"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  PREFIX=/path      - Installation prefix (default: /usr/local)"
	@echo ""
	@echo "Examples:"
	@echo "  make"
	@echo "  make test"
	@echo "  make install PREFIX=/opt/local"
