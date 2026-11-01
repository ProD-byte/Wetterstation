// LILYGO T-Display-S3 - Wetterstation (ESP32-S3, 1,9" ST7789, 320 x 170, 8-Bit-Parallelbus)
// Standort: am Handy gewaehlt (Einrichtung), sonst grob ueber die Internet-Adresse, sonst Cottbus
//   Taster GPIO 0:  kurz = naechste Seite, 2 s halten = Sprache Deutsch/Englisch (zeigt auch Adresse + Build)
//   Taster GPIO 14: kurz = Helligkeit, 3 s halten = WLAN und Ort neu einrichten
// Seiten: Jetzt, 24 Stunden, naechste Tage, Sonne & Mond, Luft & Pollen, Regenradar, letzte 7 Tage,
//         weitere Orte (wechseln auch von allein)
// Wetterdaten: open-meteo.com, Warnungen: Deutscher Wetterdienst ueber brightsky.dev
// WLAN: Einrichtung per Handy ueber das WLAN "Wetterstation-Setup" (siehe wlan.cpp)
// Einstellungen: http://wetterstation.local (siehe web.cpp)
// Updates: von selbst aus dem Internet (netzupdate.cpp) oder ueber WLAN mit "env:tdisplay_s3_wlan"

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
#include "radar.h"
#include "verlauf.h"
#include "orte.h"

#ifndef OTA_PASSWORT
#define OTA_PASSWORT ""
#endif

// Pins (siehe INFO.md) - das Display selbst steht in Setup206_LilyGo_T_Display_S3.h
const int TASTER_SEITE = 0;   // BOOT-Taste
const int TASTER_LICHT = 14;
const int AKKU_PIN     = 4;   // Akkuspannung ueber Teiler 1:2 (nur ohne USB aussagekraeftig)
const int STROM_EIN    = 15;  // muss HIGH sein, sonst bleibt das Display im Akkubetrieb dunkel
const int LICHT_PIN    = 38;  // Hintergrundlicht: Treiberchip mit 16 Stufen, gesteuert ueber Pulse

const float AKKU_FAKTOR = 2.0f;        // Teiler 1:2 - mit Multimeter nachmessen und anpassen
const uint32_t WECHSEL_DAUER = 380;    // Schiebe-Animation zwischen den Seiten

const int B = 320, H = 170;  // Bildschirm quer

const uint16_t WEISS = TFT_WHITE;
const uint16_t HELL  = 0xDF7E;  // helles Blaugrau

TFT_eSPI tft;
TFT_eSprite bild(&tft);  // ganzer Bildschirm im Speicher (PSRAM) - so flackert nichts

int seite = 0, alteSeite = 0;
bool wechsel = false;
uint32_t wechselStart = 0;
uint32_t seiteSeit = 0;
uint32_t letzterTastendruck = 0;

const int HELLIGKEIT[] = {16, 10, 5, 2};  // Stufen des Lichtchips (16 = hellste)
const int HELLIGKEIT_NACHT = 1;
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

// Hintergrundlicht: der Chip zaehlt Pulse (wie im Beispiel von LILYGO), 0 = aus, 16 = hellste Stufe
void lichtStufe(int wert) {
  static int stufe = 0;
  const int STUFEN = 16;
  wert = constrain(wert, 0, STUFEN);
  if (wert == 0) {
    digitalWrite(LICHT_PIN, LOW);
    delay(3);
    stufe = 0;
    return;
  }
  if (stufe == 0) {
    digitalWrite(LICHT_PIN, HIGH);
    stufe = STUFEN;
    delayMicroseconds(30);
  }
  int von = STUFEN - stufe, bis = STUFEN - wert;
  int pulse = (STUFEN + bis - von) % STUFEN;
  for (int i = 0; i < pulse; i++) {
    digitalWrite(LICHT_PIN, LOW);
    digitalWrite(LICHT_PIN, HIGH);
  }
  stufe = wert;
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
  bild.fillTriangle(x - 4, y - 1, x + 4, y - 1, x, y - 9, f);
  bild.fillSmoothCircle(x, y, 4, f);
}

// Minuten seit Mitternacht aus "06:56"
int minuten(const char *hhmm) { return atoi(hhmm) * 60 + atoi(hhmm + 3); }

// Morgen-/Abendrot: 1 genau bei Sonnenauf- oder -untergang, 0 ab 50 Minuten Abstand
float daemmerung() {
  struct tm zeit;
  if (!getLocalTime(&zeit, 0) || !w.sonneAuf[0] || w.sonneAuf[0] == '-') return 0;
  int jetzt = zeit.tm_hour * 60 + zeit.tm_min;
  int abstand = min(abs(jetzt - minuten(w.sonneAuf)), abs(jetzt - minuten(w.sonneUnter)));
  return abstand < 50 ? 1 - abstand / 50.0f : 0;
}

// Titel oben links. Gibt es eine DWD-Warnung, wechselt er alle 2,5 s mit der Warnung.
void titel(int ox, const char *text, uint32_t t) {
  schrift(bild, S_MF);
  if (w.warnung[0] && (t / 2500) % 2) {
    uint16_t f = w.warnStufe >= 3 ? farbe(255, 80, 70) : farbe(255, 160, 40);
    float blink = 0.6f + 0.4f * sinf(t * 0.012f);
    zeichneWarnzeichen(bild, ox + 15, 11, 15, bild.alphaBlend(255 * blink, f, hintergrund(ox + 15, 11)));
    schreibe(bild, kuerzen(TX(w.warnung, w.warnungEn), 200).c_str(), ox + 28, 18, f, LINKS);
  } else {
    schreibe(bild, kuerzen(text, 210).c_str(), ox + 8, 18, WEISS, LINKS);
  }
}

