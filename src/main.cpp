// TTGO T-Display - Wetterstation
// Standort: am Handy gewaehlt (Einrichtung), sonst grob ueber die Internet-Adresse, sonst Cottbus
//   Taster GPIO 0:  kurz = naechste Seite, 2 s halten = Info (Adresse der Einstellungsseite, Build)
//   Taster GPIO 35: kurz = Helligkeit, 3 s halten = WLAN und Ort neu einrichten
// Seiten: Jetzt, 24 Stunden, naechste Tage, Sonne & Mond, Luft & Pollen (wechseln auch von allein)
// Wetterdaten: open-meteo.com, Warnungen: Deutscher Wetterdienst ueber brightsky.dev
// WLAN: Einrichtung per Handy ueber das WLAN "Wetterstation-Setup" (siehe wlan.cpp)
// Einstellungen: http://wetterstation.local (siehe web.cpp)
// Updates: von selbst aus dem Internet (netzupdate.cpp) oder ueber WLAN mit "env:tdisplay_wlan"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "wetter.h"
#include "bilder.h"
#include "wlan.h"
#include "ort.h"
#include "einstellungen.h"
#include "netzupdate.h"
#include "web.h"

#ifndef OTA_PASSWORT
#define OTA_PASSWORT ""
#endif

// Pins (siehe INFO.md)
const int TASTER_SEITE = 0;
const int TASTER_LICHT = 35;
const int AKKU_PIN     = 34;
const int AKKU_EIN     = 14;  // schaltet den Spannungsteiler am Akku ein
const int LICHT_PIN    = 4;   // Hintergrundlicht

const float AKKU_FAKTOR = 2.0f;        // Teiler 1:2 - mit Multimeter nachmessen und anpassen
const uint32_t WECHSEL_DAUER = 380;    // Schiebe-Animation zwischen den Seiten

const uint16_t WEISS = TFT_WHITE;
const uint16_t HELL  = 0xDF7E;  // helles Blaugrau

TFT_eSPI tft;
TFT_eSprite bild(&tft);  // ganzer Bildschirm im Speicher - so flackert nichts

int seite = 0, alteSeite = 0;
bool wechsel = false;
uint32_t wechselStart = 0;
uint32_t seiteSeit = 0;
uint32_t letzterTastendruck = 0;

const int HELLIGKEIT[] = {255, 140, 60, 15};
const int HELLIGKEIT_NACHT = 4;
uint32_t lichtAnzeigeBis = 0;
uint32_t infoBis = 0;

float akkuVolt = 0;
uint32_t akkuZeit = 0;

static Wetter w;

