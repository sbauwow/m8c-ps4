# m8c PS4 port — display mirror + DS4 input + USB iso audio.
# Staged upstream: app/src @ m8c v1.7.10; PS4 backends in port/.
# Upstream usb.c + usb_audio.c are REPLACED by port/usb_ps4.c +
# port/usbio_ps4.c (sceUsbd); audio.c + serial.c self-exclude via
# -DUSE_LIBUSB. TITLE_ID must be [A-Z]{4}[0-9]{5} or BGFT rejects the pkg
# with CE-32957-6 (learned the hard way).

TITLE       := m8c PS4
VERSION     := 1.00
TITLE_ID    := MBCA00001
CONTENT_ID  := UP0001-MBCA00001_00-M8CTRACKER000000

LIBS        := -lc -lkernel -lSDL2 -lSceUsbd \
               -lSceVideoOut -lScePad -lSceUserService -lSceAudioOut -lSceSysmodule -lSceSystemService

# Sony runtime modules aren't redistributed here; copied from the
# OpenOrbis SDL2 sample on first build.
LIBMODULES  := sce_module/libc.prx sce_module/libSceFios2.prx
SDK_SAMPLE   = $(TOOLCHAIN)/samples/SDL2

# Same defaults scripts/deploy.sh uses, so a bare `make` works too.
OO_PS4_TOOLCHAIN ?= $(HOME)/ps4-toolchain/OpenOrbis/PS4Toolchain
export OO_PS4_TOOLCHAIN   # create-fself reads it from the environment
OPENSSL11_LIB    ?= $(HOME)/ps5-jailbreak/ps4-transfer/pkgtool/openssl11/usr/lib
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
# PkgTool.Core is .NET: it needs invariant globalization and OpenSSL 1.1.
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT := 1
export LD_LIBRARY_PATH := $(OPENSSL11_LIB)$(if $(LD_LIBRARY_PATH),:$(LD_LIBRARY_PATH))
INTDIR      := x64/Debug

SRC_DIR     := app/src
PORT_DIR    := port

# Upstream sources minus the libusb backends this port replaces.
CFILES      := $(filter-out usb.c usb_audio.c, \
               $(notdir $(wildcard $(SRC_DIR)/*.c)))
CFILES      += usb_ps4.c ps4_shims.c usbio_ps4.c audio_native_ps4.c
# Audio is opt-in via audio_enabled in /data/m8c_config.ini; with it off the
# app never starts usbio_ps4.c and keeps the sync display-only path.

OBJS        := $(addprefix $(INTDIR)/,$(CFILES:.c=.o))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
               -I$(TOOLCHAIN)/include/SDL2 \
               -I$(TOOLCHAIN)/include/orbis/_types \
               -I$(SRC_DIR) -I$(PORT_DIR) \
               -DUSE_LIBUSB -DPS4 -std=gnu17 -O2 -Wall
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

CC          := /usr/bin/clang
LD          := /usr/bin/ld.lld

CDIR        := linux
PKGTOOL     := $(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core
CREATE_GP4  := $(TOOLCHAIN)/bin/$(CDIR)/create-gp4
CREATE_FSELF:= $(TOOLCHAIN)/bin/$(CDIR)/create-fself

_unused     := $(shell mkdir -p $(INTDIR))

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(PKGTOOL) pkg_build $< .

pkg.gp4: eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx $(LIBMODULES)
	$(CREATE_GP4) -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/icon0.png: icon/make_icon.py
	python3 icon/make_icon.py
	cp icon/icon0.png sce_sys/icon0.png

sce_sys/param.sfo: Makefile
	$(PKGTOOL) sfo_new $@
	$(PKGTOOL) sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(PKGTOOL) sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(PKGTOOL) sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(PKGTOOL) sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(PKGTOOL) sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(PKGTOOL) sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(PKGTOOL) sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(PKGTOOL) sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/m8c.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(INTDIR)/m8c.elf -out=$(INTDIR)/m8c.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PORT_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

sce_module/%.prx:
	@mkdir -p $(@D)
	cp $(SDK_SAMPLE)/$@ $@

sce_sys/about/right.sprx:
	@mkdir -p $(@D)
	cp $(SDK_SAMPLE)/$@ $@

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 $(CONTENT_ID).pkg sce_sys/param.sfo
