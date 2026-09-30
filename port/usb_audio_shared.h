// Shared between usb_audio_ps4.c (iso capture) and audio_native_ps4.c
// (sceAudioOut output). One owner per symbol; header exists so the output
// side can arm/tear the capture stream without duplicating state.
#ifndef USB_AUDIO_SHARED_H_
#define USB_AUDIO_SHARED_H_

#include <SDL.h>
#include <stdint.h>

#include <orbis/Usbd.h>

#include "ringbuffer.h"

#define AUDIO_IFACE 4
#define AUDIO_ALT_SETTING 1
#define EP_ISO_IN 0x85
// Packet size: the PROBE measured sceUsbdGetMaxIsoPacketSize(0x85) = 48 on
// 6.72 (fw descriptors say 180). Arming 180-byte packets crashed the module
// inside sceUsbdSubmitTransfer (log: dies between setlen and submit1).
// 48B x 8k microframes/s = 384 KB/s, above the 176.4 KB/s the M8's 44.1k
// s16 stereo needs — bandwidth is fine.
#define PACKET_SIZE 48
#define NUM_PACKETS 2
#define NUM_TRANSFERS 8

extern struct libusb_transfer *xfr[NUM_TRANSFERS];
extern Uint8 *xfr_buf[NUM_TRANSFERS];
extern volatile int audio_capturing;
extern RingBuffer *audio_ring;

// Single-threaded USB: the MAIN thread services sceUsbd events once per
// frame via usbd_pump_events(). Rationale (all proven on console):
// - a pump THREAD servicing async bulk events deadlocks the module
// - async bulk submit needs alloc(1) + explicit type field
// - sync bulk hangs whenever iso transfers are armed
// => no second USB thread anywhere; iso events are serviced between frames,
//    all bulk I/O stays sync on the main thread.
void usbd_pump_events(void);

int start_pump(void);
void stop_pump_thread(void);
int start_iso_stream(void);
void teardown_iso_stream(void);

#endif