static const char *WOCHENTAG[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
static const char *WOCHENTAG_KURZ[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
static const char *WOCHENTAG_EN[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *WOCHENTAG_KURZ_EN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

float akkuSpannung() {
  uint32_t summe = 0;
  for (int i = 0; i < 16; i++) summe += analogReadMilliVolts(AKKU_PIN);
  return summe / 16.0f * AKKU_FAKTOR / 1000.0f;
}

uint16_t hintergrund(int x, int y) {
  return bild.readPixel(constrain(x, 0, bild.width() - 1), constrain(y, 0, bild.height() - 1));
}

// Text auf eine Breite kuerzen (mit der gerade eingestellten Schrift), lange Ortsnamen z. B.
String kuerzen(String text, int maxBreite) {
  if (breite(bild, text.c_str()) <= maxBreite) return text;
  while (text.length() > 1 && breite(bild, (text + ".").c_str()) > maxBreite) {
    int n = text.length() - 1;
    while (n > 0 && ((uint8_t)text[n] & 0xC0) == 0x80) n--;  // keinen Umlaut halb abschneiden
    text.remove(n);
  }
  return text + ".";
}

void tropfenSymbol(int x, int y, uint16_t f) {
  bild.fillTriangle(x - 3, y - 1, x + 3, y - 1, x, y - 7, f);
  bild.fillSmoothCircle(x, y, 3, f);
}

// Minuten seit Mitternacht aus "06:56"
int minuten(const char *hhmm) { return atoi(hhmm) * 60 + atoi(hhmm + 3); }

// Titel oben links. Gibt es eine DWD-Warnung, wechselt er alle 2,5 s mit der Warnung.
void titel(int ox, const char *text, uint32_t t) {
  bild.setTextFont(2);
  if (w.warnung[0] && (t / 2500) % 2) {
    uint16_t f = w.warnStufe >= 3 ? farbe(255, 80, 70) : farbe(255, 160, 40);
    float blink = 0.6f + 0.4f * sinf(t * 0.012f);
    zeichneWarnzeichen(bild, ox + 12, 8, 12, bild.alphaBlend(255 * blink, f, hintergrund(ox + 12, 8)));
    schreibe(bild, kuerzen(TX(w.warnung, w.warnungEn), 170).c_str(), ox + 22, 13, f, LINKS);
  } else {
    schreibe(bild, kuerzen(text, 175).c_str(), ox + 6, 13, WEISS, LINKS);
  }
}

// ------------------------------------------------------------
// Seite 1: Jetzt
// ------------------------------------------------------------
void seiteJetzt(int ox, uint32_t t) {
  titel(ox, w.ort, t);

  zeichneBild(bild, bildFuerCode(w.code), w.tag, ox + 58, 74, 96, t);

  bild.setFreeFont(&FreeSansBold24pt7b);
  schreibeTemp(bild, w.temp, ox + 118, 66, WEISS, LINKS);

  const char *text = textFuerCode(w.code, w.tag);
  bild.setFreeFont(&FreeSans9pt7b);
  if (breite(bild, text) > 118) bild.setTextFont(2);
  schreibe(bild, text, ox + 118, 88, WEISS, LINKS);

  bild.setTextFont(2);
  const char *gef = TX("Gefühlt", "Feels like");
  schreibe(bild, gef, ox + 118, 106, HELL, LINKS);
  schreibeTemp(bild, w.gefuehlt, ox + 122 + breite(bild, gef), 106, HELL, LINKS);

  // Regen in den naechsten 2 Stunden hat Vorrang vor Wind und Luftfeuchte
  char z[24];
  if (w.regenArt) {
    snprintf(z, sizeof(z), w.regenArt == 1 ? TX("Regen ab %s", "Rain from %s") : TX("Regen bis %s", "Rain until %s"),
             w.regenZeit);
    tropfenSymbol(ox + 122, 118, farbe(130, 190, 255));
    schreibe(bild, z, ox + 130, 122, farbe(150, 205, 255), LINKS);
    return;
  }
  snprintf(z, sizeof(z), TX("Wind %d km/h", "Wind %d km/h"), (int)lroundf(w.wind));
  schreibe(bild, z, ox + 118, 122, HELL, LINKS);
  snprintf(z, sizeof(z), "%d%%", w.feuchte);
  schreibe(bild, z, ox + 236, 122, HELL, RECHTS);
  tropfenSymbol(ox + 230 - breite(bild, z), 118, farbe(130, 190, 255));
}

// ------------------------------------------------------------
// Seite 2: naechste 24 Stunden als Kurve
// ------------------------------------------------------------
void seiteStunden(int ox, uint32_t t) {
  titel(ox, TX("Nächste 24 Stunden", "Next 24 hours"), t);

  int n = w.stunden;
  if (n < 2) return;

  float tmin = w.stdTemp[0], tmax = w.stdTemp[0];
  for (int i = 1; i < n; i++) {
    tmin = min(tmin, w.stdTemp[i]);
    tmax = max(tmax, w.stdTemp[i]);
  }
  if (tmax - tmin < 4) {  // flache Kurve nicht zu stark aufblasen
    float m = (tmax + tmin) / 2;
    tmin = m - 2;
    tmax = m + 2;
  }

  const int X0 = 14, X1 = 226, YO = 42, YU = 90;
  auto px = [&](float i) { return ox + X0 + (X1 - X0) * i / (n - 1); };
  auto py = [&](float v) { return YU - (v - tmin) / (tmax - tmin) * (YU - YO); };
  auto tempBei = [&](float f) {
    int i = min((int)f, n - 2);
    float r = f - i;
    return w.stdTemp[i] * (1 - r) + w.stdTemp[i + 1] * r;
  };

  // Kurve baut sich von links nach rechts auf
  float fort = min(1.0f, (t - seiteSeit) / 900.0f);
  fort = 1 - powf(1 - fort, 3);
  float xEnde = px(0) + (X1 - X0) * fort;

  // Flaeche unter der Kurve, nach unten durchsichtiger
  uint16_t orange = farbe(255, 168, 64);
  for (int x = max(0, (int)px(0)); x <= min(239, (int)xEnde); x++) {
    float f = (x - px(0)) / (X1 - X0) * (n - 1);
    int yc = py(tempBei(f));
    for (int y = yc; y <= YU; y++) {
      uint8_t a = 20 + 120 * (1 - (y - yc) / (YU - yc + 1.0f));
      bild.drawPixel(x, y, bild.alphaBlend(a, orange, bild.readPixel(x, y)));
    }
  }

  // Linie
  for (int i = 0; i < n - 1; i++) {
    float xa = px(i), xb = px(i + 1);
    if (xa > xEnde) break;
    float yb = py(w.stdTemp[i + 1]);
    if (xb > xEnde) {
      float f = (xEnde - px(0)) / (X1 - X0) * (n - 1);
      xb = xEnde;
      yb = py(tempBei(f));
    }
    bild.drawWideLine(xa, py(w.stdTemp[i]), xb, yb, 2.5f, farbe(255, 214, 130));
  }

  // Regenwahrscheinlichkeit als Balken
  bool regen = false;
  for (int i = 0; i < n; i++) regen |= w.stdRegen[i] > 0;
  uint16_t blau = farbe(110, 170, 255);
  if (regen) {
    for (int i = 0; i < n; i++) {
      int h = w.stdRegen[i] * 14 / 100;
      if (h <= 0 || px(i) > xEnde) continue;
      bild.fillRect(px(i) - 3, 109 - h, 6, h, bild.alphaBlend(200, blau, hintergrund(px(i), 105)));
    }
  } else {
    bild.setTextFont(2);
    schreibe(bild, TX("Kein Regen erwartet", "No rain expected"), ox + 120, 108, HELL, MITTE);
  }

  // Punkte alle 6 Stunden mit Temperatur und Uhrzeit
  float puls = 0.5f + 0.5f * sinf(t * 0.005f);
  for (int i = 0; i < n; i++) {
    bool marke = i == 0 || (i >= 3 && w.stdStunde[i] % 6 == 0);
    if (!marke || px(i) > xEnde) continue;
    float x = px(i), y = py(w.stdTemp[i]);
    if (i == 0) bild.fillSmoothCircle(x, y, 4 + 3 * puls, bild.alphaBlend(90 - 60 * puls, WEISS, hintergrund(x, y)));
    bild.fillSmoothCircle(x, y, 3, WEISS);
    bild.setTextFont(2);
    schreibeTemp(bild, w.stdTemp[i], x, y - 6, WEISS, MITTE);
    char uhr[8];
    if (i == 0) strcpy(uhr, TX("jetzt", "now"));
    else snprintf(uhr, sizeof(uhr), "%02d", w.stdStunde[i]);
    schreibe(bild, uhr, x, 125, HELL, MITTE);
  }
}

// ------------------------------------------------------------
// Seite 3: heute, morgen, uebermorgen
// ------------------------------------------------------------
void seiteTage(int ox, uint32_t t) {
  titel(ox, TX("Die nächsten Tage", "Next days"), t);

  for (int i = 0; i < 3; i++) {
    int x = ox + 6 + i * 78, mx = x + 36;
    uint16_t karte = bild.alphaBlend(i == 0 ? 95 : 70, TFT_BLACK, hintergrund(mx, 70));
    bild.fillSmoothRoundRect(x, 20, 72, 106, 8, karte);

    const char *name = i == 0 ? TX("Heute", "Today") : i == 1 ? TX("Morgen", "Tomorrow") : TX(WOCHENTAG, WOCHENTAG_EN)[w.tagWochentag[i]];
    bild.setTextFont(2);
    if (breite(bild, name) > 66) name = TX(WOCHENTAG_KURZ, WOCHENTAG_KURZ_EN)[w.tagWochentag[i]];
    schreibe(bild, name, mx, 36, WEISS, MITTE);

    zeichneBild(bild, bildFuerCode(w.tagCode[i]), true, mx, 62, 44, t + i * 700);

    bild.setFreeFont(&FreeSansBold12pt7b);
    schreibeTemp(bild, w.tagMax[i], mx, 101, WEISS, MITTE);

    bild.setTextFont(2);
    schreibeTemp(bild, w.tagMin[i], x + 18, 118, HELL, MITTE);
    char z[8];
    snprintf(z, sizeof(z), "%d%%", w.tagRegen[i]);
    schreibe(bild, z, x + 67, 118, HELL, RECHTS);
    tropfenSymbol(x + 61 - breite(bild, z), 114, farbe(130, 190, 255));
  }
}

// ------------------------------------------------------------
// Seite 4: Sonne & Mond
// ------------------------------------------------------------
void seiteSonne(int ox, uint32_t t) {
  titel(ox, TX("Sonne & Mond", "Sun & Moon"), t);

  // Sonnenbogen: links Aufgang, rechts Untergang, die Sonne steht dort, wo sie gerade ist
  const int CX = ox + 62, CY = 96, R = 46;
  int auf = minuten(w.sonneAuf), unter = minuten(w.sonneUnter);
  struct tm zeit;
  int jetzt = getLocalTime(&zeit, 0) ? zeit.tm_hour * 60 + zeit.tm_min : -1;
  float frac = unter > auf && jetzt >= 0 ? float(jetzt - auf) / (unter - auf) : -1;

  bild.drawFastHLine(ox + 6, CY, 112, bild.alphaBlend(120, WEISS, hintergrund(CX, CY)));
  for (int i = 0; i <= 60; i++) {  // gestrichelter Bogen
    if (i % 2) continue;
    float a = PI * i / 60;
    bild.drawPixel(CX - R * cosf(a), CY - R * sinf(a), bild.alphaBlend(110, WEISS, hintergrund(CX, CY - R)));
  }
  if (frac >= 0 && frac <= 1) {
    // schon gelaufenes Stueck hell nachzeichnen
    for (float a = 0; a < PI * frac; a += 0.02f) {
      bild.drawWideLine(CX - R * cosf(a), CY - R * sinf(a), CX - R * cosf(a + 0.02f), CY - R * sinf(a + 0.02f), 2,
                        farbe(255, 205, 90));
    }
    zeichneBild(bild, KLAR, true, CX - R * cosf(PI * frac), CY - R * sinf(PI * frac), 34, t);
  } else {
    zeichneBild(bild, KLAR, false, CX, CY - 24, 30, t);  // nachts: Mond ueber dem Horizont
  }

  bild.setTextFont(2);
  schreibe(bild, w.sonneAuf, ox + 6, 112, WEISS, LINKS);
  schreibe(bild, w.sonneUnter, ox + 118, 112, WEISS, RECHTS);
  int dauer = unter - auf;
  if (dauer > 0) {
    char z[24];
    snprintf(z, sizeof(z), TX("Tag %d h %02d min", "Day %d h %02d min"), dauer / 60, dauer % 60);
    schreibe(bild, z, ox + 6, 128, HELL, LINKS);
  }

  // Mondphase rechts
  time_t sek;
  time(&sek);
  float p = mondphase(sek);
  zeichneMondphase(bild, ox + 185, 54, 22, p);
  schreibe(bild, mondName(p), ox + 185, 96, WEISS, MITTE);
  char z[32];
  snprintf(z, sizeof(z), TX("%d %% beleuchtet", "%d %% lit"), (int)lroundf((1 - cosf(p * 2 * PI)) / 2 * 100));
  schreibe(bild, z, ox + 185, 112, HELL, MITTE);
  snprintf(z, sizeof(z), TX("UV-Index %d", "UV index %d"), (int)lroundf(w.uv));
  schreibe(bild, z, ox + 185, 128, w.uv >= 6 ? farbe(255, 160, 40) : HELL, MITTE);
}

// ------------------------------------------------------------
// Seite 5: Luft & Pollen
// ------------------------------------------------------------
void seiteLuft(int ox, uint32_t t) {
  titel(ox, TX("Luft & Pollen", "Air & Pollen"), t);
  bild.setTextFont(2);
  if (!w.luftGueltig) {
    schreibe(bild, TX("Für diesen Ort gibt es", "No air quality data"), ox + 120, 64, HELL, MITTE);
    schreibe(bild, TX("keine Luftdaten", "for this place"), ox + 120, 82, HELL, MITTE);
    return;
  }

  // Europaeischer Luftqualitaetsindex als Ring
  static const char *STUFE_DE[] = {"Sehr gut", "Gut", "Mäßig", "Schlecht", "Sehr schlecht", "Extrem"};
  static const char *STUFE_EN[] = {"Very good", "Good", "Moderate", "Poor", "Very poor", "Extreme"};
  const char **STUFE = TX(STUFE_DE, STUFE_EN);
  static const uint16_t FARBE[] = {0x4F6A, 0x8F68, 0xFE84, 0xFC64, 0xF9E8, 0xA01F};
  int stufe = constrain(w.aqi / 20, 0, 5);
  uint16_t f = FARBE[stufe];
  const int CX = ox + 56, CY = 70, R = 36;
  float fort = min(1.0f, (t - seiteSeit) / 900.0f);  // Ring fuellt sich beim Umblaettern
  uint16_t hg = hintergrund(CX, CY - R);
  bild.drawSmoothArc(CX, CY, R, R - 7, 30, 330, bild.alphaBlend(60, WEISS, hg), hg, true);
  int ende = 30 + 300 * min(1.0f, w.aqi / 100.0f) * (1 - powf(1 - fort, 3));
  if (ende > 31) bild.drawSmoothArc(CX, CY, R, R - 7, 30, ende, f, hg, true);
  bild.setFreeFont(&FreeSansBold12pt7b);
  char z[24];
  snprintf(z, sizeof(z), "%d", w.aqi);
  schreibe(bild, z, CX, CY + 8, WEISS, MITTE);
  bild.setTextFont(2);
  schreibe(bild, STUFE[stufe], CX, 118, f, MITTE);
  snprintf(z, sizeof(z), TX("Feinstaub %d", "PM2.5: %d"), (int)lroundf(w.pm25));
  schreibe(bild, z, CX, 133, HELL, MITTE);

  // Pollen: drei Stufen je Art
  for (int i = 0; i < 5; i++) {
    int y = 40 + i * 18;
    float k = w.pollen[i];
    int lvl = k < 1 ? 0 : k <= 10 ? 1 : k <= 50 ? 2 : 3;
    schreibe(bild, pollenName(i), ox + 116, y, lvl ? WEISS : HELL, LINKS);
    static const uint16_t PF[] = {0, 0x8F68, 0xFE84, 0xF9E8};
    for (int s = 0; s < 3; s++) {
      uint16_t c = s < lvl ? PF[lvl] : bild.alphaBlend(50, WEISS, hintergrund(ox + 180, y - 5));
      bild.fillSmoothRoundRect(ox + 178 + s * 19, y - 9, 16, 7, 3, c);
    }
  }
}

void zeichneSeite(int s, int ox, uint32_t t) {
  if (ox <= -240 || ox >= 240) return;
  switch (s) {
    case 0: seiteJetzt(ox, t); break;
    case 1: seiteStunden(ox, t); break;
    case 2: seiteTage(ox, t); break;
    case 3: seiteSonne(ox, t); break;
    case 4: seiteLuft(ox, t); break;
  }
}

// ------------------------------------------------------------
// Immer sichtbar: Uhrzeit, Akku, Seitenpunkte, Helligkeit, Info
// ------------------------------------------------------------
void statusLeiste() {
  struct tm zeit;
  char uhr[8] = "--:--";
  if (getLocalTime(&zeit, 0)) strftime(uhr, sizeof(uhr), "%H:%M", &zeit);
  bild.setTextFont(2);
  schreibe(bild, uhr, 212, 13, WEISS, RECHTS);

  // Akku-Symbol
  int x = 217, y = 4;
  bild.drawRect(x, y, 17, 9, WEISS);
  bild.fillRect(x + 17, y + 3, 2, 3, WEISS);
  if (akkuVolt > 4.3f) {  // haengt am USB
    bild.fillTriangle(x + 9, y + 1, x + 5, y + 5, x + 9, y + 5, farbe(255, 214, 60));
    bild.fillTriangle(x + 8, y + 4, x + 12, y + 4, x + 8, y + 8, farbe(255, 214, 60));
  } else {
    int prozent = constrain((int)((akkuVolt - 3.3f) / 0.9f * 100), 0, 100);
    uint16_t f = prozent > 50 ? farbe(90, 220, 110) : prozent > 20 ? farbe(255, 210, 60) : farbe(255, 80, 70);
    bild.fillRect(x + 2, y + 2, 13 * prozent / 100, 5, f);
  }
}

bool seiteAn(int s) { return einst.seiten >> s & 1; }

void seitenPunkte() {
  int n = 0;
  for (int i = 0; i < SEITEN_ANZAHL; i++) n += seiteAn(i);
  if (n < 2) return;
  int x = 120 - (n - 1) * 4;
  for (int i = 0; i < SEITEN_ANZAHL; i++) {
    if (!seiteAn(i)) continue;
    bild.fillSmoothCircle(x, 131, 2, i == seite ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(x, 131)));
    x += 8;
  }
}

void lichtAnzeige(uint32_t t) {
  if ((int32_t)(lichtAnzeigeBis - t) <= 0) return;
  bild.fillSmoothRoundRect(70, 104, 100, 20, 10, bild.alphaBlend(200, TFT_BLACK, hintergrund(120, 114)));
  for (int k = 0; k < 4; k++) {
    bool an = k < 4 - einst.licht;
    bild.fillSmoothRoundRect(80 + k * 21, 110, 17, 8, 3, an ? farbe(255, 210, 70) : farbe(70, 74, 84));
  }
}

// Taste IO0 lange halten: wo die Einstellungsseite ist
void infoAnzeige(uint32_t t) {
  if ((int32_t)(infoBis - t) <= 0) return;
  bild.fillSmoothRoundRect(14, 22, 212, 96, 12, bild.alphaBlend(225, TFT_BLACK, hintergrund(120, 70)));
  bild.setTextFont(2);
  schreibe(bild, TX("Einstellungen im Browser:", "Settings in your browser:"), 120, 42, HELL, MITTE);
  bild.setFreeFont(&FreeSansBold9pt7b);
  schreibe(bild, WLAN_HOSTNAME ".local", 120, 64, farbe(255, 198, 44), MITTE);
  bild.setTextFont(2);
  schreibe(bild, (String(TX("oder ", "or ")) + WiFi.localIP().toString()).c_str(), 120, 84, WEISS, MITTE);
  char z[40];
  if (FIRMWARE_BUILD) snprintf(z, sizeof(z), "Build %d", FIRMWARE_BUILD);
  else strcpy(z, TX("eigener Build", "own build"));
  if (updateNeuerBuild()) snprintf(z, sizeof(z), TX("Build %d - neu: %d", "Build %d - new: %d"), FIRMWARE_BUILD, updateNeuerBuild());
  schreibe(bild, z, 120, 106, HELL, MITTE);
}

// ------------------------------------------------------------
// Startbild, solange noch keine Wetterdaten da sind
// ------------------------------------------------------------
void startBild(uint32_t t) {
  zeichneHimmel(bild, HEITER, false, t, false);
  zeichneBild(bild, HEITER, true, 120, 50, 76, t);

  bild.setFreeFont(&FreeSansBold9pt7b);
  schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, 230).c_str(), 120, 100, WEISS, MITTE);

  char zeile1[40], zeile2[40] = "";
  const char *punkte[] = {"", ".", "..", "..."};
  const char *p = punkte[(t / 400) % 4];
  switch (wetterStatus()) {
    case VERBINDE:
      snprintf(zeile1, sizeof(zeile1), TX("Verbinde mit WLAN%s", "Connecting to Wi-Fi%s"), p);
      snprintf(zeile2, sizeof(zeile2), "%s", WiFi.SSID().c_str());
      break;
    case ORTUNG:
      snprintf(zeile1, sizeof(zeile1), TX("Suche Standort%s", "Finding location%s"), p);
      break;
    case LADE:
      snprintf(zeile1, sizeof(zeile1), TX("Hole Wetterdaten%s", "Loading weather%s"), p);
      break;
    default:
      strcpy(zeile1, TX("Abruf fehlgeschlagen", "Download failed"));
      strcpy(zeile2, TX("neuer Versuch in 1 min", "retrying in 1 min"));
      break;
  }
  bild.setTextFont(2);
  schreibe(bild, zeile1, 120, 117, HELL, MITTE);
  schreibe(bild, zeile2, 120, 132, farbe(150, 160, 190), MITTE);
}