// ------------------------------------------------------------
// Seite 1: Jetzt
// ------------------------------------------------------------
void seiteJetzt(int ox, uint32_t t) {
  titel(ox, w.ort, t);

  zeichneBild(bild, bildFuerCode(w.code), w.tag, ox + 80, 98, 128, t);

  schrift(bild, S_RIESIG);
  schreibeTemp(bild, w.temp, ox + 164, 82, WEISS, LINKS);

  const char *text = textFuerCode(w.code, w.tag);
  schrift(bild, S_G);
  if (breite(bild, text) > 150) schrift(bild, S_M);
  if (breite(bild, text) > 150) schrift(bild, S_K);
  schreibe(bild, text, ox + 164, 110, WEISS, LINKS);

  schrift(bild, S_K);
  const char *gef = TX("Gefühlt", "Feels like");
  schreibe(bild, gef, ox + 164, 132, HELL, LINKS);
  schreibeTemp(bild, w.gefuehlt, ox + 168 + breite(bild, gef), 132, HELL, LINKS);

  // Regen in den naechsten 2 Stunden hat Vorrang vor Wind und Luftfeuchte
  char z[24];
  if (w.regenArt) {
    snprintf(z, sizeof(z), w.regenArt == 1 ? TX("Regen ab %s", "Rain from %s") : TX("Regen bis %s", "Rain until %s"),
             w.regenZeit);
    tropfenSymbol(ox + 169, 148, farbe(130, 190, 255));
    schreibe(bild, z, ox + 178, 152, farbe(150, 205, 255), LINKS);
    return;
  }
  snprintf(z, sizeof(z), TX("Wind %d km/h", "Wind %d km/h"), (int)lroundf(w.wind));
  schreibe(bild, z, ox + 164, 152, HELL, LINKS);
  snprintf(z, sizeof(z), "%d%%", w.feuchte);
  schreibe(bild, z, ox + 312, 152, HELL, RECHTS);
  tropfenSymbol(ox + 305 - breite(bild, z), 148, farbe(130, 190, 255));
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

  const int X0 = 20, X1 = 300, YO = 56, YU = 116;
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
  for (int x = max(0, (int)px(0)); x <= min(B - 1, (int)xEnde); x++) {
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
    bild.drawWideLine(xa, py(w.stdTemp[i]), xb, yb, 3, farbe(255, 214, 130));
  }

  // Regenwahrscheinlichkeit als Balken
  bool regen = false;
  for (int i = 0; i < n; i++) regen |= w.stdRegen[i] > 0;
  uint16_t blau = farbe(110, 170, 255);
  if (regen) {
    for (int i = 0; i < n; i++) {
      int h = w.stdRegen[i] * 18 / 100;
      if (h <= 0 || px(i) > xEnde) continue;
      bild.fillRect(px(i) - 4, 141 - h, 8, h, bild.alphaBlend(200, blau, hintergrund(px(i), 135)));
    }
  } else {
    schrift(bild, S_K);
    schreibe(bild, TX("Kein Regen erwartet", "No rain expected"), ox + B / 2, 138, HELL, MITTE);
  }

  // Punkte alle 6 Stunden mit Temperatur und Uhrzeit
  float puls = 0.5f + 0.5f * sinf(t * 0.005f);
  for (int i = 0; i < n; i++) {
    bool marke = i == 0 || (i >= 3 && w.stdStunde[i] % 6 == 0);
    if (!marke || px(i) > xEnde) continue;
    float x = px(i), y = py(w.stdTemp[i]);
    if (i == 0) bild.fillSmoothCircle(x, y, 5 + 3 * puls, bild.alphaBlend(90 - 60 * puls, WEISS, hintergrund(x, y)));
    bild.fillSmoothCircle(x, y, 4, WEISS);
    schrift(bild, S_M);
    schreibeTemp(bild, w.stdTemp[i], x, y - 8, WEISS, MITTE);
    schrift(bild, S_K);
    char uhr[8];
    if (i == 0) strcpy(uhr, TX("jetzt", "now"));
    else snprintf(uhr, sizeof(uhr), "%02d", w.stdStunde[i]);
    schreibe(bild, uhr, x, 157, HELL, MITTE);
  }
}

// ------------------------------------------------------------
// Seite 3: heute, morgen, uebermorgen
// ------------------------------------------------------------
void seiteTage(int ox, uint32_t t) {
  titel(ox, TX("Die nächsten Tage", "Next days"), t);

  for (int i = 0; i < 3; i++) {
    int x = ox + 8 + i * 104, mx = x + 48;
    uint16_t karte = bild.alphaBlend(i == 0 ? 95 : 70, TFT_BLACK, hintergrund(mx, 90));
    bild.fillSmoothRoundRect(x, 28, 96, 128, 10, karte);

    const char *name = i == 0 ? TX("Heute", "Today") : i == 1 ? TX("Morgen", "Tomorrow") : TX(WOCHENTAG, WOCHENTAG_EN)[w.tagWochentag[i]];
    schrift(bild, S_MF);
    if (breite(bild, name) > 88) name = TX(WOCHENTAG_KURZ, WOCHENTAG_KURZ_EN)[w.tagWochentag[i]];
    schreibe(bild, name, mx, 49, WEISS, MITTE);

    zeichneBild(bild, bildFuerCode(w.tagCode[i]), true, mx, 82, 58, t + i * 700);

    schrift(bild, S_XG);
    schreibeTemp(bild, w.tagMax[i], mx, 128, WEISS, MITTE);

    schrift(bild, S_K);
    schreibeTemp(bild, w.tagMin[i], x + 22, 148, HELL, MITTE);
    char z[8];
    snprintf(z, sizeof(z), "%d%%", w.tagRegen[i]);
    schreibe(bild, z, x + 90, 148, HELL, RECHTS);
    tropfenSymbol(x + 83 - breite(bild, z), 144, farbe(130, 190, 255));
  }
}

