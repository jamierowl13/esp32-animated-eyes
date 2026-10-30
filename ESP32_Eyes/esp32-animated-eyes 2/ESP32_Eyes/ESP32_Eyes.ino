/*
  ============================================================================
   ESP32 Animated Eyes  -  2x GC9A01 1.28" 240x240 round IPS displays
  ============================================================================
   Features
     - 6 eye styles: Human, Cat, Robot, Dragon, Cartoon, Alien
     - 6 expressions: Normal, Happy, Sleepy, Angry, Sad, Surprised
     - Automatic random eye movement + automatic blinking
     - Analog joystick (VRx / VRy / SW) for manual eye control
     - Push button to switch eye styles
     - Serial monitor command interface (115200 baud, type "help")
     - Web control panel (WiFi access point + optional home WiFi)

   Works on ESP32 and ESP32-S3, board package 2.0.x or 3.x.
   Single-file sketch: only ESP32_Eyes.ino is needed in the sketch folder.

   Libraries (Arduino Library Manager)
     - LovyanGFX by lovyan03 (display driver - no setup file needed,
       display pins are set in the LGFX class below)
     - WiFi, WebServer, ESPmDNS come with the ESP32 board package

   WIRING - the sketch picks the right pins automatically for the board selected
   ----------------------------------------------------------------------------
                         ESP32-S3 DevKit        ESP32 DevKit V1 (WROOM-32)
   Displays (both)  VCC  3.3V                   3.3V
                    GND  GND                    GND
                    SCL  GPIO 12                GPIO 18
                    SDA  GPIO 11                GPIO 23
                    DC   GPIO 14                GPIO 2
                    RST  GPIO 18                GPIO 4
                    BL   3.3V (if present)      3.3V (if present)
   Left display     CS   GPIO 10                GPIO 5
   Right display    CS   GPIO 17                GPIO 27
   Joystick         +5V  3.3V  (!! NOT 5V - ADC pins only tolerate 3.3V !!)
                    GND  GND                    GND
                    VRx  GPIO 1                 GPIO 34
                    VRy  GPIO 2                 GPIO 35
                    SW   GPIO 4                 GPIO 32
   Mode button           GPIO 5 + GND           GPIO 33 + GND

   Use the board's USB port for upload/serial. On the ESP32-S3 avoid GPIO 19/20
   (USB), 26-37 (flash/PSRAM), 0/3/45/46 (boot strapping) and 43/44 (UART0).

   Controls
     Mode button   short press = next eye style, long press = auto-move on/off
     Joystick      move = look around (returns to auto 3 s after release)
     Joystick SW   short press = blink, long press = re-centre joystick
  ============================================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================ USER CONFIGURATION ============================
const char* AP_SSID   = "ESP32-Eyes";   // the ESP32's own WiFi network
const char* AP_PASS   = "eyes1234";     // at least 8 characters
const char* STA_SSID  = "";             // your home WiFi (leave "" to skip)
const char* STA_PASS  = "";
const char* MDNS_NAME = "eyes";         // -> http://eyes.local

#if CONFIG_IDF_TARGET_ESP32S3            // ---- ESP32-S3 pins ----
  #define TFT1_CS        10    // left eye chip select
  #define TFT2_CS        17    // right eye chip select
  #define JOY_X_PIN      1     // ADC1
  #define JOY_Y_PIN      2     // ADC1
  #define JOY_SW_PIN     4
  #define MODE_BTN_PIN   5
#else                                    // ---- classic ESP32 pins ----
  #define TFT1_CS        5
  #define TFT2_CS        27
  #define JOY_X_PIN      34
  #define JOY_Y_PIN      35
  #define JOY_SW_PIN     32
  #define MODE_BTN_PIN   33
#endif

#define USE_JOYSTICK   true   // set false if no joystick is connected
#define JOY_INVERT_X   false  // flip if eyes move the wrong way
#define JOY_INVERT_Y   false
#define JOY_SWAP_XY    false  // set true if the joystick is mounted rotated 90 deg
#define SWAP_EYES      false  // set true if left/right expressions look mirrored
#define LEFT_ROTATION  0      // 0..3, rotate each display if mounted turned
#define RIGHT_ROTATION 0
#define DISPLAY_INVERT true    // set false if colours look inverted (negative)
#define SPI_SPEED      40000000 // lower to 27000000 if the image is garbled

const float    JOY_DEADZONE   = 0.12f;
const uint32_t MANUAL_HOLD_MS = 3000;   // time before auto movement resumes
const uint32_t LONG_PRESS_MS  = 700;
const uint32_t BLINK_MS       = 220;
// ===========================================================================

#define RGB(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// ------------------------- Display driver (LovyanGFX) ----------------------
// Both displays share one SPI bus. pin_cs is -1 because the sketch switches
// the two chip-select pins itself (csSelect / csRelease).
#if CONFIG_IDF_TARGET_ESP32S3
  #define TFT_SCLK_PIN 12
  #define TFT_MOSI_PIN 11
  #define TFT_DC_PIN   14
  #define TFT_RST_PIN  18
#else
  #define TFT_SCLK_PIN 18
  #define TFT_MOSI_PIN 23
  #define TFT_DC_PIN   2
  #define TFT_RST_PIN  4
#endif

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI      _bus;
public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = SPI_SPEED;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = true;
      cfg.use_lock    = true;
      cfg.dma_channel = 0;            // no DMA: each band finishes before CS is released
      cfg.pin_sclk    = TFT_SCLK_PIN;
      cfg.pin_mosi    = TFT_MOSI_PIN;
      cfg.pin_miso    = -1;
      cfg.pin_dc      = TFT_DC_PIN;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs           = -1;
      cfg.pin_rst          = TFT_RST_PIN;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 240;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.readable         = false;
      cfg.invert           = DISPLAY_INVERT;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

const uint16_t C_BLACK = 0x0000;
const uint16_t C_WHITE = 0xFFFF;

LGFX         tft;
LGFX_Sprite  spr(&tft);
WebServer    server(80);

// The eye is rendered in horizontal bands so the frame buffer stays small
// (240 x 80 x 2 bytes = 38 KB) and fits alongside WiFi on any ESP32.
const int SCR    = 240;
const int BAND_H = 80;
const int BANDS  = SCR / BAND_H;
const float CLOSE_Y = 124.0f;           // where the lids meet when closed

// ------------------------------ Eye styles ---------------------------------
enum PupilType : uint8_t { P_ROUND, P_SLIT, P_ROBOT, P_DRAGON, P_CARTOON, P_ALIEN };

struct EyeStyle {
  const char* name;
  uint16_t sclera, irisOut, irisIn, pupil, lid;
  uint8_t  irisR, pupilR;
  PupilType type;
};

const EyeStyle STYLES[] = {
  { "Human",   RGB(245,240,232), RGB( 25, 70,150), RGB(110,190,235), RGB(  5,  5, 10), RGB(214,160,128), 52, 20, P_ROUND   },
  { "Cat",     RGB(230,225,200), RGB(110,140, 20), RGB(235,210, 50), RGB(  0,  0,  0), RGB( 95, 80, 65), 86, 12, P_SLIT    },
  { "Robot",   RGB( 12, 16, 24), RGB(  0,210,255), RGB(  0, 45, 70), RGB(190,255,255), RGB( 75, 82, 95), 62, 16, P_ROBOT   },
  { "Dragon",  RGB( 70, 12,  0), RGB(150, 20,  0), RGB(255,215, 60), RGB(  0,  0,  0), RGB( 40, 75, 35), 96,  7, P_DRAGON  },
  { "Cartoon", RGB(255,255,255), RGB(  0,  0,  0), RGB(  0,  0,  0), RGB(  0,  0,  0), RGB(255,205,175), 56,  0, P_CARTOON },
  { "Alien",   RGB(  6, 18,  8), RGB( 20,110, 40), RGB(160,255,120), RGB(  0,  0,  0), RGB( 60,115, 70), 74, 26, P_ALIEN   },
};
const int NUM_STYLES = sizeof(STYLES) / sizeof(STYLES[0]);

// ------------------------------ Expressions --------------------------------
// top/bot   = lid edge Y at the centre of the eye (0 = top of screen)
// kTop/kBot = how much the lid edge drops/rises at the corners (px)
// tilt      = slope of the upper lid (inner corner down = angry)
struct Expression { const char* name; float top, kTop, tilt, bot, kBot, pupil; };

const Expression EXPRS[] = {
  { "Normal",     10, 40,  0.00f, 232,  30, 1.00f },
  { "Happy",      18, 40,  0.00f, 150, -60, 1.05f },
  { "Sleepy",     92, 12,  0.00f, 226,  25, 0.95f },
  { "Angry",      48, 18,  0.42f, 222,  25, 0.80f },
  { "Sad",        42, 22, -0.38f, 226,  28, 1.10f },
  { "Surprised", -25, 20,  0.00f, 250,  20, 0.60f },
};
const int NUM_EXPRS = sizeof(EXPRS) / sizeof(EXPRS[0]);

// ------------------------------ Runtime state ------------------------------
int   styleIdx = 0, exprIdx = 0;
bool  autoMove = true, autoBlink = true;
float gazeX = 0, gazeY = 0, targetX = 0, targetY = 0;
float pupilUser = 1.0f;
const char* controlSource = "auto";

struct LidState { float top, kTop, tilt, bot, kBot, pupil; } lid;

uint32_t manualUntil = 0, lastJoyMove = 0, nextSaccade = 0, nextBlink = 0;
uint32_t blinkStart[2] = {0, 0};
bool     blinking[2]   = {false, false};

int   joyCX = 2048, joyCY = 2048, joyRawX = 0, joyRawY = 0;
float joyX = 0, joyY = 0;
bool  joyDebug = false;
uint32_t lastJoyPrint = 0;

String   overlayText;
uint32_t overlayUntil = 0;

float    fps = 0;
uint32_t fpsFrames = 0, fpsTimer = 0, lastFrame = 0;

String   serialBuf;
uint32_t serialLastChar = 0;

float COS48[48], SIN48[48];

// ------------------------------ Buttons ------------------------------------
struct Button {
  uint8_t  pin;
  bool     stable = HIGH, lastRead = HIGH, longFired = false;
  uint32_t lastChange = 0, downAt = 0;
  explicit Button(uint8_t p) : pin(p) {}
  // returns 0 = nothing, 1 = short press, 2 = long press
  int update(uint32_t now) {
    bool r = digitalRead(pin);
    if (r != lastRead) { lastRead = r; lastChange = now; }
    int ev = 0;
    if (now - lastChange > 25 && r != stable) {
      stable = r;
      if (stable == LOW) { downAt = now; longFired = false; }
      else if (!longFired) ev = 1;
    }
    if (stable == LOW && !longFired && now - downAt > LONG_PRESS_MS) { longFired = true; ev = 2; }
    return ev;
  }
};
Button modeBtn(MODE_BTN_PIN);
Button joyBtn(JOY_SW_PIN);

// ============================== Helpers =====================================
uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)(((int)(ar + (br - ar) * t) << 11) |
                    ((int)(ag + (bg - ag) * t) << 5)  |
                     (int)(ab + (bb - ab) * t));
}

inline void csSelect(int eye) { digitalWrite(eye == 0 ? TFT1_CS : TFT2_CS, LOW); }
inline void csRelease()       { digitalWrite(TFT1_CS, HIGH); digitalWrite(TFT2_CS, HIGH); }

void showOverlay(const String& txt) { overlayText = txt; overlayUntil = millis() + 1300; }

// ============================== Actions =====================================
void setStyle(int i) {
  styleIdx = (i % NUM_STYLES + NUM_STYLES) % NUM_STYLES;
  showOverlay(STYLES[styleIdx].name);
  Serial.printf("[STYLE] %d - %s\n", styleIdx, STYLES[styleIdx].name);
}
void stepStyle(int dir) { setStyle(styleIdx + dir); }

void setExpression(int i) {
  exprIdx = (i % NUM_EXPRS + NUM_EXPRS) % NUM_EXPRS;
  showOverlay(EXPRS[exprIdx].name);
  Serial.printf("[EXPR] %d - %s\n", exprIdx, EXPRS[exprIdx].name);
}

void startBlink(uint8_t mask) {           // bit0 = left, bit1 = right
  uint32_t now = millis();
  for (int e = 0; e < 2; e++)
    if (mask & (1 << e)) { blinking[e] = true; blinkStart[e] = now; }
}

float blinkAmount(int eye, uint32_t now) {
  if (!blinking[eye]) return 0;
  float t = (now - blinkStart[eye]) / (float)BLINK_MS;
  if (t >= 1) { blinking[eye] = false; return 0; }
  return t < 0.4f ? t / 0.4f : (1.0f - t) / 0.6f;   // close fast, open slower
}

void lookAt(float x, float y, const char* src) {
  float m = sqrtf(x * x + y * y);
  if (m > 1) { x /= m; y /= m; }
  targetX = x; targetY = y;
  manualUntil = millis() + MANUAL_HOLD_MS;
  lastJoyMove = 0;                        // web/serial overrides joystick
  controlSource = src;
}

void releaseManual() { manualUntil = 0; lastJoyMove = 0; nextSaccade = 0; }

void setAutoMove(bool on) {
  autoMove = on;
  Serial.printf("[AUTO] movement %s\n", on ? "ON" : "OFF");
  showOverlay(on ? "Auto on" : "Auto off");
}

void calibrateJoystick() {
#if USE_JOYSTICK
  long sx = 0, sy = 0;
  for (int i = 0; i < 32; i++) { sx += analogRead(JOY_X_PIN); sy += analogRead(JOY_Y_PIN); delay(2); }
  joyCX = sx / 32; joyCY = sy / 32;
  Serial.printf("[JOY] centre calibrated: x=%d y=%d\n", joyCX, joyCY);
#endif
}

// ============================== Inputs ======================================
void pollButtons(uint32_t now) {
  int m = modeBtn.update(now);
  if (m == 1) stepStyle(+1);
  else if (m == 2) setAutoMove(!autoMove);

  int j = joyBtn.update(now);
  if (j == 1) { startBlink(3); Serial.println("[JOY] blink"); }
  else if (j == 2) { calibrateJoystick(); showOverlay("Centred"); }
}

void readJoystick(uint32_t now) {
#if USE_JOYSTICK
  int rx = 0, ry = 0;
  for (int i = 0; i < 4; i++) { rx += analogRead(JOY_X_PIN); ry += analogRead(JOY_Y_PIN); }
  rx /= 4; ry /= 4;
  joyRawX = rx; joyRawY = ry;

  float x = rx >= joyCX ? (float)(rx - joyCX) / max(1, 4095 - joyCX) : (float)(rx - joyCX) / max(1, joyCX);
  float y = ry >= joyCY ? (float)(ry - joyCY) / max(1, 4095 - joyCY) : (float)(ry - joyCY) / max(1, joyCY);
  if (JOY_SWAP_XY)  { float t = x; x = y; y = t; }
  if (JOY_INVERT_X) x = -x;
  if (JOY_INVERT_Y) y = -y;

  float m = sqrtf(x * x + y * y);
  if (m < JOY_DEADZONE) { x = 0; y = 0; }
  else {
    float s = (min(m, 1.0f) - JOY_DEADZONE) / (1.0f - JOY_DEADZONE) / m;
    x *= s; y *= s;
  }
  joyX = x; joyY = y;

  if (x != 0 || y != 0) lastJoyMove = now;
  if (lastJoyMove && now - lastJoyMove < MANUAL_HOLD_MS) {
    targetX = x; targetY = y;
    manualUntil = lastJoyMove + MANUAL_HOLD_MS;
    controlSource = "joystick";
  }

  if (joyDebug && now - lastJoyPrint > 200) {
    lastJoyPrint = now;
    Serial.printf("[JOY] raw x=%4d y=%4d  ->  x=%+.2f y=%+.2f  sw=%s\n",
                  rx, ry, x, y, digitalRead(JOY_SW_PIN) == LOW ? "DOWN" : "up");
  }
#endif
}

// ============================== Animation ===================================
void updateAnimation(uint32_t now, float dt) {
  // automatic eye movement (saccades)
  if (now > manualUntil) {
    controlSource = autoMove ? "auto" : "hold";
    if (autoMove && now >= nextSaccade) {
      if (random(100) < 25) { targetX = 0; targetY = 0; }
      else {
        float a = random(0, 6283) / 1000.0f;
        float r = sqrtf(random(0, 1000) / 1000.0f) * 0.85f;
        targetX = cosf(a) * r;
        targetY = sinf(a) * r * 0.8f;
      }
      nextSaccade = now + random(500, 3500);
    }
  }
  float fg = 1.0f - expf(-dt / 40.0f);
  gazeX += (targetX - gazeX) * fg;
  gazeY += (targetY - gazeY) * fg;

  // automatic blinking
  if (autoBlink && now >= nextBlink) {
    startBlink(3);
    nextBlink = now + (random(100) < 15 ? 300 : random(2000, 6000));   // sometimes double-blink
  }

  // ease the eyelids toward the current expression
  const Expression& e = EXPRS[exprIdx];
  float fl = 1.0f - expf(-dt / 90.0f);
  lid.top   += (e.top   - lid.top)   * fl;
  lid.kTop  += (e.kTop  - lid.kTop)  * fl;
  lid.tilt  += (e.tilt  - lid.tilt)  * fl;
  lid.bot   += (e.bot   - lid.bot)   * fl;
  lid.kBot  += (e.kBot  - lid.kBot)  * fl;
  lid.pupil += (e.pupil - lid.pupil) * fl;
}

// ============================== Drawing =====================================
void gradDisc(int x, int y, int R, uint16_t outer, uint16_t inner) {
  int step = max(3, R / 14);
  for (int r = R; r > 0; r -= step)
    spr.fillCircle(x, y, r, lerp565(outer, inner, 1.0f - (float)r / R));
}

void spokes(int x, int y, int r0, int r1, uint16_t col, int every) {
  for (int i = 0; i < 48; i += every)
    spr.drawLine(x + COS48[i] * r0, y + SIN48[i] * r0, x + COS48[i] * r1, y + SIN48[i] * r1, col);
}

void highlights(int ix, int iy, int R) {
  spr.fillCircle(ix - R * 0.35f, iy - R * 0.40f, max(3, (int)(R * 0.16f)), C_WHITE);
  spr.fillCircle(ix + R * 0.28f, iy + R * 0.26f, max(2, (int)(R * 0.07f)), C_WHITE);
}

void drawIris(const EyeStyle& s, int ix, int iy, float ps, uint32_t now) {
  int R  = s.irisR;
  int pr = max(2, (int)(s.pupilR * ps));

  switch (s.type) {
    case P_ROUND: {                                    // human
      gradDisc(ix, iy, R, s.irisOut, s.irisIn);
      spokes(ix, iy, pr + 3, R - 4, lerp565(s.irisOut, C_BLACK, 0.3f), 2);
      uint16_t ring = lerp565(s.irisOut, C_BLACK, 0.6f);
      for (int i = 0; i < 3; i++) spr.drawCircle(ix, iy, R - i, ring);
      spr.fillCircle(ix, iy, pr, s.pupil);
      highlights(ix, iy, R);
    } break;

    case P_SLIT: {                                     // cat
      gradDisc(ix, iy, R, s.irisOut, s.irisIn);
      spokes(ix, iy, 10, R - 6, lerp565(s.irisOut, s.irisIn, 0.4f), 3);
      spr.drawCircle(ix, iy, R, lerp565(s.irisOut, C_BLACK, 0.5f));
      spr.fillEllipse(ix, iy, max(3, pr), R - 8, s.pupil);
      highlights(ix, iy, R * 0.8f);
    } break;

    case P_DRAGON: {
      gradDisc(ix, iy, R, s.irisOut, s.irisIn);
      spokes(ix, iy, 12, R - 3, lerp565(s.irisOut, C_BLACK, 0.2f), 2);
      for (int i = 0; i < 3; i++) spr.drawCircle(ix, iy, R - i, RGB(40, 0, 0));
      spr.fillEllipse(ix, iy, max(3, pr), R - 10, s.pupil);
      spr.fillCircle(ix - R * 0.3f, iy - R * 0.45f, 6, C_WHITE);
    } break;

    case P_ROBOT: {
      float rot = now * 0.0015f;
      for (int i = 0; i < 16; i++) {                   // rotating outer ticks
        float a = rot + i * (PI / 8);
        float c = cosf(a), sn = sinf(a);
        spr.drawLine(ix + c * (R + 6), iy + sn * (R + 6), ix + c * (R + 15), iy + sn * (R + 15), s.irisOut);
        spr.drawLine(ix + c * (R + 6) + 1, iy + sn * (R + 6), ix + c * (R + 15) + 1, iy + sn * (R + 15), s.irisOut);
      }
      for (int i = 0; i < 4; i++) spr.drawCircle(ix, iy, R - i, s.irisOut);
      spr.fillCircle(ix, iy, R - 8, s.irisIn);
      spr.drawCircle(ix, iy, R * 0.62f, s.irisOut);
      spr.drawFastHLine(ix - R + 8, iy, R / 3, s.irisOut);
      spr.drawFastHLine(ix + R - 8 - R / 3, iy, R / 3, s.irisOut);
      spr.drawFastVLine(ix, iy - R + 8, R / 3, s.irisOut);
      spr.drawFastVLine(ix, iy + R - 8 - R / 3, R / 3, s.irisOut);
      spr.fillCircle(ix, iy, pr + 5, s.irisOut);
      spr.fillCircle(ix, iy, pr, s.pupil);
    } break;

    case P_CARTOON: {
      int r = R * ps;
      spr.fillCircle(ix, iy, r, C_BLACK);
      spr.fillCircle(ix - r * 0.35f, iy - r * 0.35f, r * 0.28f, C_WHITE);
      spr.fillCircle(ix + r * 0.32f, iy + r * 0.30f, r * 0.12f, C_WHITE);
    } break;

    case P_ALIEN: {
      gradDisc(ix, iy, R, s.irisOut, s.irisIn);
      spr.drawCircle(ix, iy, R, RGB(0, 40, 10));
      spr.fillCircle(ix, iy, pr + 4, RGB(220, 255, 200));
      spr.fillCircle(ix, iy, pr, s.pupil);
      spr.fillEllipse(ix - R * 0.35f, iy - R * 0.42f, R * 0.22f, R * 0.11f, C_WHITE);
    } break;
  }
}

// Eyelids are drawn column by column: for each x the lid edge is a curve
//   upper: y = top + kTop*(dx/120)^2 + tilt*dx      (rows above are covered)
//   lower: y = bot - kBot*(dx/120)^2                (rows below are covered)
void drawLids(int eye, int yOff, const EyeStyle& s, float c, float gy) {
  uint16_t lidCol  = s.lid;
  uint16_t edgeCol = lerp565(s.lid, C_BLACK, 0.55f);

  float top  = lid.top + gy * 14;                      // lids follow the gaze a bit
  float bot  = lid.bot + gy * 8;
  top = top + (CLOSE_Y - top) * c;
  bot = bot + (CLOSE_Y - bot) * c;
  float kT   = lid.kTop * (1 - c);
  float kB   = lid.kBot * (1 - c);
  float side = (eye == 0) ? 1.0f : -1.0f;
  if (SWAP_EYES) side = -side;
  float tilt = lid.tilt * (1 - c) * side;

  for (int x = 0; x < SCR; x += 2) {
    float dx = x - 120;
    float d2 = (dx / 120.0f) * (dx / 120.0f);

    int u = (int)(top + kT * d2 + tilt * dx) - yOff;   // upper lid covers rows [0, u)
    if (u > 0) {
      int h = min(u, BAND_H);
      spr.fillRect(x, 0, 2, h, lidCol);
      int es = max(0, u - 4);
      if (es < h) spr.fillRect(x, es, 2, h - es, edgeCol);
    }

    int l = (int)(bot - kB * d2) - yOff;               // lower lid covers rows [l, BAND_H)
    if (l < BAND_H) {
      int st = max(0, l);
      spr.fillRect(x, st, 2, BAND_H - st, lidCol);
      int ee = min(BAND_H, l + 3);
      if (ee > st) spr.fillRect(x, st, 2, ee - st, edgeCol);
    }
  }
}

void drawEyeBand(int eye, int yOff, float blinkC, uint32_t now) {
  const EyeStyle& s = STYLES[styleIdx];
  int cy = 120 - yOff;

  spr.fillSprite(C_BLACK);
  spr.fillCircle(120, cy, 119, lerp565(s.sclera, C_BLACK, 0.25f));   // soft edge shading
  spr.fillCircle(120, cy, 104, s.sclera);

  float maxOff = constrain(118 - s.irisR * 0.55f, 30.0f, 72.0f);
  int ix = 120 + gazeX * maxOff;
  int iy = cy  + gazeY * maxOff;
  float ps = pupilUser * lid.pupil * (1.0f + 0.04f * sinf(now / 650.0f));  // slight "breathing"
  drawIris(s, ix, iy, ps, now);

  drawLids(eye, yOff, s, blinkC, gazeY);

  if (now < overlayUntil && yOff + BAND_H > 180) {
    spr.fillRoundRect(45, 186 - yOff, 150, 32, 10, C_BLACK);
    spr.setFont(&fonts::Font4);
    spr.setTextDatum(textdatum_t::middle_center);
    spr.setTextColor(C_WHITE);
    spr.drawString(overlayText.c_str(), 120, 203 - yOff);
  }
}

void renderEye(int eye, uint32_t now) {
  float c = blinkAmount(eye, now);
  for (int b = 0; b < BANDS; b++) {
    int yOff = b * BAND_H;
    drawEyeBand(eye, yOff, c, now);
    csSelect(eye);
    spr.pushSprite(0, yOff);
    tft.waitDisplay();
    csRelease();
    pollButtons(millis());          // keep buttons responsive mid-frame
  }
}

// ============================== Status / Serial =============================
String statusJson() {
  String j;
  j.reserve(700);
  j += "{\"style\":";  j += styleIdx;
  j += ",\"expr\":";   j += exprIdx;
  j += ",\"styles\":[";
  for (int i = 0; i < NUM_STYLES; i++) { if (i) j += ','; j += '"'; j += STYLES[i].name; j += '"'; }
  j += "],\"exprs\":[";
  for (int i = 0; i < NUM_EXPRS; i++)  { if (i) j += ','; j += '"'; j += EXPRS[i].name;  j += '"'; }
  j += "],\"auto\":";      j += autoMove  ? "true" : "false";
  j += ",\"autoBlink\":";  j += autoBlink ? "true" : "false";
  j += ",\"x\":";          j += String(gazeX, 3);
  j += ",\"y\":";          j += String(gazeY, 3);
  j += ",\"pupil\":";      j += String(pupilUser, 2);
  j += ",\"src\":\"";      j += controlSource; j += '"';
  j += ",\"fps\":";        j += String(fps, 1);
  j += ",\"joy\":[";       j += String(joyX, 2); j += ','; j += String(joyY, 2); j += ']';
  j += ",\"ap\":\"";       j += WiFi.softAPIP().toString(); j += '"';
  j += ",\"sta\":\"";      j += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(""); j += '"';
  j += ",\"heap\":";       j += ESP.getFreeHeap();
  j += '}';
  return j;
}

void printHelp() {
  Serial.println(F(
    "\n---------------- ESP32 Eyes - commands ----------------\n"
    " help              this list\n"
    " status            show current state\n"
    " styles            list eye styles\n"
    " style <n|name>    choose eye style   (e.g. style 2, style cat)\n"
    " next / prev       next / previous style\n"
    " expr <n|name>     expression (normal happy sleepy angry sad surprised)\n"
    " blink             blink both eyes\n"
    " wink <l|r>        wink one eye\n"
    " look <x> <y>      look at x,y  (-100..100 each)\n"
    " center            release manual control, back to auto\n"
    " auto <on|off>     automatic eye movement\n"
    " autoblink <on|off>\n"
    " pupil <30..200>   pupil size in percent\n"
    " cal               re-centre joystick\n"
    " joy               toggle live joystick readout\n"
    " wifi              show network info\n"
    "-------------------------------------------------------"));
}

int findByName(const String& arg, bool styles) {
  if (arg.length() && isDigit(arg[0])) return arg.toInt();
  int n = styles ? NUM_STYLES : NUM_EXPRS;
  for (int i = 0; i < n; i++)
    if (strcasecmp(arg.c_str(), styles ? STYLES[i].name : EXPRS[i].name) == 0) return i;
  return -1;
}

bool parseOnOff(const String& a) { return a == "on" || a == "1" || a == "true"; }

void processCommand(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;
  cmd.toLowerCase();
  int sp = cmd.indexOf(' ');
  String head = sp < 0 ? cmd : cmd.substring(0, sp);
  String arg  = sp < 0 ? ""  : cmd.substring(sp + 1);
  arg.trim();

  if (head == "help" || head == "?") printHelp();
  else if (head == "status") {
    Serial.printf("Style: %s | Expression: %s | Auto: %s | Autoblink: %s\n",
                  STYLES[styleIdx].name, EXPRS[exprIdx].name, autoMove ? "on" : "off", autoBlink ? "on" : "off");
    Serial.printf("Gaze: x=%+.2f y=%+.2f (%s) | Pupil: %d%% | FPS: %.1f | Free heap: %u\n",
                  gazeX, gazeY, controlSource, (int)(pupilUser * 100), fps, ESP.getFreeHeap());
  }
  else if (head == "styles") {
    for (int i = 0; i < NUM_STYLES; i++) Serial.printf("  %d  %s\n", i, STYLES[i].name);
  }
  else if (head == "style") {
    int i = findByName(arg, true);
    if (i >= 0 && i < NUM_STYLES) setStyle(i); else Serial.println("Unknown style. Type 'styles'.");
  }
  else if (head == "next") stepStyle(+1);
  else if (head == "prev") stepStyle(-1);
  else if (head == "expr") {
    int i = findByName(arg, false);
    if (i >= 0 && i < NUM_EXPRS) setExpression(i); else Serial.println("Unknown expression.");
  }
  else if (head == "blink") startBlink(3);
  else if (head == "wink")  startBlink(arg.startsWith("r") ? 2 : 1);
  else if (head == "look") {
    int sp2 = arg.indexOf(' ');
    if (sp2 < 0) { Serial.println("Usage: look <x> <y>"); return; }
    float x = arg.substring(0, sp2).toFloat() / 100.0f;
    float y = arg.substring(sp2 + 1).toFloat() / 100.0f;
    lookAt(x, y, "serial");
    Serial.printf("[LOOK] x=%+.2f y=%+.2f\n", x, y);
  }
  else if (head == "center" || head == "centre") { releaseManual(); Serial.println("[LOOK] released"); }
  else if (head == "auto")      setAutoMove(parseOnOff(arg));
  else if (head == "autoblink") { autoBlink = parseOnOff(arg); Serial.printf("[AUTO] blink %s\n", autoBlink ? "ON" : "OFF"); }
  else if (head == "pupil") {
    pupilUser = constrain(arg.toInt(), 30, 200) / 100.0f;
    Serial.printf("[PUPIL] %d%%\n", (int)(pupilUser * 100));
  }
  else if (head == "cal") calibrateJoystick();
  else if (head == "joy") { joyDebug = !joyDebug; Serial.printf("[JOY] live readout %s\n", joyDebug ? "ON" : "OFF"); }
  else if (head == "wifi") {
    Serial.printf("AP  : %s  ->  http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    if (WiFi.status() == WL_CONNECTED)
      Serial.printf("WiFi: %s  ->  http://%s  (http://%s.local)\n", STA_SSID, WiFi.localIP().toString().c_str(), MDNS_NAME);
    else Serial.println("WiFi: not connected to a router");
  }
  else Serial.printf("Unknown command '%s' - type help\n", head.c_str());
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    serialLastChar = millis();
    if (c == '\r') continue;
    if (c == '\n') { processCommand(serialBuf); serialBuf = ""; }
    else if (serialBuf.length() < 80) serialBuf += c;
  }
  // also accept commands when the monitor's line ending is set to "No line ending"
  if (serialBuf.length() && millis() - serialLastChar > 400) { processCommand(serialBuf); serialBuf = ""; }
}

// ============================== Web server ==================================
// Control panel page (embedded here so the sketch is a single file)
// Web control panel served by the ESP32 at http://192.168.4.1 (or http://eyes.local)
// Uses only system fonts so it works with no internet connection.

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Eye control</title>
<style>
:root{
  --bg:#1d2233; --panel:#262d43; --line:#343c57;
  --text:#eef0f6; --muted:#9aa3bd;
  --sclera:#f3efe6; --iris:#4a8fe0; --iris-in:#9fd0f5; --amber:#f2b441;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);
  font:16px/1.45 ui-rounded,"SF Pro Rounded","Segoe UI",system-ui,sans-serif}
header{display:flex;align-items:baseline;justify-content:space-between;gap:1rem;
  padding:1.1rem 1.4rem .4rem;max-width:980px;margin:auto}
h1{margin:0;font-size:1.7rem;font-weight:700;letter-spacing:-.01em}
#conn{font-size:.9rem;color:var(--muted)}
#conn.ok::before{content:"";display:inline-block;width:.55rem;height:.55rem;border-radius:50%;
  background:#6bd68a;margin-right:.4rem}
#conn.bad{color:#ff8a7a}
main{display:grid;grid-template-columns:minmax(260px,380px) 1fr;gap:1.6rem;
  max-width:980px;margin:auto;padding:1rem 1.4rem 2rem}
@media(max-width:760px){main{grid-template-columns:1fr}}

/* The look pad is drawn as an eyeball: drag the iris to aim the eyes */
.padwrap{display:flex;flex-direction:column;align-items:center;gap:.7rem}
#pad{position:relative;width:min(100%,360px);aspect-ratio:1;border-radius:50%;
  background:radial-gradient(circle at 50% 50%,var(--sclera) 62%,#cfc9bd 100%);
  touch-action:none;cursor:grab;box-shadow:inset 0 -10px 30px rgba(0,0,0,.25)}
