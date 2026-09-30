// Dirtywave M8 CDC-ACM backend for PS4 (sceUsbd), replacing upstream m8c
// usb.c (libusb) / serial.c (libserialport). Sync bulk transfers only — the
// phase-0 probe proved every step of this sequence on 6.72 GoldHEN:
//   load libSceUsbd.sprx -> init -> open 16c0:048a -> claim 0+1 ->
//   SET_LINE_STATE -> SET_LINE_ENCODING -> bulk E/R/D/C/K.
// Iso audio lives in usb_audio_ps4.c.

#include <SDL.h>
#include <SDL_thread.h>
#include <stdbool.h>
#include <string.h>

#include <orbis/libkernel.h>

#include "serial.h"
#include "usb_audio_shared.h"
#include "usb_ps4.h"
#include "ps4_shims.h"

libusb_device_handle *g_devh = NULL;

static bool usbd_ready = false;
static SDL_mutex *init_lock = NULL;

static void init_lock_ensure(void) {
  if (init_lock == NULL) {
    init_lock = SDL_CreateMutex();
  }
}

bool ps4_usbd_ensure_init(void) {
  init_lock_ensure();
  SDL_LockMutex(init_lock);
  if (usbd_ready) {
    SDL_UnlockMutex(init_lock);
    return true;
  }
  int res = (int)sceKernelLoadStartModule("libSceUsbd.sprx", 0, NULL, 0, NULL, NULL);
  if (res < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "libSceUsbd.sprx load failed: 0x%08X", res);
    SDL_UnlockMutex(init_lock);
    return false;
  }
  if (sceUsbdInit() < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "sceUsbdInit failed");
    SDL_UnlockMutex(init_lock);
    return false;
  }
  usbd_ready = true;
  SDL_Log("sceUsbd initialised");
  SDL_UnlockMutex(init_lock);
  return true;
}

// SYNC bulk transfer. Console-proven matrix:
//   sync bulk + no iso armed               -> WORKS (display-only build)
//   sync bulk + iso armed (any gating)     -> HANGS
//   async bulk submit                      -> ok, but servicing its events
//                                             deadlocks the module (even
//                                             single-threaded, alloc(1))
// So bulk stays sync forever; iso events are only serviced once per frame by
// the main thread (see render_screen) and audio arming happens before the
// main loop, with bulk I/O quiescent until then. The remaining risk window is
// the FIRST sync bulk after audio init (m8c's glitch-avoidance reset) —
// mitigated by servicing events before it (see audio_init order in main).
// The toolchain's libusb.h has no error enum. sceUsbd mirrors libusb's codes
// as 0x802400xx; accept either spelling of TIMEOUT.
#define USBD_ERROR_TIMEOUT_LIBUSB (-7)
#define USBD_ERROR_TIMEOUT_SCE ((int)0x80240007)

static bool is_timeout(int rc) {
  return rc == USBD_ERROR_TIMEOUT_LIBUSB || rc == USBD_ERROR_TIMEOUT_SCE;
}

// Returns bytes transferred, or -1 on a real error. A timeout is NOT an error:
// libusb semantics report partial data in `sent` even then, and serial_read's
// 1 ms timeout expires whenever the M8 is quiet.
static int bulk_sync(int endpoint, unsigned char *buf, int len, unsigned int timeout_ms) {
  if (g_devh == NULL) {
    return -1;
  }
  int sent = 0;
  int rc = sceUsbdBulkTransfer(g_devh, endpoint, buf, len, &sent, timeout_ms);
  if (rc < 0 && !is_timeout(rc)) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "bulk 0x%02X: 0x%08X", endpoint, rc);
    return -1;
  }
  return sent;
}

static int bulk_write(const unsigned char *buf, int len, unsigned int timeout_ms) {
  return bulk_sync(EP_OUT, (unsigned char *)buf, len, timeout_ms);
}

int serial_read(uint8_t *serial_buf, int count) {
  // m8c's main loop treats <=0 as "idle tick". One sync attempt per call.
  return bulk_sync(EP_IN, (unsigned char *)serial_buf, count, 1);
}

