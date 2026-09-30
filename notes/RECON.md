# m8c-ps4 recon (2026-09-29)

Port of laamaa/m8c (M8 headless client) to PS4 6.72 GoldHEN.
Target: display mirror + DS4 input + AUDIO (hard requirement) in v1.

## Why it's feasible

- Base: m8c v1.7.10 (last SDL2 release) — already proven to talk to M8 fw 6.5.2
  (laptop test during the Switch port; M8_V6_5_2C_HEADLESS.hex is in ~/m8-headless/Releases).
- OpenOrbis v0.5.3 toolchain works on Manjaro (see Build env below).
- libSceUsbd = libusb-1.0 clone, INCLUDING iso transfers (Usbd.h): FillIsoTransfer,
  SetIsoPacketLengths, GetIsoPacketBuffer, GetMaxIsoPacketSize(dev!, ep).
  => Both CDC-ACM serial and UAC2 audio are API-feasible. Better than Switch
  (usb:hs has no iso => that port is display-only).
- PS4 shows USB devices to userland as ugenX.Y (CTurt); sceUsbd is the access path.
  Unknown until probe runs: module load from homebrew on 6.72, enumeration of
  16c0:048a, control/bulk/iso behavior.

## M8 protocol constants (from m8c v1.7.10 + Switch port)

- VID:PID 16c0:048a, USB CDC-ACM.
- Serial: claim ifaces 0+1; control 0x21/0x22 SET_CONTROL_LINE_STATE
  wValue=DTR|RTS(0x03) if=0; control 0x21/0x20 SET_LINE_ENCODING, 7-byte body
  {00 C2 01 00 00 00 08} (115200 8N1).
- Bulk EPs 0x03 out / 0x83 in. One-byte commands: 'E' enable display,
  'R' reset display, 'D' disconnect, 'C'+u8 controller state,
  'K'+note+vel keyjazz. Reads: SLIP-framed draw commands (slip.c).
- Audio (audio_libusb.c): claim iface 4, alt setting 1, iso IN EP 0x85,
  PACKET_SIZE 180, NUM_PACKETS 2, NUM_TRANSFERS 64, resubmit in callback,
  256KB ring buffer, prebuffer 8KB, SDL stream S16 stereo 44100.

## Build env (Manjaro, proven)

- Toolchain: ~/ps4-toolchain/OpenOrbis/PS4Toolchain (v0.5.3, llvm-18.2 zip;
  zip contains toolchain-llvm-18.tar.gz — extract twice).
- PATH trap: theos clang-11 (iOS target) shadows /usr/bin — build scripts must
  pin CC=/usr/bin/clang, LD=/usr/bin/ld.lld (or PATH=/usr/bin:$PATH).
- PkgTool (.NET) needs BOTH: DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 AND
  LD_LIBRARY_PATH=~/ps5-jailbreak/ps4-transfer/pkgtool/openssl11/usr/lib
  (openssl-1.1; Manjaro ships 3.x).
- SDL2 is a STATIC lib (libSDL2.a) in the toolchain — no sprx needed; pkg
  bundles only libc.prx + libSceFios2.prx (+ right.sprx in sce_sys/about).
- Content ID must be 36 chars: IV0000-XXXXXXXXX_00-AAAAABBBBBBBBBB (last
  segment exactly 16). create-eboot is gone in v0.5.3: use create-fself
  --eboot "eboot.bin" --paid 0x3800000000000011.
- CE-32957-6 on install = SCE_BGFT_ERROR_INVALID_CONTENTID (psdevwiki
  0x80990055). NOT the IV0000/UP0001 prefix (the IV0000 hello_world sample
  installs fine). Real cause: TITLE_ID must match [A-Z]{4}[0-9]{5} - the
  author's "clever" M8CB00001 (digit in the letter zone) was rejected by
  BGFT. MBCB00001 is the fixed ID. Keep scene conventions: 4 letters + 5
  digits, cf. BREW00083 / PSNE00001.
- SFO compare helper: scripts/sfo_dump.py <file.sfo> (pkg_extractentry
  <pkg> 9 out.sfo first; PARAM_SFO is entry index 9).
- eboot PAID used by samples: 0x3800000000000011.
- link flags: --target=x86_64-pc-freebsd12-elf, link.x script, crt1.o,
  -lc -lkernel (+ -lSceUsbd for the probe).
