#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define PIN_BAT_ADC    D0
#define PIN_SPEAKER    D1
#define PIN_BTN_RED    D2
#define PIN_BTN_LEFT   D3
#define PIN_BTN_RIGHT  D4

#define TFT_CS         D6
#define TFT_DC         D7
#define TFT_SCK        D8
#define TFT_MOSI       D10
#define TFT_RST        -1

#define SCREEN_W       160
#define SCREEN_H       128

Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(SCREEN_W, SCREEN_H);
Preferences prefs;

struct Theme {
  const char* name;
  uint16_t bg;
  uint16_t panel;
  uint16_t grid;
  uint16_t pri;
  uint16_t dim;
  uint16_t hi;
  uint16_t accent;
  uint16_t white;
};

const Theme THEMES[5] = {
  { "PHOSPHOR",   0x0881, 0x1103, 0x1A05, 0x37F1, 0x246A, 0xFE29, 0xFACA, 0xEFFF },
  { "CUTESY BMO", 0x1083, 0x20E6, 0x398A, 0x6F9A, 0x44D1, 0xFC36, 0xFF0C, 0xFFDF },
  { "GAMEBOY",    0x0901, 0x19A3, 0x32C6, 0x9DE1, 0x53A3, 0xD78E, 0xFFFF, 0xF7F9 },
  { "AMBER CRT",  0x1060, 0x20C1, 0x4162, 0xFD80, 0x9B40, 0xFF10, 0xFABA, 0xFFDC },
  { "FAMICOM",    0x1882, 0x28C3, 0x4985, 0xF68D, 0x9BD0, 0xFAC6, 0x5F1C, 0xFFDD }
};

const uint16_t ICON_SEND_MORSE[12] = {
  0b000100100000, 0b001001001000, 0b000000000000, 0b000000011110,
  0b000000011110, 0b011111111100, 0b000110001000, 0b000110001000,
  0b011111111110, 0b111111111111, 0b000000000000, 0b000000000000
};

const uint16_t ICON_TRANSLATE[12] = {
  0b011111111110, 0b110000000011, 0b100110111101, 0b100000000001,
  0b100111101101, 0b100000000001, 0b110000000011, 0b011110011110,
  0b000110000000, 0b001100000000, 0b000000000000, 0b000000000000
};

const uint16_t ICON_INBOX[12] = {
  0b000111111000, 0b001000000100, 0b010011110010, 0b111111111111,
  0b101000000101, 0b100100001001, 0b100011110001, 0b100000000001,
  0b111111111111, 0b000000000000, 0b000000000000, 0b000000000000
};

const uint16_t ICON_SETTINGS[12] = {
  0b000000000000, 0b000110000000, 0b111111111111, 0b000110000000,
  0b000000000000, 0b000000011000, 0b111111111111, 0b000000011000,
  0b000000000000, 0b001100000000, 0b111111111111, 0b001100000000
};

struct MorseMap {
  char ch;
  const char* code;
};

