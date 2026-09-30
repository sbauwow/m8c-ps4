// Async USB engine for the PS4 port (sceUsbd). Used whenever audio is on.
//
// Why a separate engine: with iso transfers armed, sync sceUsbdBulkTransfer
// hangs (console-proven), so the display path cannot stay sync once audio
// runs. Here ONE thread owns every sceUsbd call from usbio_start() until
// usbio_stop() returns: bulk IN, bulk OUT and iso IN are all async, and that
// thread services completions with sceUsbdHandleEventsTimeout.
//
// Display-only mode never starts this engine and keeps the proven sync path
// in usb_ps4.c.
#ifndef USBIO_PS4_H_
#define USBIO_PS4_H_

#include <stdbool.h>
#include <stdint.h>

// Claims the audio interface (when with_audio), arms all transfers and starts
// the USB thread. Caller must not touch sceUsbd again until usbio_stop().
bool usbio_start(bool with_audio);

// Cancels everything, joins the USB thread, drops the audio alt setting.
// Idempotent. Queued writes are flushed first (bounded wait).
void usbio_stop(void);

bool usbio_active(void);

// Queue a CDC message for the M8. Returns len, or -1 when the engine is down.
int usbio_write(const uint8_t *buf, int len);

// Pop received CDC bytes. Returns count (0 when idle) or -1 if the M8 is gone.
int usbio_read(uint8_t *buf, int count);

// M8 PCM (44.1 kHz s16le stereo) for audio_native_ps4.c.
uint32_t usbio_audio_pop(uint8_t *buf, uint32_t len);
uint32_t usbio_audio_level(void);
void usbio_note_underrun(void);

#endif
