# m8c-ps4

[m8c](https://github.com/laamaa/m8c) (the Dirtywave M8 Headless client) as an installable PS4
app. Plug the M8 into the console's USB port: the M8 screen shows on the TV, audio plays through
the TV and/or the DualShock 4 speaker, and the controller drives the M8.

Developed and tested on PS4 6.72 with GoldHEN and an M8 Headless (Teensy 4.1, firmware 6.5.2).
Sister port: [m8c-ps5](https://github.com/sbauwow/m8c-ps5), which shares `app/src` byte for byte.

## Requirements

- A jailbroken PS4 with GoldHEN (tested on 6.72), with GoldHEN's FTP server on port 2121.
- Debug Settings enabled, for the Package Installer.
- An M8 Headless (or an M8 in headless mode) on a USB port of the PS4.

## Build (Linux host)

- OpenOrbis PS4 toolchain at `~/ps4-toolchain/OpenOrbis/PS4Toolchain` (or set `OO_PS4_TOOLCHAIN`).
- PkgTool needs .NET with invariant globalization plus OpenSSL 1.1. `scripts/deploy.sh` sets the
  environment.

```sh
scripts/deploy.sh [ps4-ip]
```

This rebuilds if anything changed, uploads the `.pkg` to `/data/pkg/` over FTP and verifies its
hash. Then install it on the PS4: **Settings → Debug Settings → Game → Package Installer**.
The **m8c PS4** tile appears on the home screen; launch it like any game.

Without an IP, the script finds the PS4 by its GoldHEN FTP banner. A PS5 also answers on port
2121, so always check that banner.

## Controls (DualShock 4)

| M8 key | DualShock 4 |
|---|---|
| Arrow keys | D-pad (or left stick) |
| SHIFT | **Share** (or **L2**) |
| PLAY | **Options** (or **R2**) |
| OPTION | **Circle** |
| EDIT | **Cross** |
| Cycle audio output | **Triangle** + Share/L2 |
| Reset display | **L3** + Share/L2 |
| **Quit to home screen** | **R3** + Share/L2 |

Square, L1, R1 and the right stick are unmapped. The PS button only goes to the home screen and
leaves m8c running.

### Remapping

Edit the `[gamepad]` section of `/data/m8c_config.ini` over FTP, then relaunch. Values are SDL
GameController button numbers:

| # | Button | # | Button |
|---|---|---|---|
| 0 | Cross | 8 | R3 |
| 1 | Circle | 9 | L1 |
| 2 | Square | 10 | R1 |
| 3 | Triangle | 11 | D-pad up |
| 4 | Share | 12 | D-pad down |
| 5 | PS | 13 | D-pad left |
| 6 | Options | 14 | D-pad right |
| 7 | L3 | | |

Analog axes (`gamepad_analog_axis_*`): 0 left X, 1 left Y, 2 right X, 3 right Y, 4 L2, 5 R2.
Use `-1` to disable an axis.

## Audio output

`audio_enabled=true` in `[audio]` turns on M8 audio capture over USB (44.1 kHz, resampled to
48 kHz). `audio_device_name` picks where it plays:

| Value | Output |
|---|---|
| `Default` | TV / system output. This includes a headset on the controller jack, following the PS4 sound settings. |
| `speaker` | DualShock 4 speaker only (mono mixdown) |
| `both` | TV and controller speaker |

Or switch while m8c runs: **Triangle + Share** cycles TV → controller speaker → both. A
notification shows the new mode, and the choice is saved to the config.

Known issue: on the DS4 headphone jack, audio has only been heard in the left ear.

## Files on the console

| Path | What |
|---|---|
| `/data/m8c_config.ini` | m8c config (controls, audio, `wait_for_device`, ...) |
| `/data/m8c_audio.ini` | optional USB iso tuning overrides (no reinstall needed) |
| `/data/m8c.log` | log: boot stages, USB/audio stats every 5 s, render fps |

Read the log with `curl ftp://<ps4-ip>:2121/data/m8c.log`. The first lines say which build is
running (`[stage] BUILD-C8`).

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Log says `Device not detected` | Unplug the M8 and plug it back in. m8c waits for it (`wait_for_device=true`). |
| Garbled screen / choppy audio | Check the log: `ringdrop` should stay 0 and `iso` should be ~176000 B/s with `under` 0. |
| PKG install fails with CE-32957-6 | The title ID must be `[A-Z]{4}[0-9]{5}` (here `MBCA00001`). |
| Upload looks fine but the old build runs | `deploy.sh` checks the hash. Re-run it and reinstall the package. |

More detail on the port, its pitfalls and its history is in `notes/RECON.md`.

## Layout

- `app/src/`: upstream m8c v1.7.10 sources with `PS4`/`PS5` guards, identical to m8c-ps5.
- `port/`: PS4 backends. sceUsbd handles the USB serial link and UAC2 audio capture
  (`usbio_ps4.c`). sceAudioOut handles TV and pad-speaker output. Also logging, notifications and
  exit-to-home.
- `probe/`: the pre-port USB probe app.
- `scripts/`: deploy, PS4 discovery, log fetch, M8 health check.
- `notes/RECON.md`: porting record.

## License

MIT, see [LICENSE](LICENSE). `app/src/` is upstream m8c under its own MIT license and bundled
notices ([app/src/LICENSE](app/src/LICENSE)).
