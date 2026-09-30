// PS4 shims for m8c: file logging + pref-path replacement.
//
// CE-34878-0 on the PS4 gives no diagnostics, so ALL SDL_Log output is
// mirrored to /data/m8c.log (readable over GoldHEN FTP). SDL_GetPrefPath is
// not guaranteed to exist in the PS4 SDL2 port and m8c snprintf()s its config
// path from it; on PS4 we keep everything flat in /data instead.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "ps4_shims.h"

static FILE *g_logfile = NULL;

static void ps4_log_output(void *userdata, int category, SDL_LogPriority priority,
                           const char *message) {
  (void)userdata;
  (void)category;
  if (g_logfile) {
    fprintf(g_logfile, "%s\n", message);
    fflush(g_logfile);
  }
}

void ps4_log_init(void) {
  if (g_logfile) {
    return;
  }
  g_logfile = fopen("/data/m8c.log", "w");
  if (g_logfile) {
    fprintf(g_logfile, "=== m8c PS4 ===\n");
    fflush(g_logfile);
  }
  SDL_LogSetOutputFunction(ps4_log_output, NULL);
}

void ps4_stage(const char *msg) {
  if (g_logfile == NULL) {
    g_logfile = fopen("/data/m8c.log", "w");
    if (g_logfile) {
      fprintf(g_logfile, "=== m8c PS4 ===\n");
    }
  }
  if (g_logfile) {
    fprintf(g_logfile, "[stage] %s\n", msg);
    fflush(g_logfile);
  }
}

void ps4_stage_once(const char *msg) {
  static const char *seen[64];
  static int seen_count = 0;
  for (int i = 0; i < seen_count; i++) {
    if (seen[i] == msg) {
      return; // string literals: pointer identity is enough
    }
  }
  if (seen_count < 64) {
    seen[seen_count++] = msg;
  }
  ps4_stage(msg);
}

void ps4_logf(const char *fmt, ...) {
  if (g_logfile == NULL) {
    g_logfile = fopen("/data/m8c.log", "w");
    if (g_logfile) {
      fprintf(g_logfile, "=== m8c PS4 ===\n");
    }
  }
  if (g_logfile) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_logfile, fmt, ap);
    fprintf(g_logfile, "\n");
    fflush(g_logfile);
    va_end(ap);
  }
}

const char *ps4_pref_path(const char *filename) {
  static char path[512];
  snprintf(path, sizeof(path), "/data/m8c_%s", filename);
  return path;
}