// ------------------------------------------------------------
// WLAN-Einrichtung: Anleitung auf dem Display
// ------------------------------------------------------------
void wlanSymbol(int cx, int cy, uint32_t t) {
  // Wellen laufen nacheinander an
  for (int i = 0; i < 3; i++) {
    float an = 0.5f + 0.5f * sinf(t * 0.006f - i * 1.1f);
    uint16_t f = bild.alphaBlend(80 + 175 * an, farbe(255, 198, 44), hintergrund(cx, cy - 10));
    int r = 10 + i * 9;
    bild.drawArc(cx, cy, r + 2, r - 1, 135, 225, f, hintergrund(cx, cy - r));  // 180 = oben
  }
  bild.fillSmoothCircle(cx, cy, 4, farbe(255, 198, 44));
}

// Standort-Nadel, die sanft auf und ab huepft
void ortSymbol(int cx, int cy, uint32_t t) {
  float hub = fabsf(sinf(t * 0.004f)) * 6;
  uint16_t gelb = farbe(255, 198, 44);
  bild.fillSmoothCircle(cx, cy + 20, 9 - hub / 2, bild.alphaBlend(70, TFT_BLACK, hintergrund(cx, cy + 20)));  // Schatten
  int y = cy - hub;
  bild.fillTriangle(cx - 10, y - 2, cx + 10, y - 2, cx, y + 18, gelb);
  bild.fillSmoothCircle(cx, y - 6, 11, gelb);
  bild.fillSmoothCircle(cx, y - 6, 4, hintergrund(cx, cy - 30));
}