- Usbd.h includes <libusb.h> => add -I$TOOLCHAIN/include/orbis/_types.
- sceUsbdGetMaxIsoPacketSize takes libusb_device* (sceUsbdGetDevice(devh)),
  NOT the handle. Bulk/interrupt take unsigned char* data (cast char buffers).
- libSceUsbd loading: not in SceSysmodule enum => sceKernelLoadStartModule by
  path: try /app0/sce_module/libSceUsbd.prx (if we can source a sprx),
  /system/common/lib/libSceUsbd.sprx, then bare "libSceUsbd.sprx".
  PROBE WILL TELL US WHICH WORKS.

## Console / deploy

- TARGET: PS4 6.72 GoldHEN at .199 (banner `GoldHEN FTP server v2.2`).
  CAUTION: PS5 11.60 also answers on 2121 (.106, banner `ftpsrv.elf v0.21`)
  - first probe upload went to the PS5 by mistake; always check the banner.
  A stray probe pkg (IV0000-M8CB00001_00-USBPROBE00000000) still sits in the
  PS5's /data/pkg/ - harmless, delete via PS5 Package Installer Options menu.
- PS4 /data/pkg/ may not exist: curl needs --ftp-create-dirs (first upload
  silently no-op'd without it - rc=0 but no file).
- FTP port 2121, binloader 9090.
- IP drifts: scan with scripts/find_ps4.py (2121/9090 sweep), read banner.
- Upload pkgs to /data/pkg/ then Settings → Debug Settings → Game →
  Package Installer. Same flow as Rocksmith FPKG.
- Probe writes /data/m8c_probe.log (plain fopen; Fios2 handles /data).

## Probe (phase 0) — RAN 2026-09-30, FULL PASS

- libSceUsbd loads via bare name "libSceUsbd.sprx" (app0 path and
  /system/common/lib path both 0x80020002 = ENOENT-ish). No sprx bundling.
- M8 16c0:048a enumerates + opens on PS4 front port. SetConfig(1), claim 0+1,
  SET_CONTROL_LINE_STATE, SET_LINE_ENCODING(7B), bulk 'E' all OK.
- M8 replied FF 00 06 05 02 00 C0 = fw 6.5.x version packet => m8c 1.7.10
  handshake compatible. Display mirror PROVEN.
- Audio iface 4 claim + alt setting 1 ACCEPTED. GetMaxIsoPacketSize(0x85)
  returned 48 (!) - descriptors say 180B. Either sceUsbd reads the wrong
  alt-setting descriptor, or 6.72 caps it. Handle in audio backend: trust
  ACTUAL per-packet lengths (iso_packet_desc), size buffers to max(48,180).
- TEARDOWN BUG (probe only): hang/crash after GetMaxIsoPacketSize - no
  ReleaseInterface(4), no 'D', no RESULT logged. Fix in real app: cancel
  transfers, SetInterfaceAltSetting(4,0) BEFORE ReleaseInterface(4), and on
  quit prefer sceUsbdClose+Exit over fussy per-iface releases. M8 likely left
  in display mode ('E' sent, 'D' never sent) - power-cycle or replug resets.

## Decision tree after probe

- Module fails to load → try GoldHEN plugin loader route, or give up on USB;
  fallback architecture: network bridge (Teensy/Pi on LAN, TCP→serial),
  PS4 app speaks TCP (net sample exists).
- Enum fails only with hub/dock → front ports direct.
- Transfers fail with proper enum → check 6.72 usbd quirks vs 9.00 docs.

## Audio campaign status (2026-09-30 session end)

Proven on console:
- iso arm + submit WORKS (alloc(1), transfer->type=3, num_iso_packets set,
  PACKET_SIZE 48) — audio: running + audio_init_return reached, twice.
- sceAudioOutInit + Open (MAIN port, 256 frames, 48k, S16 stereo) works.
- The M8 streams; display mirror + DS4 input are stable for hours (v1 shipped).

The wall (module-internal, repeatedly reproduced):
1. After iso transfers are ARMED, ANY sync sceUsbdBulkTransfer hangs forever
   (2ms/12ms/hold-release gating irrelevant; no other USB thread anywhere).
2. sceUsbdHandleEventsTimeout servicing async BULK events deadlocks (alloc(1),
   type=1 set, single-threaded too).
3. HandleEventsTimeout on the MAIN thread with zero armed transfers CRASHES
   non-deterministically (frame 1..N) — never call it with nothing armed.
4. Async bulk submit itself is fine (submit1 ok with alloc(1)); only its event
   servicing wedges.
=> No working completion mechanism for bulk or iso exists via HandleEvents*.
   Untested next lever: sceUsbdHandleEvents() (plain, blocking — real export
   in the sprx, 0x2186) and sceUsbdWaitForEvent. If plain HandleEvents works,
   frame-pump pattern becomes: arm iso -> per-frame HandleEvents (returns in
   ~1ms during streaming).
- Also never tested: arm iso with alt-setting applied AFTER submit, or
  SceUsbd via GoldHEN plugin kernel-side.

Test hygiene rules learned:
- Deleting /data/m8c_config.ini regenerates it with COMPILED defaults — audio
  builds must ship audio_enabled=false compiled AND flip config explicitly.
- ALWAYS check log mtime vs session start before reading results (stale logs
  burned 10 minutes twice).
- Crash-loops pollute module state: full reboot before A/B conclusions.

## C4 (2026-09-30): header-mismatch hypothesis + async engine

OpenOrbis Usbd.h disagrees with libusb 1.0 (which sceUsbd clones) in ways
that explain the "wall" above:
1. sceUsbdHandleEventsTimeout(int32_t *) — libusb takes struct timeval *
   (16 B). Old code passed an int32: 12 bytes of stack garbage as timeout
   => "deadlocks" (huge timeout) and random crashes.
2. sceUsbdFillIsoTransfer lacks libusb's num_iso_packets arg => callback /
   user_data / timeout possibly shifted. Now: never call Fill*, set fields.
3. type=3 (iso) / type=1 (bulk) were wrong: libusb enum is CONTROL=0,
   ISO=1, BULK=2, INTERRUPT=3.
port/usbio_ps4.c: one USB thread owns sceUsbd while audio runs; bulk IN/OUT +
iso all async; logs descriptors, 5 s stats, WATCHDOG lines. Tunables without
reinstall: /data/m8c_audio.ini (pkt= npkts= nxfers= ev_us=).

## C4 result (2026-09-30)

- Descriptors (fw 6.5.x, high speed): EP 0x85 iso IN is on INTERFACE 3 alt 1
  (maxpkt 48, interval 2 => 4000 pkt/s, 192 KB/s cap). Interface 4 alt 1 is
  the host->M8 stream (EP 0x05 + feedback 0x86). Upstream's IFACE_NUM 4 is
  wrong for this firmware => every iso transfer failed (status 1), and all
  earlier audio campaigns claimed the wrong interface too. 48 B is correct.
- USB thread stalled >2 s inside HandleEventsTimeout; M8 overflowed and the
  full-screen redraw was lost (grid gone, cursor still drawn). C5 times each
  events call and logs stats from the watchdog thread.

## C5 result (2026-09-30): AUDIO CAPTURE WORKS

- iso on interface 3: 176,400 B/s steady, 0 packet errors, 0 underruns,
  ring ~6-8 KB, HandleEventsTimeout max 2 ms (the C4 stall was the bad iface).
- Input lost: the first async bulk OUT (timeout 200) never called back, so
  tx_busy stuck and every later message queued forever. C6: timeout 0,
  stall detect -> cancel -> fall back to sync sends on the USB thread;
  `tx=sync` in /data/m8c_audio.ini forces the fallback.

## C6 result (2026-09-30)

- Every async bulk OUT hangs (actual 0) once audio is set up; cancel works
  (status 3). Bulk IN on 0x83 works. Grid showed boxes: the post-audio 'R'
  was the first stuck send.
- Hypothesis (C7): sceUsbd = FreeBSD libusb; SET alt interface tears down the
  kernel pipes of endpoints already opened, but libusb10 keeps its cached
  handle. 0x03 was used (sync E/R) BEFORE the alt switch; 0x83 only after.
  Also explains the old "sync bulk hangs once iso armed". C7 switches iface 3
  to alt 1 inside init_serial, before any bulk traffic.

## C7 result (2026-09-30): AUDIO + DISPLAY + INPUT ALL WORK

Alt-setting-before-bulk fixed OUT: tx idle, 0 stalls; iso 176.4 KB/s clean,
0 underruns, ring 6-8 KB, events max 2 ms. Audio heard on the DS4 headphone
jack — left channel only (open: headset mono? output routing?).
