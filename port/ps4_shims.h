// PS4-specific shims for m8c (see ps4_shims.c).
#ifndef PS4_SHIMS_H_
#define PS4_SHIMS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Mirror all SDL_Log output to /data/m8c.log. Call before anything else logs.
void ps4_log_init(void);

// Raw stage marker, independent of SDL_Log: writes "[stage] msg" to the log.
// Used to bisect boot hangs (CE-34878-0 gives no diagnostics).
void ps4_stage(const char *msg);

// Like ps4_stage, but each unique message is logged only the first time it is
// seen — safe to call every frame.
void ps4_stage_once(const char *msg);

// printf-style line directly to /data/m8c.log. SDL_Log does NOT reach the
// output function on this SDL2 build, so diagnostics must bypass it.
void ps4_logf(const char *fmt, ...);

// Replaces SDL_GetPrefPath("", file) on PS4: flat /data/m8c_<file>.
const char *ps4_pref_path(const char *filename);

#ifdef __cplusplus
}
#endif

#endif
