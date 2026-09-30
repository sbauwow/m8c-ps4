// PS4 native audio output, replacing SDL_OpenAudioDevice (whose PS4 backend
// crashes the app on init — CE-34878-0 pinned to audio_init_enter with no
// return). Raw sceAudioOut, the path proven by the OpenOrbis audio-wav
// sample on this very console.
//
// Flow: the iso capture pump thread pushes M8 PCM (44.1k s16 stereo) into the
// ring buffer; THIS file owns a small consumer thread that resamples to the
// PS4's 48 kHz and calls sceAudioOutOutput (blocking — it IS the pacing).
// The audio_init/audio_destroy/toggle_audio contract is unchanged.

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_thread.h>

#include <orbis/AudioOut.h>
#include <orbis/_types/audio_out.h>
#include <orbis/UserService.h>

#include "ps4_shims.h"
#include "ringbuffer.h"
#include "usb_audio_shared.h"
#include "usb_ps4.h"

#define OUT_RATE 48000
#define OUT_GRANULARITY 256 // frames per sceAudioOutOutput call (~5.3 ms)
#define RING_BYTES (256 * 1024)

static volatile int audio_running = 0;
static SDL_Thread *out_thread = NULL;
static int32_t out_handle = -1;

// Linear resampler state: converts ring contents 44.1k -> 48k.
// rs_pos counts down per output frame; below zero => consume one input frame.
static double rs_pos = 0.0;
static int16_t rs_last_l = 0, rs_last_r = 0;

static int out_thread_fn(void *arg) {
  (void)arg;
  static int16_t frame_buf[OUT_GRANULARITY * 2];

  while (audio_running) {
    for (int f = 0; f < OUT_GRANULARITY; f++) {
      rs_pos -= 1.0;
      while (rs_pos < 0.0) {
        uint8_t tmp[4];
        uint32_t got = ring_buffer_pop(audio_ring, tmp, 4);
        if (got != 4) {
          // Starved: pad the rest of this buffer with silence and keep the
          // resampler position where it is.
          memset(frame_buf + f * 2, 0, (OUT_GRANULARITY - f) * 4);
          goto emit;
        }
        rs_last_l = (int16_t)(tmp[0] | (tmp[1] << 8));
        rs_last_r = (int16_t)(tmp[2] | (tmp[3] << 8));
        rs_pos += (double)44100 / OUT_RATE;
      }
      frame_buf[f * 2 + 0] = rs_last_l;
      frame_buf[f * 2 + 1] = rs_last_r;
    }
  emit:
    // Blocking: returns when the PREVIOUS buffer finished playing — the rate
    // control. s16 stereo is already the wire format here.
    if (sceAudioOutOutput(out_handle, frame_buf) < 0) {
      ps4_logf("audio: output failed");
      break;
    }
  }
  return 0;
}

int audio_init(unsigned int audio_buffer_size, const char *output_device_name) {
  (void)audio_buffer_size;
  (void)output_device_name;
  ps4_stage("audio: native init");

  if (g_devh == NULL) {
    ps4_logf("audio: no M8 handle");
    return -1;
  }

  if (sceAudioOutInit() != 0) {
    ps4_logf("audio: sceAudioOutInit failed");
    return -1;
  }
  ps4_stage("audio: outinit ok");

  OrbisUserServiceUserId user_id = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
  out_handle = sceAudioOutOpen(user_id, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, OUT_GRANULARITY,
                               OUT_RATE, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
  if (out_handle <= 0) {
    ps4_logf("audio: sceAudioOutOpen -> %d", out_handle);
    return -1;
  }
  ps4_stage("audio: outopen ok");

  audio_ring = ring_buffer_create(RING_BYTES);
  rs_pos = 0.0;

  // Iso capture: claim + arm the interface.
  int rc = sceUsbdClaimInterface(g_devh, AUDIO_IFACE);
  if (rc < 0) {
    ps4_logf("audio: claim(4) -> %d", rc);
    return rc;
  }
  rc = sceUsbdSetInterfaceAltSetting(g_devh, AUDIO_IFACE, AUDIO_ALT_SETTING);
  if (rc < 0) {
    ps4_logf("audio: altset(4,1) -> %d", rc);
    sceUsbdReleaseInterface(g_devh, AUDIO_IFACE);
    return rc;
  }
  ps4_stage("audio: iface armed");

  // Output thread first, so capture data has a consumer immediately.
  audio_running = 1;
  out_thread = SDL_CreateThread(out_thread_fn, "m8c_audio_out", NULL);
  if (out_thread == NULL) {
    ps4_logf("audio: out thread create failed");
    audio_running = 0;
    return -1;
  }

  // Event pump BEFORE arming transfers: once submitted, their completion
  // callbacks can only fire while someone services sceUsbd events.
  ps4_stage("audio: pump start");
  if (start_pump() != 0) {
    ps4_logf("audio: pump create failed");
    audio_running = 0;
    return -1;
  }

  ps4_stage("audio: arming iso");
  if (start_iso_stream() != 0) {
    ps4_logf("audio: iso stream failed");
    audio_running = 0;
    SDL_WaitThread(out_thread, NULL);
    out_thread = NULL;
    return -1;
  }
  ps4_stage("audio: running");
  return 1;
}

int audio_destroy() {
  if (!audio_running) {
    return -1;
  }
  audio_running = 0;

  teardown_iso_stream();

  if (out_thread != NULL) {
    SDL_WaitThread(out_thread, NULL);
    out_thread = NULL;
  }

  // Probe teardown trap: alt 0 BEFORE release.
  sceUsbdSetInterfaceAltSetting(g_devh, AUDIO_IFACE, 0);
  sceUsbdReleaseInterface(g_devh, AUDIO_IFACE);

  if (out_handle > 0) {
    sceAudioOutClose(out_handle);
    out_handle = -1;
  }
  if (audio_ring != NULL) {
    ring_buffer_free(audio_ring);
    audio_ring = NULL;
  }
  ps4_logf("audio: closed");
  return 1;
}

void toggle_audio(unsigned int audio_buffer_size, const char *output_device_name) {
  (void)audio_buffer_size;
  (void)output_device_name;
  ps4_logf("audio: toggle not implemented");
}