int check_serial_port(void) {
  // Same as upstream's libusb backend: no probe read here (it would swallow
  // SLIP bytes). An unplug surfaces as a real error from serial_read.
  return g_devh != NULL;
}

int init_serial(int verbose, const char *preferred_device) {
  (void)verbose;
  (void)preferred_device; // single fixed device on PS4

  if (g_devh != NULL) {
    return 1;
  }
  if (!ps4_usbd_ensure_init()) {
    return 0;
  }

  g_devh = sceUsbdOpenDeviceWithVidPid(M8_VID, M8_PID);
  if (g_devh == NULL) {
    SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "No M8 (%04X:%04X) found", M8_VID, M8_PID);
    return 0;
  }

  int rc = sceUsbdSetConfiguration(g_devh, 1);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SetConfiguration(1): %d", rc);
    goto fail;
  }

  for (int iface = 0; iface <= 1; iface++) {
    rc = sceUsbdClaimInterface(g_devh, iface);
    if (rc < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "ClaimInterface(%d): %d", iface, rc);
      goto fail;
    }
  }

  rc = sceUsbdControlTransfer(g_devh, 0x21, 0x22, ACM_CTRL_DTR | ACM_CTRL_RTS, 0, NULL, 0, 500);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SET_CONTROL_LINE_STATE: %d", rc);
    goto fail;
  }

  unsigned char encoding[] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08}; // 115200 8N1
  rc = sceUsbdControlTransfer(g_devh, 0x21, 0x20, 0, 0, encoding, sizeof(encoding), 500);
  if (rc < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "SET_LINE_ENCODING: %d", rc);
    goto fail;
  }

  SDL_Log("M8 opened over sceUsbd");
  return 1;

fail:
  sceUsbdClose(g_devh);
  g_devh = NULL;
  return 0;
}

int list_devices() {
  if (!ps4_usbd_ensure_init()) {
    return 0;
  }
  libusb_device **list = NULL;
  int count = sceUsbdGetDeviceList(&list);
  int found = 0;
  for (int i = 0; i < count; i++) {
    struct libusb_device_descriptor d;
    if (sceUsbdGetDeviceDescriptor(list[i], &d) == 0 && d.idVendor == M8_VID &&
        d.idProduct == M8_PID) {
      found++;
    }
  }
  if (count >= 0) {
    sceUsbdFreeDeviceList(list);
  }
  SDL_Log("%d M8 device(s)", found);
  return found;
}

int reset_display() {
  unsigned char buf = 'R';
  if (bulk_write(&buf, 1, 5) != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error resetting M8 display");
    return 0;
  }
  return 1;
}

int enable_and_reset_display() {
  unsigned char buf = 'E';
  if (bulk_write(&buf, 1, 5) != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error enabling M8 display");
    return 0;
  }
  SDL_Delay(5);
  return reset_display();
}

int disconnect() {
  SDL_Log("Disconnecting M8");

  unsigned char buf = 'D';
  if (bulk_write(&buf, 1, 5) != 1) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Error sending disconnect");
    return -1;
  }

  if (g_devh != NULL) {
    for (int if_num = 0; if_num < 2; if_num++) {
      if (sceUsbdReleaseInterface(g_devh, if_num) < 0) {
        SDL_Log("Error releasing interface %d", if_num);
        return 0;
      }
    }
    sceUsbdClose(g_devh);
    g_devh = NULL;
  }

  // Deliberately no sceUsbdExit: the iso-audio backend shares this context and
  // the probe showed teardown here can wedge the module.
  return 1;
}

int send_msg_controller(uint8_t input) {
  unsigned char buf[2] = {'C', input};
  return bulk_write(buf, 2, 5);
}

int send_msg_keyjazz(uint8_t note, uint8_t velocity) {
  unsigned char buf[3] = {'K', note, velocity};
  return bulk_write(buf, 3, 5);
}