#pad:active{cursor:grabbing}
#knob{position:absolute;left:50%;top:50%;width:34%;aspect-ratio:1;border-radius:50%;
  transform:translate(-50%,-50%);
  background:radial-gradient(circle,#05070d 0 32%,var(--iris-in) 34%,var(--iris) 78%,#1b3d6e 100%);
  box-shadow:0 0 0 3px rgba(20,40,80,.35);pointer-events:none}
#knob::after{content:"";position:absolute;left:24%;top:20%;width:16%;height:16%;
  border-radius:50%;background:#fff}
#gaze{position:absolute;left:50%;top:50%;width:12%;aspect-ratio:1;border-radius:50%;
  border:2px dashed var(--amber);transform:translate(-50%,-50%);pointer-events:none;opacity:.9}
.hint{color:var(--muted);font-size:.9rem;text-align:center;margin:0}

section{margin-bottom:1.4rem}
h2{font-size:1.05rem;font-weight:600;margin:0 0 .6rem}
.chips{display:flex;flex-wrap:wrap;gap:.5rem}
button{font:inherit;color:var(--text);background:var(--panel);border:1px solid var(--line);
  border-radius:999px;padding:.5rem 1rem;cursor:pointer}
button:hover{border-color:var(--muted)}
button:focus-visible,input:focus-visible{outline:3px solid var(--amber);outline-offset:2px}
button.on{background:var(--amber);border-color:var(--amber);color:#2a1d00;font-weight:600}
.row{display:flex;align-items:center;justify-content:space-between;gap:1rem;
  padding:.55rem 0;border-bottom:1px solid var(--line)}
.row:last-child{border-bottom:0}
input[type=range]{width:55%;accent-color:var(--amber)}
.switch{appearance:none;width:2.8rem;height:1.6rem;border-radius:1rem;background:var(--line);
  position:relative;cursor:pointer;transition:background .15s}
.switch::after{content:"";position:absolute;top:.2rem;left:.2rem;width:1.2rem;height:1.2rem;
  border-radius:50%;background:#fff;transition:left .15s}
.switch:checked{background:var(--amber)}
.switch:checked::after{left:1.4rem}
#stats{color:var(--muted);font-size:.85rem;display:grid;grid-template-columns:auto 1fr;gap:.15rem 1rem}
@media(prefers-reduced-motion:reduce){.switch,.switch::after{transition:none}}
</style>
</head>
<body>
<header>
  <h1>Eye control</h1>
  <span id="conn">Connecting…</span>
</header>

<main>
  <div class="padwrap">
    <div id="pad" role="application" aria-label="Drag to aim the eyes">
      <div id="gaze"></div><div id="knob"></div>
    </div>
    <p class="hint">Drag the iris to aim the eyes. The dashed ring shows where they are looking. Arrow keys and space bar work too.</p>
  </div>

  <div>
    <section>
      <h2>Eye style</h2>
      <div class="chips" id="styles"></div>
    </section>

    <section>
      <h2>Expression</h2>
      <div class="chips" id="exprs"></div>
    </section>

    <section>
      <h2>Actions</h2>
      <div class="chips">
        <button onclick="api('blink?eye=both')">Blink</button>
        <button onclick="api('blink?eye=left')">Wink left</button>
        <button onclick="api('blink?eye=right')">Wink right</button>
        <button onclick="api('release')">Look ahead</button>
      </div>
    </section>

    <section>
      <h2>Settings</h2>
      <div class="row"><label for="auto">Move eyes automatically</label>
        <input type="checkbox" class="switch" id="auto" onchange="api('auto?move='+(this.checked?1:0))"></div>
      <div class="row"><label for="ablink">Blink automatically</label>
        <input type="checkbox" class="switch" id="ablink" onchange="api('auto?blink='+(this.checked?1:0))"></div>
      <div class="row"><label for="snap">Return to auto when I let go</label>
        <input type="checkbox" class="switch" id="snap" checked></div>
      <div class="row"><label for="pupil">Pupil size <span id="pv"></span></label>
        <input type="range" id="pupil" min="30" max="200" value="100"></div>
      <div class="row"><span>Joystick drifting?</span>
        <button onclick="api('calibrate')">Re-centre joystick</button></div>
    </section>

    <section>
      <h2>Status</h2>
      <div id="stats"></div>
    </section>
  </div>
</main>

<script>
const $=id=>document.getElementById(id);
const pad=$('pad'), knob=$('knob'), gaze=$('gaze');
let dragging=false, lastSend=0, pend=null, tmr=null, last=[0,0], busy=false, pupilDrag=false, kx=0, ky=0;

function setConn(ok){const c=$('conn');c.className=ok?'ok':'bad';c.textContent=ok?'Connected':'No connection – check WiFi';}

function api(p){
  return fetch('/api/'+p).then(r=>{setConn(true);return r.status===200?r.json():null})
    .then(j=>{if(j)render(j)}).catch(()=>setConn(false));
}

function chips(el,list,active,path){
  if(el.childElementCount!==list.length){
    el.innerHTML='';
    list.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;
      b.onclick=()=>api(path+'?i='+i);el.appendChild(b);});
  }
  [...el.children].forEach((b,i)=>{b.classList.toggle('on',i===active);b.setAttribute('aria-pressed',i===active)});
}