const MorseMap MORSE_TABLE[] = {
  {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},  {'E', "."},
  {'F', "..-."}, {'G', "--."},  {'H', "...."}, {'I', ".."},   {'J', ".---"},
  {'K', "-.-"},  {'L', ".-.."}, {'M', "--"},   {'N', "-."},   {'O', "---"},
  {'P', ".--."}, {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
  {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"}, {'Y', "-.--"},
  {'Z', "--.."},
  {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
  {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
  {'?', "..--.."}
};
const int MORSE_TABLE_LEN = sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]);

const char* KEYBOARD_KEYS[40] = {
  "A", "B", "C", "D", "E", "F", "G", "H",
  "I", "J", "K", "L", "M", "N", "O", "P",
  "Q", "R", "S", "T", "U", "V", "W", "X",
  "Y", "Z", "0", "1", "2", "3", "4", "5",
  "6", "7", "8", "9", "?", "DEL", "SPC", "SND"
};

char decodeMorseSymbol(const String& sym) {
  for (int i = 0; i < MORSE_TABLE_LEN; i++) {
    if (sym.equals(MORSE_TABLE[i].code)) return MORSE_TABLE[i].ch;
  }
  return '?';
}

const char* encodeCharToMorse(char c) {
  c = toupper(c);
  for (int i = 0; i < MORSE_TABLE_LEN; i++) {
    if (MORSE_TABLE[i].ch == c) return MORSE_TABLE[i].code;
  }
  return nullptr;
}

typedef struct __attribute__((packed)) {
  uint8_t  packetType;
  uint16_t dotDashMs;
  uint16_t letterGapMs;
  uint16_t wordGapMs;
} EspNowCwPacket;

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

enum ScreenState {
  SCR_BOOT,
  SCR_HOME,
  SCR_SEND,
  SCR_TRANSLATE,
  SCR_RECEIVE,
  SCR_SETTINGS,
  SCR_GAMES,
  SCR_DINO,
  SCR_FLAPPY
};

ScreenState currentScreen = SCR_BOOT;
uint32_t bootStartMs = 0;

uint16_t cfgDotDashMs   = 200;
uint16_t cfgLetterGapMs = 600;
uint16_t cfgWordGapMs   = 1400;
uint8_t  cfgVolumeIdx   = 2;
uint8_t  cfgThemeIdx    = 0;

const char* VOLUME_LABELS[4] = { "MUTE", "LOW", "MED", "HIGH" };
const uint8_t VOLUME_DUTY[4] = { 0, 18, 65, 128 };

int homeIdx = 0;
int homeScroll = 0;
int settingsIdx = 0;
int settingsScroll = 0;
bool settingsLocked = false;
uint32_t resetFlashUntil = 0;
int gamesIdx = 0;
int kbIdx = 0;

int batteryPct = 100;
uint32_t lastBatReadMs = 0;

bool     sendKeyDown = false;
uint32_t sendPressStartMs = 0;
uint32_t sendLastReleaseMs = 0;
String   sendSymbols = "";
String   sendDecoded = "";
bool     sendSpacePending = false;
String   sendStatus = "HOLD RED TO KEY";

String   transText = "HELLO ";
bool     transActive = false;
String   transPreview = "";
struct TxEvent {
  bool     carrierOn;
  uint16_t durationMs;
  String   preview;
};
TxEvent  txQueue[96];
int      txQueueLen = 0;
int      txQueueIdx = 0;
uint32_t txStepStartMs = 0;

volatile bool     rxEdgePendingOn = false;
volatile bool     rxEdgePendingOff = false;
volatile uint16_t rxIncomingDotDash = 200;
volatile uint16_t rxIncomingLetterGap = 600;
volatile uint16_t rxIncomingWordGap = 1400;

bool     rxCarrierOn = false;
uint32_t rxPressStartMs = 0;
uint32_t rxLastReleaseMs = 0;
String   rxSymbols = "";
String   rxDecoded = "";
bool     rxSpacePending = false;
bool     rxHasUnread = false;
String   rxStatus = "WAITING FOR SIGNAL";
uint16_t rxSyncedDotDash = 200;
uint16_t rxSyncedLetterGap = 600;
uint16_t rxSyncedWordGap = 1400;

struct DinoObstacle { float x; int w; int h; bool active; };
DinoObstacle dinoObs[4];
bool  dinoGameOver = false;
float dinoY = 0, dinoVy = 0;
int   dinoScore = 0, dinoHigh = 0, dinoSpawnTick = 30;

struct FlappyPipe { float x; int gapY; int gapH; bool passed; bool active; };
FlappyPipe flappyPipes[3];
bool  flappyGameOver = false;
float birdY = 56, birdVy = 0;
int   flappyScore = 0, flappyHigh = 0, flappySpawnTick = 10;

uint32_t beepOffAtMs = 0;
bool     cwToneActive = false;

void setSpeakerTone(uint16_t freq) {
  uint8_t duty = VOLUME_DUTY[cfgVolumeIdx];
  if (duty == 0 || freq == 0) {
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(PIN_SPEAKER, 0);
    #else
      ledcWrite(0, 0);
    #endif
    return;
  }
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(PIN_SPEAKER, freq);
    ledcWrite(PIN_SPEAKER, duty);
  #else
    ledcWriteTone(0, freq);
    ledcWrite(0, duty);
  #endif
}

void startCwTone(uint16_t freq = 780) {
  cwToneActive = true;
  setSpeakerTone(freq);
}

void stopCwTone() {
  cwToneActive = false;
  setSpeakerTone(0);
}

void playUiBeep(uint16_t freq = 920, uint16_t durationMs = 25) {
  if (cwToneActive || VOLUME_DUTY[cfgVolumeIdx] == 0) return;
  setSpeakerTone(freq);
  beepOffAtMs = millis() + durationMs;
}

void updateAudioTimer() {
  if (!cwToneActive && beepOffAtMs > 0 && millis() >= beepOffAtMs) {
    beepOffAtMs = 0;
    setSpeakerTone(0);
  }
}

void loadSettings() {
  prefs.begin("morse_cfg", true);
  cfgDotDashMs   = prefs.getUShort("dotDash", 200);
  cfgLetterGapMs = prefs.getUShort("ltrGap",  600);
  cfgWordGapMs   = prefs.getUShort("wrdGap",  1400);
  cfgVolumeIdx   = prefs.getUChar("vol",      2);
  cfgThemeIdx    = prefs.getUChar("theme",    0);
  prefs.end();

  if (cfgVolumeIdx > 3) cfgVolumeIdx = 2;
  if (cfgThemeIdx > 4)  cfgThemeIdx = 0;

  rxSyncedDotDash   = cfgDotDashMs;
  rxSyncedLetterGap = cfgLetterGapMs;
  rxSyncedWordGap   = cfgWordGapMs;
}

void saveSettings() {
  prefs.begin("morse_cfg", false);
  prefs.putUShort("dotDash", cfgDotDashMs);
  prefs.putUShort("ltrGap",  cfgLetterGapMs);
  prefs.putUShort("wrdGap",  cfgWordGapMs);
  prefs.putUChar("vol",      cfgVolumeIdx);
  prefs.putUChar("theme",    cfgThemeIdx);
  prefs.end();
}

void sendEspNowPacket(uint8_t type) {
  EspNowCwPacket pkt;
  pkt.packetType  = type;
  pkt.dotDashMs   = cfgDotDashMs;
  pkt.letterGapMs = cfgLetterGapMs;
  pkt.wordGapMs   = cfgWordGapMs;
  esp_now_send(broadcastAddress, (uint8_t*)&pkt, sizeof(pkt));
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(EspNowCwPacket)) return;
  EspNowCwPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  rxIncomingDotDash   = pkt.dotDashMs;
  rxIncomingLetterGap = pkt.letterGapMs;
  rxIncomingWordGap   = pkt.wordGapMs;

  if (pkt.packetType == 1) {
    rxEdgePendingOn = true;
  } else if (pkt.packetType == 0) {
    rxEdgePendingOff = true;
  }
}

void updateBatteryPercentage() {
  if (millis() - lastBatReadMs < 2500 && lastBatReadMs != 0) return;
  lastBatReadMs = millis();

  uint32_t mvSum = 0;
  for (int i = 0; i < 8; i++) {
    mvSum += analogReadMilliVolts(PIN_BAT_ADC);
  }
  uint32_t batMv = (mvSum / 8) * 2;

  if (batMv < 2500) {
    batteryPct = 100;
  } else {
    int pct = (int)((batMv - 3300) * 100L / (4200 - 3300));
    batteryPct = constrain(pct, 5, 100);
  }
}

void stopTranslateTx() {
  if (transActive) {
    sendEspNowPacket(0);
    stopCwTone();
  }
  transActive = false;
  txQueueLen = 0;
  txQueueIdx = 0;
  transPreview = "";
}

void startTranslateTx() {
  String msg = transText;
  msg.trim();
  if (msg.length() == 0) return;

  stopTranslateTx();
  txQueueLen = 0;

  uint16_t u        = max(40, (int)(cfgDotDashMs / 2));
  uint16_t dashMs   = (uint16_t)(cfgDotDashMs * 1.5);
  uint16_t intraGap = u;

  for (unsigned int i = 0; i < msg.length() && txQueueLen < 90; i++) {
    char ch = msg[i];
    if (ch == ' ') {
      txQueue[txQueueLen++] = { false, cfgWordGapMs, "[SPACE]" };
      continue;
    }
    const char* code = encodeCharToMorse(ch);
    if (!code) continue;

    String label = String(ch) + " [" + String(code) + "]";
    int cLen = strlen(code);
    for (int s = 0; s < cLen && txQueueLen < 92; s++) {
      uint16_t toneDur = (code[s] == '.') ? u : dashMs;
      txQueue[txQueueLen++] = { true,  toneDur, label };
      if (s < cLen - 1) {
        txQueue[txQueueLen++] = { false, intraGap, label };
      }
    }
    txQueue[txQueueLen++] = { false, cfgLetterGapMs, label };
  }

  if (txQueueLen > 0) {
    transActive = true;
    txQueueIdx = 0;
    txStepStartMs = millis();
    transPreview = txQueue[0].preview;
    if (txQueue[0].carrierOn) {
      startCwTone(780);
      sendEspNowPacket(1);
    }
  }
}

void updateTranslateTx() {
  if (!transActive) return;

  uint32_t now = millis();
  if (now - txStepStartMs >= txQueue[txQueueIdx].durationMs) {
    bool wasOn = txQueue[txQueueIdx].carrierOn;
    txQueueIdx++;

    if (txQueueIdx >= txQueueLen) {
      if (wasOn) {
        stopCwTone();
        sendEspNowPacket(0);
      }
      transActive = false;
      transPreview = "";
      return;
    }

    txStepStartMs = now;
    transPreview = txQueue[txQueueIdx].preview;
    bool nowOn = txQueue[txQueueIdx].carrierOn;

    if (nowOn && !wasOn) {
      startCwTone(780);
      sendEspNowPacket(1);
    } else if (!nowOn && wasOn) {
      stopCwTone();
      sendEspNowPacket(0);
    }
  }
}

void initDinoGame() {
  dinoGameOver = false;
  dinoY = 0;
  dinoVy = 0;
  dinoScore = 0;
  dinoSpawnTick = 25;
  for (int i = 0; i < 4; i++) dinoObs[i].active = false;
}

void updateDinoGame() {
  if (dinoGameOver) return;

  dinoY += dinoVy;
  dinoVy += 0.45f;
  if (dinoY >= 0) { dinoY = 0; dinoVy = 0; }

  dinoSpawnTick--;
  if (dinoSpawnTick <= 0) {
    for (int i = 0; i < 4; i++) {
      if (!dinoObs[i].active) {
        dinoObs[i].active = true;
        dinoObs[i].x = SCREEN_W + 4;
        dinoObs[i].w = (random(0, 10) > 5) ? 10 : 6;
        dinoObs[i].h = (random(0, 10) > 6) ? 16 : 11;
        break;
      }
    }
    dinoSpawnTick = random(40, 78);
  }

  const int dinoX = 18, dinoW = 10, dinoH = 14, groundY = 106;
  int dinoTop = groundY - dinoH + (int)dinoY;

  for (int i = 0; i < 4; i++) {
    if (!dinoObs[i].active) continue;
    dinoObs[i].x -= 2.2f;

    int obTop = groundY - dinoObs[i].h;
    if (dinoX + dinoW - 2 > dinoObs[i].x &&
        dinoX + 2 < dinoObs[i].x + dinoObs[i].w &&
        dinoTop + dinoH - 1 > obTop) {
      dinoGameOver = true;
      if (dinoScore > dinoHigh) dinoHigh = dinoScore;
      playUiBeep(240, 150);
    }
    if (dinoObs[i].x + dinoObs[i].w < 0) {
      dinoObs[i].active = false;
      dinoScore++;
    }
  }
}

void initFlappyGame() {
  flappyGameOver = false;
  birdY = 56;
  birdVy = 0;
  flappyScore = 0;
  flappySpawnTick = 10;
  for (int i = 0; i < 3; i++) flappyPipes[i].active = false;
}

void updateFlappyGame() {
  if (flappyGameOver) return;

  birdVy += 0.22f;
  birdY += birdVy;

  if (birdY < 15 || birdY > 107) {
    flappyGameOver = true;
    if (flappyScore > flappyHigh) flappyHigh = flappyScore;
    playUiBeep(240, 150);
  }

  flappySpawnTick--;
  if (flappySpawnTick <= 0) {
    for (int i = 0; i < 3; i++) {
      if (!flappyPipes[i].active) {
        flappyPipes[i].active = true;
        flappyPipes[i].x = SCREEN_W + 4;
        flappyPipes[i].gapY = random(28, 72);
        flappyPipes[i].gapH = 34;
        flappyPipes[i].passed = false;
        break;
      }
    }
    flappySpawnTick = 58;
  }

  const int birdX = 26, birdSize = 7;
  for (int i = 0; i < 3; i++) {
    if (!flappyPipes[i].active) continue;
    flappyPipes[i].x -= 1.4f;

    if (birdX + birdSize > flappyPipes[i].x && birdX < flappyPipes[i].x + 12) {
      if (birdY < flappyPipes[i].gapY || birdY + birdSize > flappyPipes[i].gapY + flappyPipes[i].gapH) {
        flappyGameOver = true;
        if (flappyScore > flappyHigh) flappyHigh = flappyScore;
        playUiBeep(240, 150);
      }
    }
    if (!flappyPipes[i].passed && flappyPipes[i].x + 12 < birdX) {
      flappyPipes[i].passed = true;
      flappyScore++;
    }
    if (flappyPipes[i].x < -16) {
      flappyPipes[i].active = false;
    }
  }
}

int wrapIdx(int cur, int delta, int total) {
  return (cur + delta + total) % total;
}

void syncHomeViewport() {
  if (homeIdx < homeScroll) homeScroll = homeIdx;
  else if (homeIdx >= homeScroll + 3) homeScroll = homeIdx - 2;
}

void syncSettingsViewport() {
  if (settingsIdx < settingsScroll) settingsScroll = settingsIdx;
  else if (settingsIdx >= settingsScroll + 3) settingsScroll = settingsIdx - 2;
}

void adjustLockedSetting(int dir) {
  if (settingsIdx == 0) {
    cfgDotDashMs = constrain((int)cfgDotDashMs + dir * 20, 80, 400);
  } else if (settingsIdx == 1) {
    cfgLetterGapMs = constrain((int)cfgLetterGapMs + dir * 50, 150, 1600);
  } else if (settingsIdx == 2) {
    cfgWordGapMs = constrain((int)cfgWordGapMs + dir * 100, 350, 3000);
  } else if (settingsIdx == 3) {
    cfgVolumeIdx = wrapIdx(cfgVolumeIdx, dir, 4);
  } else if (settingsIdx == 4) {
    cfgThemeIdx = wrapIdx(cfgThemeIdx, dir, 5);
  }
  playUiBeep(dir > 0 ? 1180 : 880, 25);
}

void handleNavLeft() {
  if (currentScreen == SCR_BOOT) {
    currentScreen = SCR_HOME;
    return;
  }
  switch (currentScreen) {
    case SCR_HOME:
      playUiBeep(820, 25);
      homeIdx = wrapIdx(homeIdx, -1, 4);
      syncHomeViewport();
      break;
    case SCR_SETTINGS:
      if (settingsLocked) adjustLockedSetting(-1);
      else {
        playUiBeep(820, 25);
        settingsIdx = wrapIdx(settingsIdx, -1, 6);
        syncSettingsViewport();
      }
      break;
    case SCR_TRANSLATE:
      if (!transActive) {
        playUiBeep(820, 20);
        kbIdx = wrapIdx(kbIdx, -1, 40);
      }
      break;
    case SCR_GAMES:
      playUiBeep(820, 25);
      gamesIdx = wrapIdx(gamesIdx, -1, 2);
      break;
    case SCR_SEND:
      playUiBeep(660, 35);
      sendSymbols = "";
      sendDecoded = "";
      sendSpacePending = false;
      sendStatus = "TEXT CLEARED";
      break;
    case SCR_RECEIVE:
      playUiBeep(660, 35);
      rxDecoded = "";
      rxSymbols = "";
      rxSpacePending = false;
      rxHasUnread = false;
      rxStatus = "INBOX CLEARED";
      break;
    default: break;
  }
}

void handleNavRight() {
  if (currentScreen == SCR_BOOT) {
    currentScreen = SCR_HOME;
    return;
  }
  switch (currentScreen) {
    case SCR_HOME:
      playUiBeep(1040, 25);
      homeIdx = wrapIdx(homeIdx, +1, 4);
      syncHomeViewport();
      break;
    case SCR_SETTINGS:
      if (settingsLocked) adjustLockedSetting(+1);
      else {
        playUiBeep(1040, 25);
        settingsIdx = wrapIdx(settingsIdx, +1, 6);
        syncSettingsViewport();
      }
      break;
    case SCR_TRANSLATE:
      if (!transActive) {
        playUiBeep(1040, 20);
        kbIdx = wrapIdx(kbIdx, +1, 40);
      }
      break;
    case SCR_GAMES:
      playUiBeep(1040, 25);
      gamesIdx = wrapIdx(gamesIdx, +1, 2);
      break;
    case SCR_SEND:
      playUiBeep(760, 30);
      if (sendSymbols.length() > 0) sendSymbols = "";
      else if (sendDecoded.length() > 0) sendDecoded.remove(sendDecoded.length() - 1);
      break;
    case SCR_RECEIVE:
      playUiBeep(1040, 35);
      currentScreen = SCR_TRANSLATE;
      break;
    default: break;
  }
}

void handleBothWhiteChord() {
  stopCwTone();
  stopTranslateTx();
  if ( settingsLocked ) {
    settingsLocked = false;
    saveSettings();
  }
  playUiBeep(620, 60);

  if (currentScreen == SCR_BOOT) {
    currentScreen = SCR_HOME;
  } else if (currentScreen == SCR_HOME) {
    currentScreen = SCR_GAMES;
    gamesIdx = 0;
  } else if (currentScreen == SCR_DINO || currentScreen == SCR_FLAPPY) {
    currentScreen = SCR_GAMES;
  } else {
    currentScreen = SCR_HOME;
  }
}

void handleRedPress() {
  if (currentScreen == SCR_BOOT) {
    playUiBeep(1240, 35);
    currentScreen = SCR_HOME;
    return;
  }

  switch (currentScreen) {
    case SCR_HOME:
      playUiBeep(1240, 45);
      if (homeIdx == 0) {
        currentScreen = SCR_SEND;
        sendSymbols = "";
        sendStatus = "HOLD RED TO KEY";
      } else if (homeIdx == 1) {
        currentScreen = SCR_TRANSLATE;
      } else if (homeIdx == 2) {
        currentScreen = SCR_RECEIVE;
        rxHasUnread = false;
      } else if (homeIdx == 3) {
        currentScreen = SCR_SETTINGS;
        settingsLocked = false;
        syncSettingsViewport();
      }
      break;

    case SCR_SETTINGS:
      if (settingsIdx == 5) {
        cfgDotDashMs   = 200;
        cfgLetterGapMs = 600;
        cfgWordGapMs   = 1400;
        cfgVolumeIdx   = 2;
        settingsLocked = false;
        saveSettings();
        sendEspNowPacket(2);
        resetFlashUntil = millis() + 900;
        playUiBeep(1420, 80);
      } else {
        settingsLocked = !settingsLocked;
        if (!settingsLocked) {
          saveSettings();
          sendEspNowPacket(2);
        }
        playUiBeep(settingsLocked ? 1360 : 960, 45);
      }
      break;

    case SCR_SEND:
      if (!sendKeyDown) {
        sendKeyDown = true;
        sendPressStartMs = millis();
        startCwTone(760);
        sendEspNowPacket(1);
      }
      break;

    case SCR_RECEIVE:
      playUiBeep(1240, 45);
      currentScreen = SCR_SEND;
      break;

    case SCR_TRANSLATE: {
      if (transActive) {
        stopTranslateTx();
        playUiBeep(520, 50);
        return;
      }
      const char* key = KEYBOARD_KEYS[kbIdx];
      playUiBeep(1120, 30);
      if (strcmp(key, "DEL") == 0) {
        if (transText.length() > 0) transText.remove(transText.length() - 1);
      } else if (strcmp(key, "SPC") == 0) {
        if (transText.length() < 18) transText += " ";
      } else if (strcmp(key, "SND") == 0) {
        startTranslateTx();
      } else {
        if (transText.length() < 18) transText += key;
      }
      break;
    }

    case SCR_GAMES:
      playUiBeep(1240, 45);
      if (gamesIdx == 0) {
        initDinoGame();
        currentScreen = SCR_DINO;
      } else {
        initFlappyGame();
        currentScreen = SCR_FLAPPY;
      }
      break;

    case SCR_DINO:
      if (dinoGameOver) initDinoGame();
      else if (dinoY == 0) {
        dinoVy = -5.4f;
        playUiBeep(980, 35);
      }
      break;

    case SCR_FLAPPY:
      if (flappyGameOver) initFlappyGame();
      else {
        birdVy = -3.1f;
        playUiBeep(1050, 30);
      }
      break;
    default: break;
  }
}

void handleRedRelease() {
  if (currentScreen == SCR_SEND && sendKeyDown) {
    uint32_t now = millis();
    uint32_t dur = now - sendPressStartMs;
    sendKeyDown = false;
    stopCwTone();
    sendEspNowPacket(0);

    char sym = (dur < cfgDotDashMs) ? '.' : '-';
    if (sendSymbols.length() < 6) sendSymbols += sym;
    sendLastReleaseMs = now;
    sendStatus = (sym == '.' ? "DOT " : "DASH ") + String(dur) + "MS";
  }
}

bool btnLeftWasDown = false;
bool btnRightWasDown = false;
bool btnRedWasDown = false;
bool chordLatched = false;

void pollButtons() {
  bool leftNow  = (digitalRead(PIN_BTN_LEFT)  == HIGH);
  bool rightNow = (digitalRead(PIN_BTN_RIGHT) == HIGH);
  bool redNow   = (digitalRead(PIN_BTN_RED)   == HIGH);

  if (leftNow && rightNow && !chordLatched) {
    chordLatched = true;
    handleBothWhiteChord();
  }

  if (!leftNow && btnLeftWasDown) {
    if (!chordLatched) handleNavLeft();
    if (!rightNow) chordLatched = false;
  }

  if (!rightNow && btnRightWasDown) {
    if (!chordLatched) handleNavRight();
    if (!leftNow) chordLatched = false;
  }

  if (redNow && !btnRedWasDown) handleRedPress();
  if (!redNow && btnRedWasDown) handleRedRelease();

  btnLeftWasDown  = leftNow;
  btnRightWasDown = rightNow;
  btnRedWasDown   = redNow;
}

void updateMorseDecoders() {
  uint32_t now = millis();

  if (rxEdgePendingOn) {
    rxEdgePendingOn = false;
    rxSyncedDotDash   = rxIncomingDotDash;
    rxSyncedLetterGap = rxIncomingLetterGap;
    rxSyncedWordGap   = rxIncomingWordGap;
    if (!rxCarrierOn) {
      rxCarrierOn = true;
      rxPressStartMs = now;
      startCwTone(860);
    }
  }

  if (rxEdgePendingOff) {
    rxEdgePendingOff = false;
    if (rxCarrierOn) {
      uint32_t pulseMs = now - rxPressStartMs;
      rxCarrierOn = false;
      stopCwTone();

      char sym = (pulseMs < rxSyncedDotDash) ? '.' : '-';
      if (rxSymbols.length() < 6) rxSymbols += sym;
      rxLastReleaseMs = now;
      rxStatus = (sym == '.' ? "RX DOT " : "RX DASH ") + String(pulseMs) + "MS";
    }
  }

  if (rxCarrierOn && (now - rxPressStartMs > 3000)) {
    rxCarrierOn = false;
    stopCwTone();
    rxLastReleaseMs = now;
  }

  if (!sendKeyDown && sendLastReleaseMs > 0) {
    uint32_t idleMs = now - sendLastReleaseMs;
    if (sendSymbols.length() > 0 && idleMs >= cfgLetterGapMs) {
      char c = decodeMorseSymbol(sendSymbols);
      sendDecoded += c;
      if (sendDecoded.length() > 22) sendDecoded.remove(0, sendDecoded.length() - 22);
      sendSymbols = "";
      sendSpacePending = true;
      sendStatus = "LETTER: " + String(c);
      playUiBeep(1320, 20);
    }
    if (sendSpacePending && idleMs >= cfgWordGapMs) {
      if (sendDecoded.length() > 0 && !sendDecoded.endsWith(" ")) {
        sendDecoded += " ";
        if (sendDecoded.length() > 22) sendDecoded.remove(0, sendDecoded.length() - 22);
        sendStatus = "WORD SPACE";
      }
      sendSpacePending = false;
    }
  }

  if (!rxCarrierOn && rxLastReleaseMs > 0) {
    uint32_t rxIdleMs = now - rxLastReleaseMs;
    if (rxSymbols.length() > 0 && rxIdleMs >= rxSyncedLetterGap) {
      char c = decodeMorseSymbol(rxSymbols);
      rxDecoded += c;
      if (rxDecoded.length() > 22) rxDecoded.remove(0, rxDecoded.length() - 22);
      rxSymbols = "";
      rxSpacePending = true;
      rxStatus = "DECODED: " + String(c);
      if (currentScreen != SCR_RECEIVE) rxHasUnread = true;
    }
    if (rxSpacePending && rxIdleMs >= rxSyncedWordGap) {
      if (rxDecoded.length() > 0 && !rxDecoded.endsWith(" ")) {
        rxDecoded += " ";
        if (rxDecoded.length() > 22) rxDecoded.remove(0, rxDecoded.length() - 22);
        rxStatus = "WORD SPACE";
      }
      rxSpacePending = false;
    }
  }
}

int textWidth(const String& s, uint8_t size = 1) {
  return s.length() > 0 ? (int)s.length() * (6 * size) - size : 0;
}

void drawText(const String& s, int x, int y, uint16_t color, uint8_t size = 1) {
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}

void drawCenteredText(const String& s, int y, uint16_t color, uint8_t size = 1) {
  int w = textWidth(s, size);
  drawText(s, (SCREEN_W - w) / 2, y, color, size);
}

void drawBox(int x, int y, int w, int h, uint16_t stroke, int32_t fill = -1) {
  if (fill >= 0) canvas.fillRect(x, y, w, h, (uint16_t)fill);
  canvas.drawRect(x, y, w, h, stroke);
}

void drawRetroCard(int x, int y, int w, int h, uint16_t stroke, uint16_t fill, bool active) {
  const Theme& P = THEMES[cfgThemeIdx];
  drawBox(x, y, w, h, stroke, fill);
  if (active) {
    canvas.fillRect(x + 1, y + 1, 2, 2, P.hi);
    canvas.fillRect(x + w - 3, y + 1, 2, 2, P.hi);
    canvas.fillRect(x + 1, y + h - 3, 2, 2, P.hi);
    canvas.fillRect(x + w - 3, y + h - 3, 2, 2, P.hi);
  }
}

void drawIcon12x12(const uint16_t* icon, int x, int y, uint16_t color) {
  for (int r = 0; r < 12; r++) {
    uint16_t row = icon[r];
    for (int c = 0; c < 12; c++) {
      if (row & (1 << (11 - c))) {
        canvas.drawPixel(x + c, y + r, color);
      }
    }
  }
}

void drawScrollbar(int yStart, int trackH, int totalItems, int visibleItems, int firstIdx) {
  const Theme& P = THEMES[cfgThemeIdx];
  drawBox(154, yStart, 4, trackH, P.grid, P.bg);
  int maxStart = max(1, totalItems - visibleItems);
  int thumbH = max(8, (visibleItems * (trackH - 2)) / totalItems);
  int thumbY = yStart + 1 + ((firstIdx * (trackH - 2 - thumbH)) / maxStart);
  canvas.fillRect(155, thumbY, 2, thumbH, P.hi);
}

void drawTopBar(const String& title, const String& extraTag = "") {
  const Theme& P = THEMES[cfgThemeIdx];
  canvas.fillRect(0, 0, SCREEN_W, 13, P.panel);
  canvas.drawFastHLine(0, 12, SCREEN_W, P.grid);

  bool activeRf = sendKeyDown || transActive || rxCarrierOn;
  if (activeRf) {
    canvas.fillRect(2, 1, 12, 10, P.hi);
    canvas.fillRect(3, 7, 2, 3, P.bg);
    canvas.fillRect(6, 5, 2, 5, P.bg);
    canvas.fillRect(9, 3, 2, 7, P.bg);
  } else {
    canvas.fillRect(3, 7, 2, 3, P.pri);
    canvas.fillRect(6, 5, 2, 5, P.pri);
    canvas.fillRect(9, 3, 2, 7, P.pri);
  }

  drawText(title, 16, 3, P.hi, 1);

  if (extraTag.length() > 0) {
    drawText(extraTag, 74, 3, P.white, 1);
  }

  String pctStr = String(batteryPct) + "%";
  int pw = textWidth(pctStr, 1);
  drawText(pctStr, 143 - pw, 3, P.pri, 1);

  int bars = (batteryPct + 32) / 33;
  uint16_t batColor = (batteryPct <= 20) ? P.accent : P.pri;
  canvas.fillRect(145, 3, 11, 7, batColor);
  canvas.fillRect(156, 5, 2, 3, batColor);
  canvas.fillRect(146, 4, 9, 5, P.panel);
  if (bars >= 1) canvas.fillRect(147, 5, 2, 3, batColor);
  if (bars >= 2) canvas.fillRect(150, 5, 2, 3, batColor);
  if (bars >= 3) canvas.fillRect(153, 5, 2, 3, batColor);
}

void drawBottomBar(const String& lBadge, const String& lText, const String& rBadge, const String& rText) {
  const Theme& P = THEMES[cfgThemeIdx];
  canvas.fillRect(0, 115, SCREEN_W, 13, P.panel);
  canvas.drawFastHLine(0, 115, SCREEN_W, P.grid);

  int lbW = textWidth(lBadge, 1) + 4;
  canvas.fillRect(3, 117, lbW, 9, P.dim);
  drawText(lBadge, 5, 118, P.bg, 1);
  drawText(lText, 5 + lbW + 2, 118, P.pri, 1);

  int rlW = textWidth(rText, 1);
  int rbW = textWidth(rBadge, 1) + 4;
  int rx = SCREEN_W - (rbW + 2 + rlW) - 3;

  canvas.fillRect(rx, 117, rbW, 9, P.hi);
  drawText(rBadge, rx + 2, 118, P.bg, 1);
  drawText(rText, rx + rbW + 2, 118, P.hi, 1);
}

void renderBootScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  uint32_t elapsed = millis() - bootStartMs;
  float progress = constrain((float)elapsed / 2000.0f, 0.0f, 1.0f);

  drawRetroCard(4, 4, SCREEN_W - 8, SCREEN_H - 8, P.grid, P.bg, true);

  int cx = 80;
  int waveStep = (elapsed / 200) % 4;

  canvas.fillRect(cx - 1, 22, 2, 22, P.pri);
  canvas.drawLine(cx - 8, 44, cx, 26, P.pri);
  canvas.drawLine(cx + 8, 44, cx, 26, P.pri);
  canvas.drawFastHLine(cx - 10, 44, 21, P.pri);
  canvas.fillRect(cx - 2, 19, 4, 4, P.hi);

  if (waveStep >= 1) {
    canvas.fillRect(cx - 9, 20, 2, 6, P.pri);
    canvas.fillRect(cx + 7, 20, 2, 6, P.pri);
  }
  if (waveStep >= 2) {
    canvas.fillRect(cx - 15, 18, 2, 10, P.hi);
    canvas.fillRect(cx + 13, 18, 2, 10, P.hi);
  }
  if (waveStep >= 3) {
    canvas.fillRect(cx - 21, 16, 2, 14, P.accent);
    canvas.fillRect(cx + 19, 16, 2, 14, P.accent);
  }

  drawCenteredText("MORSE", 54, P.hi, 2);
  drawCenteredText("TRANSCEIVER", 74, P.white, 1);

  drawBox(24, 94, 112, 8, P.pri, P.panel);
  int segs = (int)(progress * 13);
  for (int s = 0; s < segs; s++) {
    canvas.fillRect(26 + s * 8, 96, 6, 4, (s == segs - 1) ? P.hi : P.pri);
  }

  if (elapsed >= 2000) {
    currentScreen = SCR_HOME;
  }
}

void renderHomeScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  drawTopBar("MORSE TRANSCEIVER");

  const char* labels[4] = { "SEND MORSE", "TRANSLATE", "INBOX (RX)", "SETTINGS" };
  const char* descs[4]  = { "TAP DOTS & DASHES", "TEXT TO MORSE TX", "RECEIVED MESSAGES", "TIMING & THEME" };
  const uint16_t* icons[4] = { ICON_SEND_MORSE, ICON_TRANSLATE, ICON_INBOX, ICON_SETTINGS };

  for (int v = 0; v < 3; v++) {
    int idx = homeScroll + v;
    if (idx >= 4) break;

    int y = 16 + v * 32;
    bool sel = (idx == homeIdx);

    drawRetroCard(4, y, 147, 29, sel ? P.hi : P.grid, sel ? P.bg : P.panel, sel);
    drawBox(9, y + 5, 18, 18, sel ? P.hi : P.dim, P.panel);
    drawIcon12x12(icons[idx], 12, y + 8, sel ? P.hi : P.pri);

    drawText(labels[idx], 31, y + 5, sel ? P.hi : P.pri, 1);
    drawText(descs[idx],  31, y + 16, sel ? P.white : P.dim, 1);

    if (idx == 2 && rxHasUnread) {
      canvas.fillRect(118, y + 4, 23, 9, P.hi);
      drawText("NEW", 121, y + 5, P.bg, 1);
    } else if (sel) {
      drawText("<", 141, y + 10, P.hi, 1);
    }
  }

  drawScrollbar(16, 93, 4, 3, homeScroll);
  drawBottomBar("L/R", "MENU", "RED", "OPEN");
}

void renderSendScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  drawTopBar("SEND MORSE");

  drawRetroCard(4, 16, 152, 27, P.grid, P.panel, sendKeyDown);
  drawText("MORSE:", 8, 20, P.dim, 1);
  drawText(sendSymbols.length() ? sendSymbols : "-", 46, 20, P.hi, 1);

  String threshTag = "DOT<" + String(cfgDotDashMs);
  drawText(threshTag, 152 - textWidth(threshTag, 1), 20, P.dim, 1);

  if (sendKeyDown) {
    uint32_t heldMs = millis() - sendPressStartMs;
    int fillW = min(144, (int)((heldMs * 144L) / (cfgDotDashMs * 2)));
    bool isDash = (heldMs >= cfgDotDashMs);
    drawBox(8, 31, 144, 8, P.dim, P.bg);
    canvas.fillRect(9, 32, max(2, fillW - 2), 6, isDash ? P.hi : P.pri);
  } else {
    drawText(sendStatus, 8, 32, P.pri, 1);
  }

  drawBox(4, 46, 152, 11, P.grid, P.panel);
  drawText("DECODED TEXT", 8, 48, P.hi, 1);
  drawBox(4, 56, 152, 55, P.pri, P.bg);

  String line1 = sendDecoded.substring(0, min(11, (int)sendDecoded.length()));
  String line2 = (sendDecoded.length() > 11) ? sendDecoded.substring(11, min(22, (int)sendDecoded.length())) : "";
  drawText(line1, 10, 63, P.white, 2);
  drawText(line2, 10, 85, P.white, 2);

  if ((millis() / 350) % 2 == 0) {
    int len = sendDecoded.length();
    int curLine = (len >= 11) ? 1 : 0;
    int curCol  = (len >= 11) ? (len - 11) : len;
    int cx = 10 + curCol * 12;
    int cy = (curLine == 0) ? 63 : 85;
    if (cx < 146) canvas.fillRect(cx, cy, 8, 14, P.hi);
  }

  drawBottomBar("L:CLR", "R:DEL", "RED", "HOLD");
}

void renderTranslateScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  const char* activeKey = KEYBOARD_KEYS[kbIdx];
  const char* mCode = (strlen(activeKey) == 1) ? encodeCharToMorse(activeKey[0]) : nullptr;
  String keyTag = mCode ? (String(activeKey) + String(mCode)) : String(activeKey);

  drawTopBar("TRANSLATE", keyTag);

  drawRetroCard(4, 15, 152, 20, P.pri, P.bg, transActive);
  if (transActive) {
    drawText("TX: " + transPreview, 8, 21, P.hi, 1);
  } else {
    String cur = ((millis() / 400) % 2 == 0) ? "_" : "";
    drawText(transText + cur, 8, 21, P.white, 1);
  }

  const int startX = 4, startY = 38, cellW = 19, cellH = 15;
  for (int i = 0; i < 40; i++) {
    int col = i % 8;
    int row = i / 8;
    int x = startX + col * cellW;
    int y = startY + row * cellH;
    const char* kLabel = KEYBOARD_KEYS[i];
    bool sel = (i == kbIdx);

    bool isAction = (strlen(kLabel) > 1);
    bool isDigit  = (strlen(kLabel) == 1 && kLabel[0] >= '0' && kLabel[0] <= '9');

    uint16_t fill   = P.panel;
    uint16_t stroke = P.grid;
    uint16_t tCol   = isDigit ? P.white : P.pri;

    if (isAction) tCol = (strcmp(kLabel, "SND") == 0) ? P.accent : P.hi;
    if (sel) {
      fill   = P.hi;
      stroke = P.white;
      tCol   = P.bg;
    }

    drawBox(x, y, cellW - 1, cellH - 1, stroke, fill);
    int tw = textWidth(kLabel, 1);
    int tx = x + max(0, (cellW - 1 - tw) / 2);
    drawText(kLabel, tx, y + 4, tCol, 1);
  }

  drawBottomBar("L/R", "KEY", "RED", transActive ? "STOP" : "SELECT");
}

void renderReceiveScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  drawTopBar("INBOX (RX)");

  drawRetroCard(4, 16, 152, 27, P.grid, P.panel, rxCarrierOn);
  drawText("PULSE:", 8, 20, P.dim, 1);
  drawText(rxSymbols.length() ? rxSymbols : "-", 46, 20, P.hi, 1);

  String syncTag = "SYNC:" + String(rxSyncedDotDash);
  drawText(syncTag, 152 - textWidth(syncTag, 1), 20, P.dim, 1);
  drawText(rxStatus, 8, 32, P.pri, 1);

  drawBox(4, 46, 152, 11, P.grid, P.panel);
  drawText("DECODED FROM PULSES", 8, 48, P.hi, 1);
  drawBox(4, 56, 152, 55, P.pri, P.bg);

  if (rxDecoded.length() == 0) {
    drawCenteredText("LISTENING FOR PULSES", 78, P.dim, 1);
  } else {
    String line1 = rxDecoded.substring(0, min(11, (int)rxDecoded.length()));
    String line2 = (rxDecoded.length() > 11) ? rxDecoded.substring(11, min(22, (int)rxDecoded.length())) : "";
    drawText(line1, 10, 63, P.white, 2);
    drawText(line2, 10, 85, P.white, 2);
  }

  drawBottomBar("L:CLR", "R:TYPE", "RED", "KEY");
}

void renderSettingsScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  drawTopBar("SETTINGS");

  bool isResetFlashing = (millis() < resetFlashUntil);
  const char* names[6] = {
    "1.DOT/DASH", "2.LETTER GAP", "3.WORD GAP", "4.VOLUME", "5.THEME", "6.DEFAULTS"
  };

  String vals[6] = {
    String(cfgDotDashMs) + "MS",
    String(cfgLetterGapMs) + "MS",
    String(cfgWordGapMs) + "MS",
    String(VOLUME_LABELS[cfgVolumeIdx]),
    String(THEMES[cfgThemeIdx].name),
    isResetFlashing ? "SYNCED!" : "RESET"
  };

  float pcts[6] = {
    (float)(cfgDotDashMs - 80) / (400 - 80),
    (float)(cfgLetterGapMs - 150) / (1600 - 150),
    (float)(cfgWordGapMs - 350) / (3000 - 350),
    (float)cfgVolumeIdx / 3.0f,
    (float)(cfgThemeIdx + 1) / 5.0f,
    isResetFlashing ? 1.0f : 0.0f
  };

  for (int v = 0; v < 3; v++) {
    int idx = settingsScroll + v;
    if (idx >= 6) break;

    int y = 16 + v * 32;
    bool sel = (idx == settingsIdx);
    bool lockedHere = (sel && settingsLocked);

    drawRetroCard(4, y, 147, 29, lockedHere ? P.accent : (sel ? P.hi : P.grid), sel ? P.bg : P.panel, sel);

    String prefix = sel ? (lockedHere ? "*" : ">") : " ";
    drawText(prefix + names[idx], 8, y + 6, sel ? P.hi : P.pri, 1);

    drawBox(14, y + 18, 128, 5, P.grid, P.bg);
    int barW = (int)(126 * constrain(pcts[idx], 0.0f, 1.0f));
    canvas.fillRect(15, y + 19, barW, 3, lockedHere ? P.accent : (sel ? P.hi : P.dim));

    String valStr = lockedHere ? ("<" + vals[idx] + ">") : vals[idx];
    int vw = textWidth(valStr, 1);
    if (lockedHere || (idx == 5 && isResetFlashing)) {
      canvas.fillRect(145 - vw - 3, y + 4, vw + 4, 11, P.hi);
      drawText(valStr, 145 - vw - 1, y + 6, P.bg, 1);
    } else {
      drawText(valStr, 145 - vw - 1, y + 6, sel ? P.white : P.pri, 1);
    }
  }

  drawScrollbar(16, 93, 6, 3, settingsScroll);

  if (settingsLocked) drawBottomBar("L/R", "CHANGE", "RED", "SAVE");
  else if (settingsIdx == 5) drawBottomBar("L/R", "SCROLL", "RED", "RESET");
  else drawBottomBar("L/R", "SCROLL", "RED", "EDIT");
}

void renderGamesMenuScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  drawTopBar("ARCADE GAMES");
  drawCenteredText("* SELECT A GAME *", 19, P.hi, 1);

  const char* gNames[2] = { "CHROME DINO", "FLAPPY BIRD" };
  const char* gDescs[2] = { "JUMP OVER CACTI", "FLY THROUGH PIPES" };

  for (int i = 0; i < 2; i++) {
    int y = 34 + i * 34;
    bool sel = (i == gamesIdx);
    drawRetroCard(8, y, 144, 28, sel ? P.hi : P.grid, sel ? P.bg : P.panel, sel);
    drawText(String(sel ? "> " : "  ") + gNames[i], 14, y + 5, sel ? P.hi : P.pri, 1);
    drawText(gDescs[i], 26, y + 16, sel ? P.white : P.dim, 1);
  }

  drawBottomBar("L/R", "CHOOSE", "RED", "PLAY");
}

void renderDinoScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  updateDinoGame();

  drawTopBar("CHROME DINO");
  drawText("SCORE:" + String(dinoScore) + "  BEST:" + String(dinoHigh), 6, 17, P.pri, 1);

  const int groundY = 106;
  canvas.fillRect(0, groundY, SCREEN_W, 2, P.dim);

  int dinoX = 18;
  int dy = groundY - 14 + (int)dinoY;
  canvas.fillRect(dinoX + 4, dy, 6, 5, P.hi);
  canvas.fillRect(dinoX + 2, dy + 5, 6, 6, P.hi);
  canvas.fillRect(dinoX, dy + 7, 2, 3, P.hi);
  canvas.fillRect(dinoX + 2, dy + 11, 2, 3, P.hi);
  canvas.fillRect(dinoX + 6, dy + 11, 2, 3, P.hi);

  for (int i = 0; i < 4; i++) {
    if (dinoObs[i].active) {
      canvas.fillRect((int)dinoObs[i].x, groundY - dinoObs[i].h, dinoObs[i].w, dinoObs[i].h, P.pri);
    }
  }

  if (dinoGameOver) {
    drawRetroCard(22, 40, 116, 38, P.accent, P.bg, true);
    drawCenteredText("GAME OVER", 47, P.accent, 1);
    drawCenteredText("RED: TRY AGAIN", 61, P.white, 1);
  }

  drawBottomBar("L+R", "EXIT", "RED", "JUMP");
}

void renderFlappyScreen() {
  const Theme& P = THEMES[cfgThemeIdx];
  updateFlappyGame();

  drawTopBar("FLAPPY BIRD");
  drawText("SCORE:" + String(flappyScore), 6, 17, P.pri, 1);

  for (int i = 0; i < 3; i++) {
    if (flappyPipes[i].active) {
      int px = (int)flappyPipes[i].x;
      canvas.fillRect(px, 14, 12, flappyPipes[i].gapY - 14, P.dim);
      canvas.fillRect(px, flappyPipes[i].gapY + flappyPipes[i].gapH, 12, 114 - (flappyPipes[i].gapY + flappyPipes[i].gapH), P.dim);
    }
  }

  int by = (int)birdY;
  canvas.fillRect(26, by, 7, 6, P.hi);
  canvas.fillRect(31, by + 1, 2, 2, P.white);

  if (flappyGameOver) {
    drawRetroCard(22, 40, 116, 38, P.accent, P.bg, true);
    drawCenteredText("GAME OVER", 47, P.accent, 1);
    drawCenteredText("RED: TRY AGAIN", 61, P.white, 1);
  }

  drawBottomBar("L+R", "EXIT", "RED", "FLAP");
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_RED,   INPUT_PULLUP);
  pinMode(PIN_BAT_ADC,   INPUT);

  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_SPEAKER, 1000, 8);
  #else
    ledcSetup(0, 1000, 8);
    ledcAttachPin(PIN_SPEAKER, 0);
  #endif

  loadSettings();
  updateBatteryPercentage();

  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  canvas.setTextWrap(false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onEspNowRecv);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  bootStartMs = millis();
  playUiBeep(1040, 60);
}

void loop() {
  uint32_t frameStart = millis();

  pollButtons();
  updateTranslateTx();
  updateMorseDecoders();
  updateAudioTimer();
  updateBatteryPercentage();

  canvas.fillScreen(THEMES[cfgThemeIdx].bg);

  switch (currentScreen) {
    case SCR_BOOT:      renderBootScreen();      break;
    case SCR_HOME:      renderHomeScreen();      break;
    case SCR_SEND:      renderSendScreen();      break;
    case SCR_TRANSLATE: renderTranslateScreen(); break;
    case SCR_RECEIVE:   renderReceiveScreen();   break;
    case SCR_SETTINGS:  renderSettingsScreen();  break;
    case SCR_GAMES:     renderGamesMenuScreen(); break;
    case SCR_DINO:      renderDinoScreen();      break;
    case SCR_FLAPPY:    renderFlappyScreen();    break;
  }

  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_W, SCREEN_H);

  uint32_t frameTime = millis() - frameStart;
  if (frameTime < 22) {
    delay(22 - frameTime);
  }
}
