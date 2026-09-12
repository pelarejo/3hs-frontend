MAKEFILE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
FRONTEND_DIR := $(abspath $(MAKEFILE_DIR)/..)
BUILD_DIR := $(FRONTEND_DIR)/.build-docker
IMAGE := 3hs-builder:local
AUTH_FILE ?= $(FRONTEND_DIR)/.local-secrets/hsapi-auth.env

MODE ?= debug
FORMAT ?= both
BUILD_JOBS ?= 2
SERVER_BASE ?=
NB_BASE ?=
CDN_BASE ?=
UPDATE_BASE ?=
SITE_URL ?=

NB_BASE_INPUT := $(NB_BASE)
CDN_BASE_INPUT := $(CDN_BASE)
UPDATE_BASE_INPUT := $(UPDATE_BASE)
SITE_URL_INPUT := $(SITE_URL)
ALL_URLS_EXPLICIT := $(and $(strip $(NB_BASE_INPUT)),$(strip $(CDN_BASE_INPUT)),$(strip $(UPDATE_BASE_INPUT)),$(strip $(SITE_URL_INPUT)))
SERVER_BASE_USED := $(if $(strip $(SERVER_BASE)),$(if $(ALL_URLS_EXPLICIT),,1),)

override NB_BASE := $(if $(strip $(NB_BASE_INPUT)),$(NB_BASE_INPUT),$(if $(strip $(SERVER_BASE)),$(SERVER_BASE)/nbapi,))
override CDN_BASE := $(if $(strip $(CDN_BASE_INPUT)),$(CDN_BASE_INPUT),$(strip $(SERVER_BASE)))
override UPDATE_BASE := $(if $(strip $(UPDATE_BASE_INPUT)),$(UPDATE_BASE_INPUT),$(if $(strip $(SERVER_BASE)),$(SERVER_BASE)/update,))
override SITE_URL := $(if $(strip $(SITE_URL_INPUT)),$(SITE_URL_INPUT),$(if $(strip $(SERVER_BASE)),$(SERVER_BASE)/site,))

export SERVER_BASE SERVER_BASE_USED NB_BASE CDN_BASE UPDATE_BASE SITE_URL

.PHONY: help auth build clean image-clean distclean

help:
	@echo '3hs frontend Docker build'
	@echo
	@echo 'Usage: make -C docker build SERVER_BASE=http://host:8000 [NB_BASE=url] [CDN_BASE=url] [UPDATE_BASE=url] [SITE_URL=url] [MODE=debug|release] [FORMAT=both|3dsx|cia|elf] [BUILD_JOBS=N] [AUTH_FILE=path]'
	@echo
	@echo 'Targets:'
	@echo '  help         Show this help.'
	@echo '  auth         Prompt for credentials and save them to AUTH_FILE.'
	@echo '  build        Build with Docker; use SERVER_BASE defaults or provide all four URLs.'
	@echo '  clean        Remove $(BUILD_DIR).'
	@echo '  image-clean  Remove only the $(IMAGE) Docker image, if present.'
	@echo '  distclean    Run clean and image-clean.'

auth:
	@bash '$(MAKEFILE_DIR)/setup-auth.sh' '$(AUTH_FILE)'

build:
	@if [ -n "$$SERVER_BASE_USED" ]; then bash '$(MAKEFILE_DIR)/validate-base-url.sh' SERVER_BASE; fi
	@BUILD_JOBS='$(BUILD_JOBS)' AUTH_FILE='$(AUTH_FILE)' bash '$(MAKEFILE_DIR)/build.sh' '$(MODE)' '$(FORMAT)'

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
