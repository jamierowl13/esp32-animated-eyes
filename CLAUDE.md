# CLAUDE.md – ESP32 Animated Eyes

Context for Claude Code. Read this before changing anything.

## What this is
Arduino sketch for an ESP32 (classic WROOM-32 / DevKit V1, no PSRAM) that drives two GC9A01
1.28" 240x240 round SPI displays as a pair of animated eyes. Controls: analog joystick, a mode
push button, a web control panel (ESP32 serves it), and serial commands at 115200 baud.

## Files
- `ESP32_Eyes/ESP32_Eyes.ino` – everything: config, styles/expressions tables, rendering,
  animation, buttons, joystick, serial parser, WiFi + WebServer routes.
- `ESP32_Eyes/web_page.h` – control panel HTML/CSS/JS in a PROGMEM raw string (`INDEX_HTML`).
- `ESP32_Eyes/User_Setup.h` – TFT_eSPI config. Must be copied into the TFT_eSPI library folder.
- `docs/web-panel-demo.html` – standalone browser demo that mirrors the firmware drawing code
  and fakes the `/api/*` endpoints. Keep it in sync if eye drawing or the panel changes.
- `docs/ESP32_Eyes_Project_Summary.docx` – write-up for humans.

## Hardware / pins (do not change without updating README, docs and User_Setup.h)
- Displays share SCLK 18, MOSI 23, DC 2, RST 4. CS: left = GPIO 5, right = GPIO 27.
- Joystick VRx 34, VRy 35 (ADC1 – required because ADC2 fails with WiFi on), SW 32 (pull-up).
- Mode button GPIO 33 to GND (pull-up).
- Joystick is powered from 3.3V, never 5V (ADC protection).

## Key design decisions
- **Dual display:** `TFT_CS` is `-1` in TFT_eSPI; the sketch toggles both CS pins manually.
  Both CS low during `tft.init()` so both panels initialise together.
- **Band rendering:** no full 115 KB frame buffer. One 240x80 16-bit sprite (38 KB) is drawn
  three times per eye with a y offset and pushed band by band. Keep RAM use low – WiFi needs it.
- **Buttons are polled between bands** in `renderEye()` so short presses aren't missed.
- **Eyelids** are drawn per 2-px column from curve parameters (top, kTop, tilt, bot, kBot).
  Expressions are rows in `EXPRS[]`; tilt is mirrored per eye (`SWAP_EYES` flips it).
- **Styles** are rows in `STYLES[]` with a `PupilType` that selects the iris drawing code.
- Manual input (joystick/web/serial) sets the gaze target and pauses auto movement for
  `MANUAL_HOLD_MS`.

## Adding things
- New eye style: add a `PupilType` value, a row in `STYLES[]`, and a case in `drawIris()`.
  Mirror it in `docs/web-panel-demo.html` (`STYLES` + `drawIris`).
- New expression: add a row to `EXPRS[]` (and the demo's `EXPRS`). Names appear in the web
  panel and serial commands automatically.
- New API endpoint: add `server.on(...)` in `setupWeb()`, document it in README.md.

## Build
- Arduino IDE or arduino-cli, board "ESP32 Dev Module". Library: TFT_eSPI (Bodmer).
- Prefer ESP32 board package 2.0.17; TFT_eSPI has known problems with core 3.x.
- Example: `arduino-cli compile --fqbn esp32:esp32:esp32 ESP32_Eyes`
- The code has not yet been compiled on real hardware – first job is to compile and fix
  any errors without changing behaviour.

## Conventions
- Keep all user-tunable settings in the USER CONFIGURATION block at the top of the .ino.
- Web panel uses system fonts only (it must work offline on the ESP32's access point).
- Never commit real WiFi credentials – `STA_SSID` / `STA_PASS` stay empty in the repo.