function place(el,x,y){el.style.left=(50+x*33)+'%';el.style.top=(50+y*33)+'%';}

function render(s){
  chips($('styles'),s.styles,s.style,'style');
  chips($('exprs'),s.exprs,s.expr,'expr');
  $('auto').checked=s.auto; $('ablink').checked=s.autoBlink;
  if(!pupilDrag){$('pupil').value=Math.round(s.pupil*100);$('pv').textContent=Math.round(s.pupil*100)+'%';}
  place(gaze,s.x,s.y);
  if(!dragging) place(knob,s.x,s.y);
  const src={auto:'Automatic',hold:'Holding still',joystick:'Joystick',web:'This page',serial:'Serial monitor'}[s.src]||s.src;
  $('stats').innerHTML=
    `<span>Controlled by</span><span>${src}</span>`+
    `<span>Frame rate</span><span>${s.fps} fps</span>`+
    `<span>Joystick</span><span>${s.joy[0]}, ${s.joy[1]}</span>`+
    `<span>Access point</span><span>http://${s.ap}</span>`+
    (s.sta?`<span>Home WiFi</span><span>http://${s.sta}</span>`:'')+
    `<span>Free memory</span><span>${Math.round(s.heap/1024)} KB</span>`;
}

/* ---- look pad ---- */
function flush(){
  tmr=null; if(!pend) return;
  lastSend=performance.now(); const [x,y]=pend; pend=null;
  fetch(`/api/look?x=${x.toFixed(3)}&y=${y.toFixed(3)}`).catch(()=>setConn(false));
}
function queueLook(x,y){
  pend=[x,y]; last=[x,y];
  const wait=70-(performance.now()-lastSend);
  if(wait<=0) flush(); else if(!tmr) tmr=setTimeout(flush,wait);
}
function fromEvent(e){
  const r=pad.getBoundingClientRect();
  let x=((e.clientX-r.left)/r.width*2-1)/0.66, y=((e.clientY-r.top)/r.height*2-1)/0.66;
  const m=Math.hypot(x,y); if(m>1){x/=m;y/=m;}
  place(knob,x,y); queueLook(x,y);
}
pad.addEventListener('pointerdown',e=>{dragging=true;pad.setPointerCapture(e.pointerId);fromEvent(e)});
pad.addEventListener('pointermove',e=>{if(dragging)fromEvent(e)});
['pointerup','pointercancel'].forEach(ev=>pad.addEventListener(ev,()=>{
  if(!dragging) return; dragging=false;
  if($('snap').checked){place(knob,0,0);api('release');}
}));
setInterval(()=>{if(dragging)queueLook(...last)},1000);   // keep control while holding still

