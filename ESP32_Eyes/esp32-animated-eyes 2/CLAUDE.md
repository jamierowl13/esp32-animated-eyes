# CLAUDE.md – ESP32 Animated Eyes

Context for Claude Code. Read this before changing anything.

## What this is
Arduino sketch for an ESP32-S3 (the owner's board) or classic ESP32, no PSRAM, that drives two GC9A01
1.28" 240x240 round SPI displays as a pair of animated eyes. Controls: analog joystick, a mode
push button, a web control panel (ESP32 serves it), and serial commands at 115200 baud.

## Files
- `ESP32_Eyes/ESP32_Eyes.ino` – single-file sketch: config, LovyanGFX display class, styles/
  expressions tables, rendering, animation, buttons, joystick, serial parser, WiFi, WebServer
  routes and the embedded control panel page (`INDEX_HTML`).
- `docs/web-panel-demo.html` – standalone browser demo that mirrors the firmware drawing code
  and fakes the `/api/*` endpoints. Keep it in sync if eye drawing or the panel changes.
- `docs/ESP32_Eyes_Project_Summary.docx` – write-up for humans.

## Hardware / pins
Pins are chosen with `#if CONFIG_IDF_TARGET_ESP32S3` (see README for the table).
- ESP32-S3: SCLK 12, MOSI 11, DC 14, RST 18, CS left 10 / right 17, joystick VRx 1, VRy 2,
  SW 4, mode button 5. Avoid GPIO 19/20 (USB), 26-37 (flash/PSRAM), 0/3/45/46, 43/44.
- Classic ESP32: SCLK 18, MOSI 23, DC 2, RST 4, CS 5 / 27, VRx 34, VRy 35, SW 32, button 33.
- Joystick analog pins must be ADC1 (ADC2 fails with WiFi). Joystick powered from 3.3V.

## Key design decisions
- **Display library:** LovyanGFX, NOT TFT_eSPI. TFT_eSPI crashes in `tft.init()` on ESP32-S3
  with board package 3.x (StoreProhibited, EXCVADDR 0x10). Do not switch back.
- **Dual display:** LovyanGFX panel `pin_cs = -1`; the sketch toggles both CS pins manually.
  Both CS low during `tft.init()` so both panels initialise together. DMA is off and
  `tft.waitDisplay()` is called before CS is released.
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
- Arduino IDE (owner uses 1.8.19) or arduino-cli. Board "ESP32S3 Dev Module",
  partition scheme "Huge APP". Library: LovyanGFX. ESP32 board package 3.x.
- Example: `arduino-cli compile --fqbn esp32:esp32:esp32s3:PartitionScheme=huge_app ESP32_Eyes`
- Not yet confirmed running on real hardware – if asked to debug, first ask for serial output.

## Conventions
- Keep all user-tunable settings in the USER CONFIGURATION block at the top of the .ino.
- Web panel uses system fonts only (it must work offline on the ESP32's access point).
- Never commit real WiFi credentials – `STA_SSID` / `STA_PASS` stay empty in the repo.