void wlanBild(WlanPhase phase, const char *netz) {
  static uint32_t zuletzt = 0;
  uint32_t t = millis();
  if (t - zuletzt < 33 && phase != WLAN_SUCHE) return;  // reicht fuer fluessige Animation
  zuletzt = t;

  zeichneHimmel(bild, KLAR, false, t, false);
  bild.setFreeFont(&FreeSansBold9pt7b);

  if (phase == WLAN_SUCHE) {
    zeichneBild(bild, HEITER, true, 120, 50, 76, t);
    schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, 230).c_str(), 120, 100, WEISS, MITTE);
    bild.setTextFont(2);
    schreibe(bild, TX("Verbinde mit WLAN ...", "Connecting to Wi-Fi ..."), 120, 117, HELL, MITTE);
    schreibe(bild, netz, 120, 132, farbe(150, 160, 190), MITTE);
  } else if (phase == WLAN_EINRICHTUNG) {
    schreibe(bild, TX("WLAN einrichten", "Wi-Fi setup"), 120, 20, WEISS, MITTE);
    wlanSymbol(34, 82, t);
    bild.setTextFont(2);
    schreibe(bild, TX("1. Mit dem Handy ins WLAN", "1. Connect your phone to"), 64, 46, HELL, LINKS);
    schreibe(bild, WLAN_SETUP_NAME, 64, 64, farbe(255, 198, 44), LINKS);
    schreibe(bild, TX("2. Seite öffnet sich selbst:", "2. A page opens by itself:"), 64, 88, HELL, LINKS);
    schreibe(bild, TX("dein WLAN wählen und", "choose your Wi-Fi and"), 64, 104, HELL, LINKS);
    schreibe(bild, TX("Passwort eingeben", "enter the password"), 64, 120, HELL, LINKS);
  } else if (phase == WLAN_TESTE) {
    zeichneBild(bild, HEITER, true, 120, 50, 76, t);
    schreibe(bild, TX("Verbinde ...", "Connecting ..."), 120, 100, WEISS, MITTE);
    bild.setTextFont(2);
    schreibe(bild, netz, 120, 120, farbe(255, 198, 44), MITTE);
  } else if (phase == WLAN_ORT) {
    schreibe(bild, TX("WLAN verbunden!", "Wi-Fi connected!"), 120, 20, farbe(90, 220, 110), MITTE);
    ortSymbol(34, 80, t);
    bild.setTextFont(2);
    schreibe(bild, TX("3. Jetzt am Handy:", "3. Now on your phone:"), 64, 50, HELL, LINKS);
    schreibe(bild, TX("Wo steht die Station?", "Where is the station?"), 64, 68, farbe(255, 198, 44), LINKS);
    schreibe(bild, TX("Ort oder PLZ suchen", "Search town or postcode"), 64, 92, HELL, LINKS);
    schreibe(bild, TX("oder Vorschlag antippen", "or tap the suggestion"), 64, 108, HELL, LINKS);
  } else if (phase == WLAN_OK) {
    zeichneBild(bild, KLAR, true, 120, 50, 76, t);
    schreibe(bild, TX("Fertig!", "Done!"), 120, 100, farbe(90, 220, 110), MITTE);
    bild.setTextFont(2);
    schreibe(bild, *netz ? kuerzen(String(TX("Wetter für ", "Weather for ")) + netz, 230).c_str() : TX("Handy wieder ins eigene WLAN", "Phone back to your Wi-Fi"), 120, 120,
             HELL, MITTE);
  } else {
    zeichneBild(bild, GEWITTER, true, 120, 48, 70, t);
    schreibe(bild, TX("Hat nicht geklappt", "That did not work"), 120, 100, farbe(255, 110, 100), MITTE);
    bild.setTextFont(2);
    schreibe(bild, TX("Passwort falsch? Nochmal versuchen", "Wrong password? Try again"), 120, 120, HELL, MITTE);
  }
  bild.pushSprite(0, 0);
}