/* ---- keyboard ---- */
document.addEventListener('keydown',e=>{
  if(e.target.tagName==='INPUT'||e.target.tagName==='BUTTON') return;
  const step=0.25, k=e.key;
  if(k===' '){api('blink?eye=both');e.preventDefault();return;}
  if(k==='ArrowLeft')kx-=step; else if(k==='ArrowRight')kx+=step;
  else if(k==='ArrowUp')ky-=step; else if(k==='ArrowDown')ky+=step; else return;
  e.preventDefault();
  kx=Math.max(-1,Math.min(1,kx)); ky=Math.max(-1,Math.min(1,ky));
  place(knob,kx,ky); queueLook(kx,ky);
});

/* ---- pupil slider ---- */
const ps=$('pupil');
ps.addEventListener('input',()=>{pupilDrag=true;$('pv').textContent=ps.value+'%';});
ps.addEventListener('change',()=>{pupilDrag=false;api('pupil?v='+(ps.value/100));});

/* ---- status polling ---- */
function poll(){ if(busy) return; busy=true; api('status').finally(()=>busy=false); }
poll(); setInterval(poll,400);
</script>
</body>
</html>
)rawliteral";

void sendStatus() { server.send(200, "application/json", statusJson()); }

void setupWeb() {
  server.on("/", []() { server.send(200, "text/html", INDEX_HTML); });
  server.on("/api/status", sendStatus);

  server.on("/api/style", []() {
    if (server.hasArg("i")) setStyle(server.arg("i").toInt());
    else if (server.hasArg("dir")) stepStyle(server.arg("dir") == "prev" ? -1 : 1);
    sendStatus();
  });
  server.on("/api/expr", []() {
    if (server.hasArg("i")) setExpression(server.arg("i").toInt());
    sendStatus();
  });
  server.on("/api/look", []() {
    lookAt(server.arg("x").toFloat(), server.arg("y").toFloat(), "web");
    server.send(204);
  });
  server.on("/api/release", []() { releaseManual(); sendStatus(); });
  server.on("/api/blink", []() {
    String e = server.arg("eye");
    startBlink(e == "left" ? 1 : e == "right" ? 2 : 3);
    sendStatus();
  });
  server.on("/api/auto", []() {
    if (server.hasArg("move"))  setAutoMove(server.arg("move") == "1");
    if (server.hasArg("blink")) autoBlink = server.arg("blink") == "1";
    sendStatus();
  });
  server.on("/api/pupil", []() {
    pupilUser = constrain(server.arg("v").toFloat(), 0.3f, 2.0f);
    sendStatus();
  });
  server.on("/api/calibrate", []() { calibrateJoystick(); showOverlay("Centred"); sendStatus(); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void setupWiFi() {
  WiFi.mode(strlen(STA_SSID) ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[WIFI] Access point '%s' (password '%s') -> http://%s\n",
                AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());

  if (strlen(STA_SSID)) {
    Serial.printf("[WIFI] Connecting to %s", STA_SSID);
    WiFi.begin(STA_SSID, STA_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) { delay(250); Serial.print('.'); }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] Connected -> http://%s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI] Router connection failed - using access point only");
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, AP_PASS);
    }
  }
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[WIFI] mDNS: http://%s.local\n", MDNS_NAME);
  }
}

