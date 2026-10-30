# ESP32 Animated Eyes

A pair of animated eyes on two 1.28" GC9A01 round displays, driven by an ESP32. The eyes look around and blink on their own, and you can take over with an analog joystick, a phone-friendly web control panel, or the serial monitor. A push button cycles through the eye designs.

## Features

- **6 eye styles:** Human, Cat, Robot, Dragon, Cartoon, Alien
- **6 expressions:** Normal, Happy, Sleepy, Angry, Sad, Surprised
- **Automatic behaviour:** random eye movements, natural blinking, lids that follow the gaze
- **Joystick:** move to aim, press to blink, long-press to re-centre
- **Mode button:** short press = next style, long press = auto movement on/off
- **Web control panel:** drag-the-iris look pad, style/expression buttons, blink/wink, settings, live status
- **Serial monitor:** full command set at 115200 baud (type `help`)
- **WiFi:** own access point plus optional home WiFi, reachable at `http://eyes.local`

## Hardware

- ESP32-S3 DevKit **or** classic ESP32 DevKit V1 / WROOM-32 (pins are picked automatically)
- 2 × GC9A01 1.28" 240×240 round IPS display (SPI)
- Analog joystick module (+5V, GND, VRx, VRy, SW)
- Momentary push button

## Wiring

| Connection | ESP32-S3 | ESP32 (WROOM-32) |
|---|---|---|
| Displays VCC / GND | 3.3V / GND | 3.3V / GND |
| Displays SCL / SDA | GPIO 12 / GPIO 11 | GPIO 18 / GPIO 23 |
| Displays DC / RST | GPIO 14 / GPIO 18 | GPIO 2 / GPIO 4 |
| Displays BL (if present) | 3.3V | 3.3V |
| Left display CS | GPIO 10 | GPIO 5 |
| Right display CS | GPIO 17 | GPIO 27 |
| Joystick +5V / GND | **3.3V** / GND | **3.3V** / GND |
| Joystick VRx / VRy | GPIO 1 / GPIO 2 | GPIO 34 / GPIO 35 |
| Joystick SW | GPIO 4 | GPIO 32 |
| Mode button | GPIO 5 + GND | GPIO 33 + GND |

> ⚠️ Power the joystick from **3.3V**, not 5V. At 5V its outputs can exceed 3.3V and damage the ESP32's ADC pins.

## Setup

1. Install **LovyanGFX** (by lovyan03) from the Arduino Library Manager. No setup file is needed – display pins are configured in the sketch.
2. Open `ESP32_Eyes/ESP32_Eyes.ino` (it's a single file, the web page is built in).
3. Select your board. Set **Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)**.
4. Upload, keeping the joystick still during boot while it calibrates.

If the board is stuck in a reboot loop and uploads fail: hold **BOOT**, tap **RESET**, release **BOOT**, then upload.

If colours look inverted set `DISPLAY_INVERT` to `false`; if the image is garbled lower `SPI_SPEED` to `27000000`.

## Web control panel

Join WiFi `ESP32-Eyes` (password `eyes1234`) and open `http://192.168.4.1`. Add your home WiFi in `STA_SSID` / `STA_PASS` to also use `http://eyes.local`.

A browser demo with simulated displays is in [`docs/web-panel-demo.html`](docs/web-panel-demo.html) – download it and open it in a browser.

## Serial commands

| Command | Action |
|---|---|
| `help` | List commands |
| `status` | Show current state |
| `style <n\|name>` | Choose style (e.g. `style cat`) |
| `next` / `prev` | Next / previous style |
| `expr <n\|name>` | Choose expression (e.g. `expr angry`) |
| `blink` / `wink <l\|r>` | Blink or wink |
| `look <x> <y>` | Aim eyes, -100..100 |
| `center` | Back to automatic |
| `auto <on\|off>` / `autoblink <on\|off>` | Automatic movement / blinking |
| `pupil <30..200>` | Pupil size % |
| `cal` / `joy` | Re-centre joystick / live joystick readout |
| `wifi` | Network info |

## Web API

| Endpoint | Purpose |
|---|---|
| `/api/status` | Current state as JSON |
| `/api/style?i=N` or `?dir=next\|prev` | Change style |
| `/api/expr?i=N` | Change expression |
| `/api/look?x=&y=` | Aim eyes (-1..1) |
| `/api/release` | Back to automatic |
| `/api/blink?eye=both\|left\|right` | Blink / wink |
| `/api/auto?move=0\|1&blink=0\|1` | Automatic modes |
| `/api/pupil?v=0.3..2.0` | Pupil size |
| `/api/calibrate` | Re-centre joystick |

## How it works

Both displays share the SPI bus and have separate chip-select pins driven by the sketch (LovyanGFX is configured with `pin_cs = -1`). To fit alongside WiFi without PSRAM, each eye is drawn into a 240×80 sprite three times – one horizontal band at a time – instead of a 115 KB full frame. Eyelids are curves with centre height, corner curvature and tilt; expressions are just different values for those curves.

## Configuration

All settings are at the top of `ESP32_Eyes.ino`: WiFi names and passwords, pins, `USE_JOYSTICK`, joystick invert/swap, `SWAP_EYES`, display rotation, `DISPLAY_INVERT`, `SPI_SPEED` and the manual hold time.

A full write-up is in [`docs/ESP32_Eyes_Project_Summary.docx`](docs/ESP32_Eyes_Project_Summary.docx).

## License

MIT – see [LICENSE](LICENSE).
