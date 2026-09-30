// Shared state between the PS4 CDC backend (usb_ps4.c) and the PS4 iso audio
// backend (usb_audio_ps4.c). Upstream m8c wires these through usb.h +
// `extern libusb_device_handle *devh`; we use one explicit header instead.
#ifndef USB_PS4_H_
#define USB_PS4_H_

#include <stdbool.h>
#include <stdint.h>
#include <orbis/Usbd.h>

#define M8_VID 0x16c0
#define M8_PID 0x048a

#define ACM_CTRL_DTR 0x01
#define ACM_CTRL_RTS 0x02

// CDC endpoints (fixed on M8/Teensy, same as upstream usb.c)
#define EP_OUT 0x03
#define EP_IN 0x83

// M8 UAC2 audio interface
#define AUDIO_IFACE 4
#define AUDIO_ALT_SETTING 1
#define EP_ISO_IN 0x85

// Handle to the opened M8; NULL when no device. Owned by usb_ps4.c.
extern libusb_device_handle *g_devh;

// Loads libSceUsbd.sprx + sceUsbdInit exactly once; safe from any thread.
bool ps4_usbd_ensure_init(void);

#endif