// ------------------------------------------------------------
// Seite 4: Sonne & Mond
// ------------------------------------------------------------
void seiteSonne(int ox, uint32_t t) {
  titel(ox, TX("Sonne & Mond", "Sun & Moon"), t);

  // Sonnenbogen: links Aufgang, rechts Untergang, die Sonne steht dort, wo sie gerade ist
  const int CX = ox + 86, CY = 124, R = 64;
  int auf = minuten(w.sonneAuf), unter = minuten(w.sonneUnter);
  struct tm zeit;
  int jetzt = getLocalTime(&zeit, 0) ? zeit.tm_hour * 60 + zeit.tm_min : -1;
  float frac = unter > auf && jetzt >= 0 ? float(jetzt - auf) / (unter - auf) : -1;

  bild.drawFastHLine(ox + 8, CY, 156, bild.alphaBlend(120, WEISS, hintergrund(CX, CY)));
  for (int i = 0; i <= 80; i++) {  // gestrichelter Bogen
    if (i % 2) continue;
    float a = PI * i / 80;
    bild.drawPixel(CX - R * cosf(a), CY - R * sinf(a), bild.alphaBlend(110, WEISS, hintergrund(CX, CY - R)));
  }
  if (frac >= 0 && frac <= 1) {
    // schon gelaufenes Stueck hell nachzeichnen
    for (float a = 0; a < PI * frac; a += 0.02f) {
      bild.drawWideLine(CX - R * cosf(a), CY - R * sinf(a), CX - R * cosf(a + 0.02f), CY - R * sinf(a + 0.02f), 3,
                        farbe(255, 205, 90));
    }
    zeichneBild(bild, KLAR, true, CX - R * cosf(PI * frac), CY - R * sinf(PI * frac), 46, t);
  } else {
    zeichneBild(bild, KLAR, false, CX, CY - 34, 42, t);  // nachts: Mond ueber dem Horizont
  }

  schrift(bild, S_K);
  schreibe(bild, w.sonneAuf, ox + 8, 142, WEISS, LINKS);
  schreibe(bild, w.sonneUnter, ox + 164, 142, WEISS, RECHTS);
  int dauer = unter - auf;
  if (dauer > 0) {
    char z[24];
    snprintf(z, sizeof(z), TX("Tag %d h %02d min", "Day %d h %02d min"), dauer / 60, dauer % 60);
    schreibe(bild, z, ox + 8, 160, HELL, LINKS);
  }

  // Mondphase rechts
  time_t sek;
  time(&sek);
  float p = mondphase(sek);
  zeichneMondphase(bild, ox + 246, 64, 30, p);
  schrift(bild, S_M);
  schreibe(bild, mondName(p), ox + 246, 118, WEISS, MITTE);
  schrift(bild, S_K);
  char z[32];
  snprintf(z, sizeof(z), TX("%d %% beleuchtet", "%d %% lit"), (int)lroundf((1 - cosf(p * 2 * PI)) / 2 * 100));
  schreibe(bild, z, ox + 246, 138, HELL, MITTE);
  snprintf(z, sizeof(z), TX("UV-Index %d", "UV index %d"), (int)lroundf(w.uv));
  schreibe(bild, z, ox + 246, 156, w.uv >= 6 ? farbe(255, 160, 40) : HELL, MITTE);
}

// ------------------------------------------------------------
// Seite 5: Luft & Pollen
// ------------------------------------------------------------
void seiteLuft(int ox, uint32_t t) {
  titel(ox, TX("Luft & Pollen", "Air & Pollen"), t);
  schrift(bild, S_M);
  if (!w.luftGueltig) {
    schreibe(bild, TX("Für diesen Ort gibt es", "No air quality data"), ox + B / 2, 80, HELL, MITTE);
    schreibe(bild, TX("keine Luftdaten", "for this place"), ox + B / 2, 104, HELL, MITTE);
    return;
  }

  // Europaeischer Luftqualitaetsindex als Ring
  static const char *STUFE_DE[] = {"Sehr gut", "Gut", "Mäßig", "Schlecht", "Sehr schlecht", "Extrem"};
  static const char *STUFE_EN[] = {"Very good", "Good", "Moderate", "Poor", "Very poor", "Extreme"};
  const char **STUFE = TX(STUFE_DE, STUFE_EN);
  static const uint16_t FARBE[] = {0x4F6A, 0x8F68, 0xFE84, 0xFC64, 0xF9E8, 0xA01F};
  int stufe = constrain(w.aqi / 20, 0, 5);
  uint16_t f = FARBE[stufe];
  const int CX = ox + 76, CY = 84, R = 50;
  float fort = min(1.0f, (t - seiteSeit) / 900.0f);  // Ring fuellt sich beim Umblaettern
  uint16_t hg = hintergrund(CX, CY - R);
  bild.drawSmoothArc(CX, CY, R, R - 9, 30, 330, bild.alphaBlend(60, WEISS, hg), hg, true);
  int ende = 30 + 300 * min(1.0f, w.aqi / 100.0f) * (1 - powf(1 - fort, 3));
  if (ende > 31) bild.drawSmoothArc(CX, CY, R, R - 9, 30, ende, f, hg, true);
  schrift(bild, S_XG);
  char z[24];
  snprintf(z, sizeof(z), "%d", w.aqi);
  schreibe(bild, z, CX, CY + 12, WEISS, MITTE);
  schrift(bild, S_K);
  schreibe(bild, STUFE[stufe], CX, 148, f, MITTE);
  snprintf(z, sizeof(z), TX("Feinstaub %d", "PM2.5: %d"), (int)lroundf(w.pm25));
  schreibe(bild, z, CX, 165, HELL, MITTE);

  // Pollen: drei Stufen je Art
  schrift(bild, S_M);
  for (int i = 0; i < 5; i++) {
    int y = 50 + i * 24;
    float k = w.pollen[i];
    int lvl = k < 1 ? 0 : k <= 10 ? 1 : k <= 50 ? 2 : 3;
    schreibe(bild, pollenName(i), ox + 158, y, lvl ? WEISS : HELL, LINKS);
    static const uint16_t PF[] = {0, 0x8F68, 0xFE84, 0xF9E8};
    for (int s = 0; s < 3; s++) {
      uint16_t c = s < lvl ? PF[lvl] : bild.alphaBlend(50, WEISS, hintergrund(ox + 250, y - 6));
      bild.fillSmoothRoundRect(ox + 238 + s * 26, y - 11, 22, 9, 4, c);
    }
  }
}

