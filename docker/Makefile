MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FRONTEND_DIR := $(abspath $(MAKEFILE_DIR)/..)
BUILD_DIR := $(FRONTEND_DIR)/.build-docker
IMAGE := 3hs-builder:local

MODE ?= debug
FORMAT ?= both
BUILD_JOBS ?= 2

.PHONY: help build clean image-clean distclean

help:
	@echo '3hs frontend Docker build'
	@echo
	@echo 'Usage: make -C docker [target] [MODE=debug|release] [FORMAT=both|3dsx|cia|elf] [BUILD_JOBS=N]'
	@echo
	@echo 'Targets:'
	@echo '  help         Show this help.'
	@echo '  build        Build with Docker (default: MODE=debug FORMAT=both BUILD_JOBS=2).'
	@echo '  clean        Remove $(BUILD_DIR).'
	@echo '  image-clean  Remove only the $(IMAGE) Docker image, if present.'
	@echo '  distclean    Run clean and image-clean.'

build:
	@BUILD_JOBS='$(BUILD_JOBS)' bash '$(MAKEFILE_DIR)/build.sh' '$(MODE)' '$(FORMAT)'

clean:
	@rm -rf -- '$(BUILD_DIR)'

image-clean:
	@command -v docker >/dev/null || { echo 'Docker Desktop is required.' >&2; exit 1; }; \
	if docker image inspect '$(IMAGE)' >/dev/null 2>&1; then \
		docker image rm '$(IMAGE)'; \
	elif docker image ls --format '{{.Repository}}:{{.Tag}}' >/dev/null 2>&1; then \
		echo 'Docker image $(IMAGE) is already absent.'; \
	else \
		echo 'Unable to connect to Docker.' >&2; exit 1; \
	fi

distclean: clean image-clean
