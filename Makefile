# Makefile wrapper for tcmake build system
# Author: tcmake wrapper
# Date:   2026-07-03

TCMAKE_BUILD := ../tcmake/bin/tcmake_build.sh
PREFIX       ?= /usr/local

.PHONY: all build clean distclean test install help

# Default target
all: build

# Configure and build the project
build:
	@$(TCMAKE_BUILD) all

# Run tests
test:
	@$(TCMAKE_BUILD) test

# Install to PREFIX
install:
	@$(TCMAKE_BUILD) install -p $(PREFIX)

# Clean build artifacts (keeps configuration)
clean:
	@$(TCMAKE_BUILD) clean

# Full clean (removes build directory)
distclean:
	@$(TCMAKE_BUILD) distclean

# Reconfigure (useful after changing CMakeLists.txt)
config:
	@$(TCMAKE_BUILD) config

# Help target
help:
	@echo "tcmake build wrapper - Available targets:"
	@echo ""
	@echo "  make              - Build the project (same as 'make build')"
	@echo "  make build        - Configure and build"
	@echo "  make config       - Reconfigure CMake"
	@echo "  make test         - Build and run tests"
	@echo "  make install      - Install to PREFIX (default: /usr/local)"
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