// ------------------------------------------------------------
// Seite 6: Regenradar (Karte + Radarbilder der letzten Stunde, animiert)
// ------------------------------------------------------------
void seiteRadar(int ox, uint32_t t) {
  titel(ox, TX("Regenradar", "Rain radar"), t);
  RadarStand r;
  if (!radarHolen(r) || (!r.karte && !r.bilder)) {
    schrift(bild, S_M);
    schreibe(bild, TX("Radar wird geladen …", "Loading radar …"), ox + B / 2, 100, HELL, MITTE);
    return;
  }
  const int Y0 = H - RADAR_H;
  // Bilder nacheinander, das neueste bleibt etwas laenger stehen
  int nr = r.bilder ? min((int)((t / 500) % (r.bilder + 4)), r.bilder - 1) : -1;
  const uint8_t *radar = nr >= 0 ? r.bild[nr] : nullptr;
  int xa = max(0, -ox), xe = min(RADAR_B, B - ox);  // nur der sichtbare Teil (beim Umblaettern)
  for (int y = 0; y < RADAR_H; y++) {
    const uint16_t *kz = r.kartePixel + y * RADAR_B;
    const uint8_t *rz = radar ? radar + y * RADAR_B * 3 : nullptr;
    for (int x = xa; x < xe; x++) {
      uint16_t c = r.karte ? kz[x] : farbe(20, 24, 32);
      if (rz && rz[x * 3 + 2] > 8) c = bild.alphaBlend(rz[x * 3 + 2], rz[x * 3] | (rz[x * 3 + 1] << 8), c);
      bild.drawPixel(ox + x, Y0 + y, c);
    }
  }

  // eigener Ort in der Mitte, pulsierend
  int mx = ox + RADAR_B / 2, my = Y0 + RADAR_H / 2;
  float puls = fmodf(t / 1400.0f, 1.0f);
  bild.drawSmoothCircle(mx, my, 4 + puls * 14, bild.alphaBlend(255 * (1 - puls), farbe(255, 198, 44), hintergrund(mx, my - 10)),
                        hintergrund(mx, my - 10));
  bild.fillSmoothCircle(mx, my, 5, TFT_BLACK);
  bild.fillSmoothCircle(mx, my, 3, farbe(255, 198, 44));

  // trocken im ganzen Ausschnitt: deutlich sagen, statt nur eine leere Karte zu zeigen
  if (r.bilder && !r.regen) {
    const char *txt = TX("Kein Regen in der Nähe", "No rain nearby");
    schrift(bild, S_M);
    int w = breite(bild, txt) + 24;
    bild.fillSmoothRoundRect(mx - w / 2, my + 16, w, 26, 13, farbe(16, 22, 34), hintergrund(mx, my + 16));
    schreibe(bild, txt, mx, my + 35, WEISS, MITTE);
  }

  // Uhrzeit des gezeigten Bilds und Quellen
  if (nr >= 0) {
    struct tm z;
    localtime_r(&r.zeit[nr], &z);
    char uhr[8];
    strftime(uhr, sizeof(uhr), "%H:%M", &z);
    schrift(bild, S_MF);
    schreibe(bild, uhr, ox + 8, H - 8, nr == r.bilder - 1 ? farbe(255, 198, 44) : WEISS, LINKS);
    // kleine Zeitleiste: ein Strich je Bild
    for (int i = 0; i < r.bilder; i++)
      bild.fillRect(ox + 62 + i * 7, H - 14, 5, 4, i == nr ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(ox + 62 + i * 7, H - 14)));
  } else {
    schrift(bild, S_K);
    schreibe(bild, TX("Noch keine Radarbilder", "No radar images yet"), ox + 8, H - 8, HELL, LINKS);
  }
  schrift(bild, S_K);
  schreibe(bild, "© OSM · RainViewer", ox + B - 6, H - 8, HELL, RECHTS);
}