// ------------------------------------------------------------
// Updates (ueber WLAN aus VS Code oder selbst aus dem Internet)
// ------------------------------------------------------------
// Waehrend des Updates laeuft loop() nicht weiter - deshalb wird hier selbst gezeichnet.
void updateBild(int prozent, const char *text) {
  uint32_t t = millis();
  zeichneHimmel(bild, KLAR, false, t, false);
  bild.setFreeFont(&FreeSansBold9pt7b);
  schreibe(bild, TX("Neue Version", "New version"), 120, 48, WEISS, MITTE);

  bild.fillSmoothRoundRect(30, 64, 180, 14, 7, farbe(40, 48, 80));
  if (prozent > 0) bild.fillSmoothRoundRect(30, 64, max(14, 180 * prozent / 100), 14, 7, farbe(255, 198, 44));

  char z[16];
  snprintf(z, sizeof(z), "%d %%", prozent);
  bild.setTextFont(2);
  schreibe(bild, z, 120, 100, WEISS, MITTE);
  schreibe(bild, text, 120, 120, HELL, MITTE);
  bild.pushSprite(0, 0);
}

void updateStart() {
  ArduinoOTA.setHostname(WLAN_HOSTNAME);
  if (strlen(OTA_PASSWORT) > 0) ArduinoOTA.setPassword(OTA_PASSWORT);
  ArduinoOTA.onStart([]() {
    Serial.println("Update startet");
    ledcWrite(LICHT_PIN, 255);
    updateBild(0, TX("nicht ausschalten", "do not switch off"));
  });
  ArduinoOTA.onProgress([](unsigned int fertig, unsigned int gesamt) {
    static int zuletzt = -1;
    int p = fertig * 100 / gesamt;
    if (p != zuletzt) {
      zuletzt = p;
      updateBild(p, TX("nicht ausschalten", "do not switch off"));
    }
  });
  ArduinoOTA.onEnd([]() { updateBild(100, TX("fertig - starte neu", "done - restarting")); });
  ArduinoOTA.onError([](ota_error_t fehler) {
    Serial.printf("Update-Fehler %u\n", fehler);
    updateBild(0, TX("Fehler - nochmal versuchen", "Error - please try again"));
    delay(2000);
  });
  ArduinoOTA.begin();
  Serial.printf("Update ueber WLAN bereit: wetterstation.local (%s)\n", WiFi.localIP().toString().c_str());
}

