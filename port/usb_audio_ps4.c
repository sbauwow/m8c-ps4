// M8 UAC2 iso capture over sceUsbd + native PS4 audio output.
//
// Splits the old usb_audio_ps4.c in two halves:
//   CAPTURE (this file): claim iface 4 / alt 1, 8 iso transfers x 2 x 180B in
//   flight, callbacks push into a ring buffer, own event-pump thread services
//   sceUsbd (no internal thread like libusb).
//   OUTPUT (audio_native_ps4.c): raw sceAudioOut at 48k + resampler; SDL's
//   PS4 audio backend crashes the app on SDL_OpenAudioDevice, so it is NOT
//   used. The two files share state through usb_audio_shared.h.

#include <SDL.h>
#include <SDL_thread.h>
#include <errno.h>

#include "ringbuffer.h"
#include "usb_audio_shared.h"
#include "usb_ps4.h"
#include "ps4_shims.h"

// Iso constants (PACKET_SIZE etc.) come from usb_audio_shared.h.
#define EVENT_PUMP_TIMEOUT_MS 10

struct libusb_transfer *xfr[NUM_TRANSFERS];
Uint8 *xfr_buf[NUM_TRANSFERS];

volatile int audio_capturing = 0;
static volatile int stop_pump = 1; // no pump thread: main thread services events
static SDL_Thread *pump_thread = NULL;
RingBuffer *audio_ring = NULL;

static void iso_xfr_cb(struct libusb_transfer *x) {
  if (x->status != LIBUSB_TRANSFER_COMPLETED) {
    // CANCELLED is normal during teardown; NO_DEVICE means unplugged.
    if (x->status == LIBUSB_TRANSFER_CANCELLED || x->status == LIBUSB_TRANSFER_NO_DEVICE) {
      return;
    }
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "iso transfer status %d", x->status);
    return;
  }

  for (int i = 0; i < x->num_iso_packets; i++) {
    struct libusb_iso_packet_descriptor *pack = &x->iso_packet_desc[i];
    if (pack->status != LIBUSB_TRANSFER_COMPLETED) {
      SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "iso packet %d status %d", i, pack->status);
      continue;
    }
    if (pack->actual_length == 0 || audio_ring == NULL) {
      continue;
    }
    const uint8_t *data = x->buffer + i * PACKET_SIZE;
    int pushed = pack->actual_length;
    if (pushed > PACKET_SIZE) {
      pushed = PACKET_SIZE;
    }
    if (ring_buffer_push(audio_ring, data, pushed) == (uint32_t)-1) {
      SDL_LogDebug(SDL_LOG_CATEGORY_SYSTEM, "Audio ring overflow");
    }
  }

  if (audio_capturing && sceUsbdSubmitTransfer(x) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "iso resubmit failed");
  }
}

int start_pump(void) {
  // Single-threaded USB (see usb_audio_shared.h): the MAIN thread services
  // events via usbd_pump_events() once per rendered frame, so no pump thread
  // exists. stop_pump stays 0 the whole time the stream is armed.
  stop_pump = 0;
  return 0;
}

void usbd_pump_events(void) {
  // Non-blocking event service, called from the main loop once per frame.
  // NEVER call with zero armed transfers: HandleEventsTimeout crashes the
  // module then (console-proven). Callers must gate on audio_capturing.
  if (!stop_pump) {
    int32_t timeout_us = 0;
    sceUsbdHandleEventsTimeout(&timeout_us);
  }
}

void stop_pump_thread(void) { stop_pump = 1; }

int start_iso_stream(void) {
  int submitted = 0;
  for (int i = 0; i < NUM_TRANSFERS; i++) {
    ps4_stage("audio:alloc");
    xfr[i] = sceUsbdAllocTransfer(NUM_PACKETS);
    ps4_stage("audio:buf");
    xfr_buf[i] = SDL_malloc(PACKET_SIZE * NUM_PACKETS);
    if (xfr[i] == NULL || xfr_buf[i] == NULL) {
      ps4_logf("audio: alloc %d failed (xfr=%p buf=%p)", i, xfr[i], xfr_buf[i]);
      return -ENOMEM;
    }
    ps4_stage("audio:fill");
    sceUsbdFillIsoTransfer(xfr[i], g_devh, EP_ISO_IN, xfr_buf[i], PACKET_SIZE * NUM_PACKETS,
                           iso_xfr_cb, NULL, 0);
    // libusb's fill_iso_transfer sets transfer->type and num_iso_packets
    // internally; this port's fill may not — and submit dispatches on type.
    // LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 3 (control=0, bulk=1, interrupt=2).
    xfr[i]->type = 3;
    xfr[i]->num_iso_packets = NUM_PACKETS;
    ps4_stage("audio:setlen");
    sceUsbdSetIsoPacketLengths(xfr[i], PACKET_SIZE);
    if (i == 0) {
      ps4_stage("audio:submit1");
    }
    int rc = sceUsbdSubmitTransfer(xfr[i]);
    if (rc < 0) {
      ps4_logf("audio: submit %d -> %d", i, rc);
      // Roll back armed transfers; a half-armed stream wedges teardown.
      for (int j = 0; j < submitted; j++) {
        sceUsbdCancelTransfer(xfr[j]);
      }
      return -1;
    }
    if (i == 0) {
      ps4_stage("audio:submit1 ok");
    }
    submitted++;
  }
  audio_capturing = 1;
  return 0;
}

void teardown_iso_stream(void) {
  audio_capturing = 0;
  for (int i = 0; i < NUM_TRANSFERS; i++) {
    if (xfr[i] != NULL) {
      sceUsbdCancelTransfer(xfr[i]);
    }
  }
  // Let the pump process the cancellations before the module is touched again.
  SDL_Delay(50);
  stop_pump_thread();
  for (int i = 0; i < NUM_TRANSFERS; i++) {
    if (xfr[i] != NULL) {
      sceUsbdFreeTransfer(xfr[i]);
      xfr[i] = NULL;
    }
    if (xfr_buf[i] != NULL) {
      SDL_free(xfr_buf[i]);
      xfr_buf[i] = NULL;
    }
  }
}