// ------------------------------------------------------------
// Seite 7: die letzten 7 Tage (Temperatur, Luftdruck, Regen)
// ------------------------------------------------------------
void seiteVerlauf(int ox, uint32_t t) {
  titel(ox, TX("Letzte 7 Tage", "Last 7 days"), t);
  static Verlauf v;  // 2 KB - nicht auf den Stack
  if (!verlaufHolen(v)) {
    schrift(bild, S_M);
    schreibe(bild, TX("Verlauf wird geladen …", "Loading history …"), ox + B / 2, 100, HELL, MITTE);
    return;
  }
  const int X0 = 36, X1 = 308, YO = 36, YU = 116, RU = 140, RH = 18;
  int n = v.n;
  float tmin = 999, tmax = -999, dmin = 9999, dmax = 0;
  for (int i = 0; i < n; i++) {
    if (!isnan(v.temp[i])) tmin = min(tmin, v.temp[i]), tmax = max(tmax, v.temp[i]);
    if (!isnan(v.druck[i])) dmin = min(dmin, v.druck[i]), dmax = max(dmax, v.druck[i]);
  }
  if (tmax - tmin < 4) tmin = (tmax + tmin) / 2 - 2, tmax = tmin + 4;
  if (dmax - dmin < 10) dmin = (dmax + dmin) / 2 - 5, dmax = dmin + 10;
  auto px = [&](float i) { return ox + X0 + (X1 - X0) * i / (n - 1); };
  auto pyT = [&](float v_) { return YU - (v_ - tmin) / (tmax - tmin) * (YU - YO); };
  auto pyD = [&](float v_) { return YU - (v_ - dmin) / (dmax - dmin) * (YU - YO); };

  float fort = min(1.0f, (t - seiteSeit) / 1200.0f);
  fort = 1 - powf(1 - fort, 3);
  int bis = 1 + (n - 1) * fort;

  // Tagesgrenzen (Mitternacht) und Wochentage
  static const char *WT_DE[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  static const char *WT_EN[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  schrift(bild, S_K);
  for (int d = 0; d <= 8; d++) {
    int a = max(0, d * 24 - v.ersteStunde), e = min(n - 1, (d + 1) * 24 - v.ersteStunde - 1);
    if (a >= n) break;
    if (a > 0) {
      for (int y = YO; y < RU; y += 3) bild.drawPixel(px(a), y, bild.alphaBlend(90, WEISS, hintergrund(px(a), y)));
    }
    if (e - a >= 8) schreibe(bild, TX(WT_DE, WT_EN)[(v.ersterWochentag + d) % 7], (px(a) + px(e)) / 2, 156, HELL, MITTE);
  }

  // Temperatur als Flaeche mit Linie
  uint16_t orange = farbe(255, 168, 64);
  for (int i = 1; i < bis; i++) {
    if (isnan(v.temp[i - 1]) || isnan(v.temp[i])) continue;
    for (int x = px(i - 1); x < px(i); x++) {
      float f = (x - px(i - 1)) / (px(i) - px(i - 1));
      int y = pyT(v.temp[i - 1] * (1 - f) + v.temp[i] * f);
      for (int yy = y; yy <= YU; yy += 2) bild.drawPixel(x, yy, bild.alphaBlend(60, orange, bild.readPixel(x, yy)));
    }
    bild.drawWideLine(px(i - 1), pyT(v.temp[i - 1]), px(i), pyT(v.temp[i]), 2, farbe(255, 214, 130));
  }
  // Luftdruck als duenne Linie
  for (int i = 1; i < bis; i++) {
    if (isnan(v.druck[i - 1]) || isnan(v.druck[i])) continue;
    bild.drawWideLine(px(i - 1), pyD(v.druck[i - 1]), px(i), pyD(v.druck[i]), 1.5f, farbe(140, 200, 255));
  }
  // Regen je Stunde als Balken unten
  for (int i = 0; i < bis; i++) {
    int h = min((float)RH, v.regen[i] * 8);
    if (h > 0) bild.drawFastVLine(px(i), RU - h, h, farbe(110, 170, 255));
  }

  // Beschriftung: Temperatur links, Luftdruck rechts oben
  schrift(bild, S_K);
  schreibeTemp(bild, tmax, ox + 4, YO + 8, farbe(255, 214, 130), LINKS);
  schreibeTemp(bild, tmin, ox + 4, YU, farbe(255, 214, 130), LINKS);
  char z[16];
  snprintf(z, sizeof(z), "%d hPa", (int)lroundf(v.druck[n - 1]));
  schreibe(bild, z, ox + X1, YO - 4, farbe(140, 200, 255), RECHTS);
}

// ------------------------------------------------------------
// Seite 8: weitere Orte im Vergleich (Hauptort zuerst)
// ------------------------------------------------------------
void seiteOrte(int ox, uint32_t t) {
  titel(ox, TX("Orte im Vergleich", "Places"), t);
  OrtWetter liste[ORTE_MAX + 1];
  strlcpy(liste[0].name, w.ort, sizeof(liste[0].name));
  liste[0].gueltig = true;
  liste[0].temp = w.temp, liste[0].max = w.tagMax[0], liste[0].min = w.tagMin[0];
  liste[0].code = w.code, liste[0].tag = w.tag;
  int n = 1 + orteHolen(liste + 1);
  int zh = min(36, 136 / n), y0 = 26;
  for (int i = 0; i < n; i++) {
    int y = y0 + i * zh, mitte = y + (zh - 4) / 2;
    bild.fillSmoothRoundRect(ox + 8, y, 304, zh - 4, 8, bild.alphaBlend(i ? 70 : 100, TFT_BLACK, hintergrund(ox + 160, mitte)));
    const OrtWetter &o = liste[i];
    if (o.gueltig) zeichneBild(bild, bildFuerCode(o.code), o.tag, ox + 30, mitte, min(34, zh), t + i * 500);
    schrift(bild, S_MF);
    schreibe(bild, kuerzen(o.name, 140).c_str(), ox + 56, mitte + 6, WEISS, LINKS);
    if (!o.gueltig) {
      schrift(bild, S_K);
      schreibe(bild, "…", ox + 300, mitte + 5, HELL, RECHTS);
      continue;
    }
    schrift(bild, S_GF);
    schreibeTemp(bild, o.temp, ox + 246, mitte + 8, WEISS, RECHTS);
    schrift(bild, S_K);
    char z[24];
    snprintf(z, sizeof(z), "%d° / %d°", (int)lroundf(o.max), (int)lroundf(o.min));
    schreibe(bild, z, ox + 306, mitte + 5, HELL, RECHTS);
  }
}

void zeichneSeite(int s, int ox, uint32_t t) {
  if (ox <= -B || ox >= B) return;
  switch (s) {
    case 0: seiteJetzt(ox, t); break;
    case 1: seiteStunden(ox, t); break;
    case 2: seiteTage(ox, t); break;
    case 3: seiteSonne(ox, t); break;
    case 4: seiteLuft(ox, t); break;
    case 5: seiteRadar(ox, t); break;
    case 6: seiteVerlauf(ox, t); break;
    case 7: seiteOrte(ox, t); break;
  }
}

// ------------------------------------------------------------
// Immer sichtbar: Uhrzeit, Akku, Seitenpunkte, Helligkeit, Info
// ------------------------------------------------------------
void statusLeiste() {
  struct tm zeit;
  char uhr[8] = "--:--";
  if (getLocalTime(&zeit, 0)) strftime(uhr, sizeof(uhr), "%H:%M", &zeit);
  schrift(bild, S_MF);
  schreibe(bild, uhr, 283, 18, WEISS, RECHTS);

  // Akku-Symbol
  int x = 289, y = 6;
  bild.drawRoundRect(x, y, 22, 12, 2, WEISS);
  bild.fillRect(x + 22, y + 4, 2, 4, WEISS);
  if (akkuVolt > 4.3f) {  // haengt am USB
    uint16_t g = farbe(255, 214, 60);
    bild.fillTriangle(x + 12, y + 1, x + 6, y + 7, x + 12, y + 7, g);
    bild.fillTriangle(x + 10, y + 5, x + 16, y + 5, x + 10, y + 11, g);
  } else {
    int prozent = constrain((int)((akkuVolt - 3.3f) / 0.9f * 100), 0, 100);
    uint16_t f = prozent > 50 ? farbe(90, 220, 110) : prozent > 20 ? farbe(255, 210, 60) : farbe(255, 80, 70);
    bild.fillRect(x + 2, y + 2, 18 * prozent / 100, 8, f);
  }
}

// Seite "Orte" nur, wenn weitere Orte eingetragen sind
bool seiteAn(int s) { return (einst.seiten >> s & 1) && (s != 7 || orteAnzahl() > 0); }

void seitenPunkte() {
  int n = 0;
  for (int i = 0; i < SEITEN_ANZAHL; i++) n += seiteAn(i);
  if (n < 2) return;
  int x = B / 2 - (n - 1) * 5;
  for (int i = 0; i < SEITEN_ANZAHL; i++) {
    if (!seiteAn(i)) continue;
    bild.fillSmoothCircle(x, 165, 2, i == seite ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(x, 165)));
    x += 10;
  }
}

void lichtAnzeige(uint32_t t) {
  if ((int32_t)(lichtAnzeigeBis - t) <= 0) return;
  bild.fillSmoothRoundRect(98, 126, 124, 26, 13, bild.alphaBlend(200, TFT_BLACK, hintergrund(160, 139)));
  for (int k = 0; k < 4; k++) {
    bool an = k < 4 - einst.licht;
    bild.fillSmoothRoundRect(110 + k * 26, 134, 22, 10, 4, an ? farbe(255, 210, 70) : farbe(70, 74, 84));
  }
}

// Taste IO0 lange halten: neue Sprache, dazu wo die Einstellungsseite ist
void infoAnzeige(uint32_t t) {
  if ((int32_t)(infoBis - t) <= 0) return;
  bild.fillSmoothRoundRect(30, 26, 260, 124, 14, bild.alphaBlend(225, TFT_BLACK, hintergrund(160, 88)));
  // oben die (gerade gewaehlte) Sprache, darunter wo die Einstellungsseite ist
  schrift(bild, S_GF);
  schreibe(bild, TX("Deutsch", "English"), B / 2, 54, farbe(255, 198, 44), MITTE);
  schrift(bild, S_K);
  schreibe(bild, TX("Einstellungen im Browser:", "Settings in your browser:"), B / 2, 76, HELL, MITTE);
  schrift(bild, S_M);
  schreibe(bild, WLAN_HOSTNAME ".local", B / 2, 98, WEISS, MITTE);
  schrift(bild, S_K);
  schreibe(bild, (String(TX("oder ", "or ")) + WiFi.localIP().toString()).c_str(), B / 2, 118, WEISS, MITTE);
  char z[40];
  if (FIRMWARE_BUILD) snprintf(z, sizeof(z), "Build %d", FIRMWARE_BUILD);
  else strcpy(z, TX("eigener Build", "own build"));
  if (updateNeuerBuild()) snprintf(z, sizeof(z), TX("Build %d - neu: %d", "Build %d - new: %d"), FIRMWARE_BUILD, updateNeuerBuild());
  schreibe(bild, z, B / 2, 138, HELL, MITTE);
}

// ------------------------------------------------------------
// Startbild, solange noch keine Wetterdaten da sind
// ------------------------------------------------------------
void startBild(uint32_t t) {
  zeichneHimmel(bild, HEITER, false, t, false);
  zeichneBild(bild, HEITER, true, B / 2, 62, 100, t);

  schrift(bild, S_GF);
  schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, 300).c_str(), B / 2, 128, WEISS, MITTE);

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
  schrift(bild, S_K);
  schreibe(bild, zeile1, B / 2, 149, HELL, MITTE);
  schreibe(bild, zeile2, B / 2, 165, farbe(150, 160, 190), MITTE);
}

