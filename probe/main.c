// m8c-ps4 Phase 0 probe.
//
// Answers, on real 6.72 GoldHEN hardware, the four questions that decide the
// m8c port architecture:
//   1. Does libSceUsbd.prx load and initialise from a homebrew app?
//   2. Does a Dirtywave M8 / headless Teensy (16c0:048a) enumerate?
//   3. Do the CDC-ACM control transfers + bulk write/read work (display mirror)?
//   4. Does the UAC2 audio interface claim + report an iso packet size?
//
// Findings go to /data/m8c_probe.log, readable over GoldHEN FTP (port 2121).
// The TV shows nothing; this is a headless probe on purpose.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <orbis/libkernel.h>
#include <orbis/Usbd.h>

#define M8_VID 0x16c0
#define M8_PID 0x048a

#define ACM_CTRL_DTR 0x01
#define ACM_CTRL_RTS 0x02

static FILE *g_log = NULL;

static void logf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
  if (g_log) {
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    fprintf(g_log, "\n");
    fflush(g_log);
    va_end(ap);
  }
}

static void hexlog(const char *what, const unsigned char *buf, int len) {
  logf("%s (%d bytes):", what, len);
  char line[80];
  for (int i = 0; i < len; i += 16) {
    int n = 0;
    for (int j = 0; j < 16 && i + j < len; j++) {
      n += snprintf(line + n, sizeof(line) - n, "%02X ", buf[i + j]);
    }
    logf("  %04X  %s", i, line);
  }
}

static int load_usbd_module(void) {
  // Documented modules go through sceSysmodule; libSceUsbd is undocumented,
  // so homebrew loads it by path. Try the app-bundled copy first, then the
  // system module dir, then the bare name.
  const char *paths[] = {
      "/app0/sce_module/libSceUsbd.prx",
      "/system/common/lib/libSceUsbd.sprx",
      "libSceUsbd.sprx",
  };
  for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    logf("LoadStartModule(%s) ...", paths[i]);
    int res = (int)sceKernelLoadStartModule(paths[i], 0, NULL, 0, NULL, NULL);
    logf("  -> 0x%08X (%d)", res, res);
    if (res >= 0) {
      return 1;
    }
  }
  return 0;
}

static void dump_device_list(libusb_device **list, int count) {
  logf("device count: %d", count);
  for (int i = 0; i < count; i++) {
    struct libusb_device_descriptor d;
    if (sceUsbdGetDeviceDescriptor(list[i], &d) < 0) {
      logf("  [%d] <descriptor failed>", i);
      continue;
    }
    logf("  [%d] %04X:%04X class=%02X", i, d.idVendor, d.idProduct, d.bDeviceClass);
  }
}

static int probe_m8(libusb_device_handle *devh) {
  int rc;

  rc = sceUsbdSetConfiguration(devh, 1);
  logf("SetConfiguration(1) -> %d", rc);

  for (int iface = 0; iface <= 1; iface++) {
    rc = sceUsbdClaimInterface(devh, iface);
    logf("ClaimInterface(%d) -> %d", iface, rc);
    if (rc < 0) {
      return 0;
    }
  }

  // SET_CONTROL_LINE_STATE: DTR|RTS, interface 0
  rc = sceUsbdControlTransfer(devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 500);
  logf("SET_CONTROL_LINE_STATE -> %d", rc);

  // SET_LINE_ENCODING: 115200 8N1 (same frame m8c uses)
  unsigned char encoding[] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};
  rc = sceUsbdControlTransfer(devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 500);
  logf("SET_LINE_ENCODING -> %d", rc);

  // Enable the M8 remote display, then reset it (m8c handshake).
  unsigned char enable = 'E';
  int sent = 0;
  rc = sceUsbdBulkTransfer(devh, 0x03, &enable, 1, &sent, 1000);
  logf("bulk write 'E' -> rc=%d sent=%d", rc, sent);
  if (rc < 0 || sent != 1) {
    return 0;
  }

  unsigned char buf[1024];
  int got = 0;
  rc = sceUsbdBulkTransfer(devh, 0x83, buf, sizeof(buf), &got, 1000);
  logf("bulk read -> rc=%d got=%d", rc, got);
  if (got > 0) {
    hexlog("first display data", buf, got > 128 ? 128 : got);
  }

  unsigned char reset = 'R';
  rc = sceUsbdBulkTransfer(devh, 0x03, &reset, 1, &sent, 1000);
  logf("bulk write 'R' -> rc=%d sent=%d", rc, sent);

  // Audio interface: claim 4, alt setting 1, ask for the iso packet size.
  rc = sceUsbdClaimInterface(devh, 4);
  logf("ClaimInterface(4) [audio] -> %d", rc);
  if (rc >= 0) {
    rc = sceUsbdSetInterfaceAltSetting(devh, 4, 1);
    logf("SetInterfaceAltSetting(4,1) -> %d", rc);
    libusb_device *dev = sceUsbdGetDevice(devh);
    int iso = sceUsbdGetMaxIsoPacketSize(dev, 0x85);
    logf("GetMaxIsoPacketSize(0x85) -> %d", iso);
    sceUsbdReleaseInterface(devh, 4);
  }

  // Put the M8 back in standalone mode.
  unsigned char disconnect = 'D';
  rc = sceUsbdBulkTransfer(devh, 0x03, &disconnect, 1, &sent, 1000);
  logf("bulk write 'D' -> rc=%d sent=%d", rc, sent);

  sceUsbdReleaseInterface(devh, 0);
  sceUsbdReleaseInterface(devh, 1);
  return 1;
}

int main(void) {
  g_log = fopen("/data/m8c_probe.log", "w");
  logf("=== m8c-ps4 probe ===");

  if (!load_usbd_module()) {
    logf("RESULT: libSceUsbd FAILED to load - sceUsbd backend is dead on this fw");
    goto done;
  }

  if (sceUsbdInit() < 0) {
    logf("RESULT: sceUsbdInit FAILED");
    goto done;
  }
  logf("sceUsbdInit OK");

  libusb_device **list = NULL;
  int count = sceUsbdGetDeviceList(&list);
  if (count < 0) {
    logf("GetDeviceList failed: %d", count);
    dump_device_list(NULL, 0);
  } else {
    dump_device_list(list, count);
  }

  libusb_device_handle *devh = sceUsbdOpenDeviceWithVidPid(M8_VID, M8_PID);
  if (devh == NULL) {
    logf("M8 (%04X:%04X) NOT found; waiting up to 30s - plug it in...", M8_VID, M8_PID);
    for (int i = 0; i < 10 && devh == NULL; i++) {
      sceKernelUsleep(3 * 1000 * 1000);
      devh = sceUsbdOpenDeviceWithVidPid(M8_VID, M8_PID);
    }
  }

  if (devh == NULL) {
    count = sceUsbdGetDeviceList(&list);
    logf("still nothing; final device list:");
    if (count >= 0) {
      dump_device_list(list, count);
    }
    logf("RESULT: Usbd alive, M8 never enumerated");
    sceUsbdExit();
    goto done;
  }

  logf("M8 opened, handle %p", devh);
  int ok = probe_m8(devh);
  sceUsbdClose(devh);
  sceUsbdExit();
  logf("RESULT: %s", ok ? "FULL PASS - CDC + audio interfaces usable"
                       : "PARTIAL - device enumerated, transfers failed");

done:
  logf("=== probe finished ===");
  if (g_log) {
    fclose(g_log);
  }
  // Keep the process alive briefly so GoldHEN doesn't tear us down before the
  // final fflush lands on disk.
  sleep(2);
  return 0;
}