// ------------------------------------------------------------
// Seiten, Licht, Tasten
// ------------------------------------------------------------
void naechsteSeite(uint32_t t) {
  int neu = seite;
  for (int i = 0; i < SEITEN_ANZAHL; i++) {
    neu = (neu + 1) % SEITEN_ANZAHL;
    if (seiteAn(neu)) break;
  }
  if (neu == seite) return;
  alteSeite = seite;
  seite = neu;
  wechsel = true;
  wechselStart = t;
  seiteSeit = t;
}

// Nachtmodus: zwischen "von" und "bis" sehr dunkel. Ein Tastendruck macht es fuer 20 s hell.
bool istNacht() {
  struct tm zeit;
  if (!einst.nacht || einst.nachtVon == einst.nachtBis || !getLocalTime(&zeit, 0)) return false;
  int h = zeit.tm_hour;
  return einst.nachtVon < einst.nachtBis ? h >= einst.nachtVon && h < einst.nachtBis
                                         : h >= einst.nachtVon || h < einst.nachtBis;
}

void lichtSetzen(uint32_t t) {
  static int zuletzt = -1;
  static uint32_t geprueft = 0;
  static bool nacht = false;
  if (t - geprueft > 1000) {
    nacht = istNacht();
    geprueft = t;
  }
  int soll = nacht && t - letzterTastendruck > 20000 ? HELLIGKEIT_NACHT : HELLIGKEIT[einst.licht];
  if (soll != zuletzt) {
    ledcWrite(LICHT_PIN, soll);
    zuletzt = soll;
  }
}