// ------------------------------------------------------------
// WLAN-Einrichtung: Anleitung auf dem Display
// ------------------------------------------------------------
void wlanSymbol(int cx, int cy, uint32_t t) {
  // Wellen laufen nacheinander an
  for (int i = 0; i < 3; i++) {
    float an = 0.5f + 0.5f * sinf(t * 0.006f - i * 1.1f);
    uint16_t f = bild.alphaBlend(80 + 175 * an, farbe(255, 198, 44), hintergrund(cx, cy - 10));
    int r = 13 + i * 12;
    bild.drawArc(cx, cy, r + 3, r - 1, 135, 225, f, hintergrund(cx, cy - r));  // 180 = oben
  }
  bild.fillSmoothCircle(cx, cy, 5, farbe(255, 198, 44));
}

// Standort-Nadel, die sanft auf und ab huepft
void ortSymbol(int cx, int cy, uint32_t t) {
  float hub = fabsf(sinf(t * 0.004f)) * 8;
  uint16_t gelb = farbe(255, 198, 44);
  bild.fillSmoothCircle(cx, cy + 26, 12 - hub / 2, bild.alphaBlend(70, TFT_BLACK, hintergrund(cx, cy + 26)));  // Schatten
  int y = cy - hub;
  bild.fillTriangle(cx - 13, y - 2, cx + 13, y - 2, cx, y + 24, gelb);
  bild.fillSmoothCircle(cx, y - 8, 14, gelb);
  bild.fillSmoothCircle(cx, y - 8, 5, hintergrund(cx, cy - 40));
}

