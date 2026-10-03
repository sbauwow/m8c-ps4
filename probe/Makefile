# m8c-ps4 Phase 0 probe: sceUsbd on 6.72 GoldHEN.
# Results land in /data/m8c_probe.log on the console; read over GoldHEN FTP.

TITLE       := m8c PS4 Usbd Probe
VERSION     := 1.00
TITLE_ID    := MBCB00001
CONTENT_ID  := UP0001-MBCB00001_00-USBPROBE00000000

LIBS        := -lc -lkernel -lSceUsbd

ASSETS      :=
# Sony runtime modules aren't redistributed here; copied from the
# OpenOrbis SDL2 sample on first build.
LIBMODULES  := sce_module/libc.prx sce_module/libSceFios2.prx
SDK_SAMPLE   = $(TOOLCHAIN)/samples/SDL2

TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
INTDIR      := x64/Debug

OBJS        := $(INTDIR)/main.o

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c \
               -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
               -I$(TOOLCHAIN)/include/orbis/_types -Wall -O2
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
               -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

# Theos clang-11 on PATH targets iOS and cannot build PS4 objects; /usr/bin
# holds the system clang that the hello_world sample builds with.
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
	$(LD) $(OBJS) -o $(INTDIR)/probe.elf $(LDFLAGS)
	$(CREATE_FSELF) -in=$(INTDIR)/probe.elf -out=$(INTDIR)/probe.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/main.o: main.c
	$(CC) $(CFLAGS) -o $@ $<

sce_module/%.prx:
	@mkdir -p $(@D)
	cp $(SDK_SAMPLE)/$@ $@

sce_sys/about/right.sprx:
	@mkdir -p $(@D)
	cp $(SDK_SAMPLE)/$@ $@

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 $(CONTENT_ID).pkg sce_sys/param.sfo