void bootText(const String& txt, int y, const lgfx::IFont* font) {
  tft.setFont(font);
  tft.drawString(txt.c_str(), 120, y);
}

void bootScreen() {
  for (int eye = 0; eye < 2; eye++) {
    csSelect(eye);
    tft.fillScreen(C_BLACK);
    tft.setTextDatum(textdatum_t::middle_center);
    tft.setTextColor(C_WHITE, C_BLACK);
    if (eye == 0) {
      bootText("ESP32 Eyes", 105, &fonts::Font4);
      bootText("WiFi: " + String(AP_SSID), 140, &fonts::Font2);
    } else {
      bootText("Control panel", 95, &fonts::Font4);
      bootText("http://" + WiFi.softAPIP().toString(), 130, &fonts::Font2);
      if (WiFi.status() == WL_CONNECTED)
        bootText("http://" + WiFi.localIP().toString(), 152, &fonts::Font2);
    }
    tft.waitDisplay();
    csRelease();
  }
}

// ============================== Setup / Loop ================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== ESP32 Animated Eyes ===");

  for (int i = 0; i < 48; i++) { COS48[i] = cosf(i * TWO_PI / 48); SIN48[i] = sinf(i * TWO_PI / 48); }

  pinMode(TFT1_CS, OUTPUT);
  pinMode(TFT2_CS, OUTPUT);
  pinMode(JOY_SW_PIN, INPUT_PULLUP);
  pinMode(MODE_BTN_PIN, INPUT_PULLUP);

  // Initialise BOTH displays at once (both CS low), then set rotation per display
  digitalWrite(TFT1_CS, LOW);
  digitalWrite(TFT2_CS, LOW);
  Serial.println("[TFT] initialising displays...");
  tft.init();
  tft.fillScreen(C_BLACK);
  tft.waitDisplay();
  csRelease();
  for (int eye = 0; eye < 2; eye++) {
    csSelect(eye);
    tft.startWrite();
    tft.setRotation(eye == 0 ? LEFT_ROTATION : RIGHT_ROTATION);
    tft.endWrite();
    csRelease();
  }
  Serial.println("[TFT] displays ready");

  spr.setColorDepth(16);
  if (!spr.createSprite(SCR, BAND_H)) {
    Serial.println("!! Sprite allocation failed - out of memory");
    while (true) delay(1000);
  }

#if USE_JOYSTICK
  analogReadResolution(12);
  calibrateJoystick();               // keep the joystick still during boot
#endif

  setupWiFi();
  setupWeb();
  bootScreen();
  delay(2500);

  const Expression& e = EXPRS[exprIdx];
  lid = { e.top, e.kTop, e.tilt, e.bot, e.kBot, e.pupil };
  randomSeed(esp_random());
  setStyle(0);
  printHelp();
  lastFrame = fpsTimer = millis();
}

void loop() {
  uint32_t now = millis();
  float dt = constrain((float)(now - lastFrame), 1.0f, 200.0f);
  lastFrame = now;

  server.handleClient();
  handleSerial();
  pollButtons(now);
  readJoystick(now);
  updateAnimation(now, dt);

  renderEye(0, now);
  server.handleClient();
  renderEye(1, now);

  fpsFrames++;
  if (now - fpsTimer >= 1000) {
    fps = fpsFrames * 1000.0f / (now - fpsTimer);
    fpsFrames = 0;
    fpsTimer = now;
  }
}