void wlanBild(WlanPhase phase, const char *netz) {
  static uint32_t zuletzt = 0;
  uint32_t t = millis();
  if (t - zuletzt < 33 && phase != WLAN_SUCHE) return;  // reicht fuer fluessige Animation
  zuletzt = t;

  zeichneHimmel(bild, KLAR, false, t, false);
  schrift(bild, S_GF);

  if (phase == WLAN_SUCHE) {
    zeichneBild(bild, HEITER, true, B / 2, 62, 100, t);
    schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, 300).c_str(), B / 2, 128, WEISS, MITTE);
    schrift(bild, S_K);
    schreibe(bild, TX("Verbinde mit WLAN ...", "Connecting to Wi-Fi ..."), B / 2, 149, HELL, MITTE);
    schreibe(bild, netz, B / 2, 165, farbe(150, 160, 190), MITTE);
  } else if (phase == WLAN_EINRICHTUNG) {
    schreibe(bild, TX("WLAN einrichten", "Wi-Fi setup"), B / 2, 27, WEISS, MITTE);
    wlanSymbol(46, 110, t);
    schrift(bild, S_M);
    schreibe(bild, TX("1. Mit dem Handy ins WLAN", "1. Connect your phone to"), 94, 60, HELL, LINKS);
    schrift(bild, S_MF);
    schreibe(bild, WLAN_SETUP_NAME, 94, 82, farbe(255, 198, 44), LINKS);
    schrift(bild, S_M);
    schreibe(bild, TX("2. Seite öffnet sich selbst:", "2. A page opens by itself:"), 94, 112, HELL, LINKS);
    schreibe(bild, TX("dein WLAN wählen und", "choose your Wi-Fi and"), 94, 134, HELL, LINKS);
    schreibe(bild, TX("Passwort eingeben", "enter the password"), 94, 156, HELL, LINKS);
  } else if (phase == WLAN_TESTE) {
    zeichneBild(bild, HEITER, true, B / 2, 62, 100, t);
    schreibe(bild, TX("Verbinde ...", "Connecting ..."), B / 2, 128, WEISS, MITTE);
    schrift(bild, S_K);
    schreibe(bild, netz, B / 2, 152, farbe(255, 198, 44), MITTE);
  } else if (phase == WLAN_ORT) {
    schreibe(bild, TX("WLAN verbunden!", "Wi-Fi connected!"), B / 2, 27, farbe(90, 220, 110), MITTE);
    ortSymbol(46, 100, t);
    schrift(bild, S_M);
    schreibe(bild, TX("3. Jetzt am Handy:", "3. Now on your phone:"), 94, 62, HELL, LINKS);
    schrift(bild, S_MF);
    schreibe(bild, TX("Wo steht die Station?", "Where is the station?"), 94, 86, farbe(255, 198, 44), LINKS);
    schrift(bild, S_M);
    schreibe(bild, TX("Ort oder PLZ suchen", "Search town or postcode"), 94, 118, HELL, LINKS);
    schreibe(bild, TX("oder Vorschlag antippen", "or tap the suggestion"), 94, 140, HELL, LINKS);
  } else if (phase == WLAN_OK) {
    zeichneBild(bild, KLAR, true, B / 2, 62, 100, t);
    schreibe(bild, TX("Fertig!", "Done!"), B / 2, 128, farbe(90, 220, 110), MITTE);
    schrift(bild, S_K);
    schreibe(bild, *netz ? kuerzen(String(TX("Wetter für ", "Weather for ")) + netz, 300).c_str() : TX("Handy wieder ins eigene WLAN", "Phone back to your Wi-Fi"), B / 2, 152,
             HELL, MITTE);
  } else {
    zeichneBild(bild, GEWITTER, true, B / 2, 58, 92, t);
    schreibe(bild, TX("Hat nicht geklappt", "That did not work"), B / 2, 128, farbe(255, 110, 100), MITTE);
    schrift(bild, S_K);
    schreibe(bild, TX("Passwort falsch? Nochmal versuchen", "Wrong password? Try again"), B / 2, 152, HELL, MITTE);
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
  schrift(bild, S_GF);
  schreibe(bild, TX("Neue Version", "New version"), B / 2, 56, WEISS, MITTE);

  bild.fillSmoothRoundRect(40, 74, 240, 16, 8, farbe(40, 48, 80));
  if (prozent > 0) bild.fillSmoothRoundRect(40, 74, max(16, 240 * prozent / 100), 16, 8, farbe(255, 198, 44));

  char z[16];
  snprintf(z, sizeof(z), "%d %%", prozent);
  schrift(bild, S_M);
  schreibe(bild, z, B / 2, 120, WEISS, MITTE);
  schrift(bild, S_K);
  schreibe(bild, text, B / 2, 146, HELL, MITTE);
  bild.pushSprite(0, 0);
}