void tasten(uint32_t t, bool hatDaten) {
  // IO0: kurz = naechste Seite (beim Loslassen), 2 s halten = Info
  static uint32_t seiteSeitDruck = 0;
  bool s = digitalRead(TASTER_SEITE) == LOW;
  if (s && !seiteSeitDruck) seiteSeitDruck = t ? t : 1;
  if (s && t - seiteSeitDruck > 2000 && (int32_t)(infoBis - t) <= 0) infoBis = t + 10000;
  if (!s && seiteSeitDruck) {
    if (t - seiteSeitDruck < 1000) {
      if ((int32_t)(infoBis - t) > 0) infoBis = t;  // Info wieder zu
      else if (hatDaten && !wechsel) naechsteSeite(t);
    }
    seiteSeitDruck = 0;
    letzterTastendruck = t;
  }

  // IO35: kurz = Helligkeit (beim Loslassen), 3 s halten = WLAN und Ort neu einrichten
  static uint32_t lichtSeitDruck = 0;
  bool l = digitalRead(TASTER_LICHT) == LOW;
  if (l && !lichtSeitDruck) lichtSeitDruck = t ? t : 1;
  if (l && t - lichtSeitDruck > 3000) {
    wlanBild(WLAN_EINRICHTUNG, "");
    wlanEinrichtungAnfordern();
  }
  if (!l && lichtSeitDruck) {
    bool warDunkel = istNacht() && t - letzterTastendruck > 20000;
    if (t - lichtSeitDruck < 1000 && !warDunkel) {  // nachts weckt der erste Druck nur auf
      einst.licht = (einst.licht + 1) % 4;
      einstellungenSpeichern();
      lichtAnzeigeBis = t + 1500;
    }
    lichtSeitDruck = 0;
    letzterTastendruck = t;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nWetterstation, Build %d\n", FIRMWARE_BUILD);

  pinMode(TASTER_SEITE, INPUT_PULLUP);
  pinMode(TASTER_LICHT, INPUT);  // GPIO 35 hat einen Pull-up auf dem Board

  pinMode(AKKU_EIN, OUTPUT);
  digitalWrite(AKKU_EIN, HIGH);
  analogSetPinAttenuation(AKKU_PIN, ADC_11db);
  akkuVolt = akkuSpannung();

  einstellungenLaden();

  tft.init();
  tft.setRotation(1);  // quer
  tft.fillScreen(TFT_BLACK);

  ledcAttach(LICHT_PIN, 5000, 8);
  ledcWrite(LICHT_PIN, HELLIGKEIT[einst.licht]);

  // Bildspeicher vor dem WLAN holen, solange noch genug am Stueck frei ist
  if (!bild.createSprite(240, 135)) {
    Serial.println("Zu wenig Speicher - nehme 8 Bit Farbe");
    bild.setColorDepth(8);
    bild.createSprite(240, 135);
  }

  // GPIO 35 beim Einschalten gedrueckt halten = WLAN neu einrichten
  bool einrichten = digitalRead(TASTER_LICHT) == LOW;
  ortLaden();
  wlanVerbinden(einrichten, wlanBild);

  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "de.pool.ntp.org", "pool.ntp.org");
  updateStart();
  webStart();
  wetterStart();
  if (!seiteAn(seite)) naechsteSeite(millis());
}

