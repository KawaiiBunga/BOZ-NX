APP_TITLE := CoD Black Ops Zombies
APP_AUTHOR := Kawaii
TOPDIR ?= $(CURDIR)
APP_VERSION := $(strip $(shell cat "$(TOPDIR)/../VERSION"))
TARGET := codboz
PORT_PAYLOAD := codboz_native
LAUNCHER_SOURCES := source ../third_party/lzma
LAUNCHER_LIBS := -lminizip -lz
# Custom cover art stays local. Fresh clones still build with libnx's icon.
A32 ?= $(TOPDIR)/../runtime
include $(A32)/launcher/launcher.mk
export APP_ICON := $(if $(wildcard $(TOPDIR)/icon.jpg),$(TOPDIR)/icon.jpg,$(DEVKITPRO)/libnx/default_icon.jpg)

# Metadata and icon changes must repack the NRO even if its code is unchanged.
ifeq ($(BUILD),$(notdir $(CURDIR)))
$(OUTPUT).nacp: $(TOPDIR)/Makefile $(TOPDIR)/../VERSION
$(OUTPUT).nro: $(APP_ICON)
endif
