// v1 audio stubs: audio is parked (see notes/RECON.md "Audio campaign
// status"). These satisfy main.c's calls so the audio object files stay out
// of the binary entirely. Re-add usb_audio_ps4.c + audio_native_ps4.c (and
// remove this file from CFILES) when resuming audio work.

#include "audio.h"

int audio_init(unsigned int audio_buffer_size, const char *output_device_name) {
  (void)audio_buffer_size;
  (void)output_device_name;
  return 0;
}

void audio_destroy() {}

void toggle_audio(unsigned int audio_buffer_size, const char *output_device_name) {
  (void)audio_buffer_size;
  (void)output_device_name;
}