void loop() {
  uint32_t t = millis();

  ArduinoOTA.handle();
  webLoop();

  bool hatDaten = wetterHolen(w);
  tasten(t, hatDaten);
  lichtSetzen(t);

  // Neue Version aus dem Internet: auf Wunsch sofort, sonst wenn gerade niemand die Tasten benutzt
  bool gewuenscht = updateInstallierenGewuenscht();
  if (updateNeuerBuild() && (gewuenscht || (einst.autoUpdate && t - letzterTastendruck > 60000))) {
    ledcWrite(LICHT_PIN, 255);
    updateInstallieren(updateBild);
  }

  if (t - akkuZeit > 2000) {
    akkuVolt = akkuSpannung();
    akkuZeit = t;
  }

  if (!hatDaten) {
    startBild(t);
  } else {
    if (!seiteAn(seite) && !wechsel) naechsteSeite(t);  // Seite wurde gerade abgeschaltet
    Bild b = bildFuerCode(w.code);
    zeichneHimmel(bild, b, w.tag, t, b == GEWITTER && blitzAktiv(t));

    if (wechsel) {
      float p = (t - wechselStart) / float(WECHSEL_DAUER);
      if (p >= 1) {
        wechsel = false;
      } else {
        int weg = (1 - powf(1 - p, 3)) * 240;
        zeichneSeite(alteSeite, -weg, t);
        zeichneSeite(seite, 240 - weg, t);
      }
    }
    if (!wechsel) zeichneSeite(seite, 0, t);

    statusLeiste();
    seitenPunkte();

    if (einst.wechsel && !wechsel && t - seiteSeit > einst.wechsel * 1000UL && t - letzterTastendruck > 30000)
      naechsteSeite(t);
  }
  lichtAnzeige(t);
  infoAnzeige(t);

  bild.pushSprite(0, 0);
}