void updateStart() {
  ArduinoOTA.setHostname(WLAN_HOSTNAME);
  if (strlen(OTA_PASSWORT) > 0) ArduinoOTA.setPassword(OTA_PASSWORT);
  ArduinoOTA.onStart([]() {
    Serial.println("Update startet");
    lichtStufe(16);
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
    lichtStufe(soll);
    zuletzt = soll;
  }
}

void tasten(uint32_t t, bool hatDaten) {
  // IO0: kurz = naechste Seite (beim Loslassen), 2 s halten = Sprache Deutsch/Englisch wechseln
  // (zeigt die neue Sprache zusammen mit Adresse der Einstellungsseite und Build)
  static uint32_t seiteSeitDruck = 0;
  static bool sprachGewechselt = false;
  bool s = digitalRead(TASTER_SEITE) == LOW;
  if (s && !seiteSeitDruck) seiteSeitDruck = t ? t : 1;
  if (s && t - seiteSeitDruck > 2000 && !sprachGewechselt) {
    einst.sprache ^= 1;
    einstellungenSpeichern();
    infoBis = t + 6000;
    sprachGewechselt = true;
  }
  if (!s && seiteSeitDruck) {
    sprachGewechselt = false;
    if (t - seiteSeitDruck < 1000) {
      if ((int32_t)(infoBis - t) > 0) infoBis = t;  // Info wieder zu
      else if (hatDaten && !wechsel) naechsteSeite(t);
    }
    seiteSeitDruck = 0;
    letzterTastendruck = t;
  }

  // IO14: kurz = Helligkeit (beim Loslassen), 3 s halten = WLAN und Ort neu einrichten
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
  // Zuerst Strom fuers Display einschalten (im Akkubetrieb sonst dunkel)
  pinMode(STROM_EIN, OUTPUT);
  digitalWrite(STROM_EIN, HIGH);

  Serial.begin(115200);
  Serial.printf("\nWetterstation T-Display-S3, Build %d\n", FIRMWARE_BUILD);

  pinMode(TASTER_SEITE, INPUT_PULLUP);
  pinMode(TASTER_LICHT, INPUT_PULLUP);
  analogSetPinAttenuation(AKKU_PIN, ADC_11db);
  akkuVolt = akkuSpannung();

  einstellungenLaden();

  tft.init();
  tft.setRotation(einst.gedreht ? 3 : 1);  // quer, 3 = um 180 Grad gedreht (Einstellungsseite)
  tft.fillScreen(TFT_BLACK);

  pinMode(LICHT_PIN, OUTPUT);
  lichtStufe(HELLIGKEIT[einst.licht]);

  // Bildspeicher fuer den ganzen Bildschirm (liegt im PSRAM)
  if (!bild.createSprite(B, H)) {
    Serial.println("Zu wenig Speicher - nehme 8 Bit Farbe");
    bild.setColorDepth(8);
    bild.createSprite(B, H);
  }

  // GPIO 14 beim Einschalten gedrueckt halten = WLAN neu einrichten
  bool einrichten = digitalRead(TASTER_LICHT) == LOW;
  ortLaden();
  orteLaden();
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

  // Bild drehen wurde auf der Einstellungsseite umgeschaltet -> sofort uebernehmen
  static bool gedreht = einst.gedreht;
  if (gedreht != einst.gedreht) {
    gedreht = einst.gedreht;
    tft.setRotation(gedreht ? 3 : 1);
  }

  // Neue Version aus dem Internet: auf Wunsch sofort, sonst wenn gerade niemand die Tasten benutzt
  bool gewuenscht = updateInstallierenGewuenscht();
  if (updateNeuerBuild() && (gewuenscht || (einst.autoUpdate && t - letzterTastendruck > 60000))) {
    lichtStufe(16);
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
    zeichneHimmel(bild, b, w.tag, t, b == GEWITTER && blitzAktiv(t), daemmerung());

    if (wechsel) {
      float p = (t - wechselStart) / float(WECHSEL_DAUER);
      if (p >= 1) {
        wechsel = false;
      } else {
        int weg = (1 - powf(1 - p, 3)) * B;
        zeichneSeite(alteSeite, -weg, t);
        zeichneSeite(seite, B - weg, t);
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
