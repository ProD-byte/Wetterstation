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
#include "ersatz.h"
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

// Bildschirm: quer 320 x 170, hochkant 170 x 320 (Einstellung "Ausrichtung", siehe lageSetzen)
int B = 320, H = 170;
inline bool hoch() { return B < H; }
// hochkant: oben Uhr + Akku, darunter der Titel, Inhalt ab TOP
const int HOCH_TITEL_Y = 44, HOCH_TOP = 54;

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
void zeitzoneSetzen(int32_t abweichung);

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

// Farbe hinter (x, y) fuer Kaesten, Balken und Kanten - unempfindlich gegen Regen/Schnee (siehe grundFarbe)
uint16_t hintergrund(int x, int y) { return grundFarbe(bild, x, y); }

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

// Ortszeit, false solange die Uhr noch nicht gestellt ist. Ersetzt getLocalTime(&z, 0): das meldet
// manchmal ohne Grund "keine Zeit", wenn waehrend des Aufrufs die Millisekunde wechselt - dann
// stand fuer ein Bild "--:--" da und Abendrot/Sonne/Mond sprangen (das Display zuckte)
bool ortszeit(struct tm &z) {
  time_t jetzt = time(nullptr);
  if (jetzt < 1700000000) return false;  // vor Nov. 2023: noch nicht per NTP gestellt
  localtime_r(&jetzt, &z);
  return true;
}

// Minuten seit Mitternacht aus "06:56"
int minuten(const char *hhmm) { return atoi(hhmm) * 60 + atoi(hhmm + 3); }

// Morgen-/Abendrot: 1 genau bei Sonnenauf- oder -untergang, 0 ab 50 Minuten Abstand
float daemmerung() {
  struct tm zeit;
  if (!ortszeit(zeit) || !w.sonneAuf[0] || w.sonneAuf[0] == '-') return 0;
  int jetzt = zeit.tm_hour * 60 + zeit.tm_min;
  int abstand = min(abs(jetzt - minuten(w.sonneAuf)), abs(jetzt - minuten(w.sonneUnter)));
  return abstand < 50 ? 1 - abstand / 50.0f : 0;
}

// Titel oben links. Gibt es eine DWD-Warnung, wechselt er alle 2,5 s mit der Warnung.
// Hochkant steht der Titel in einer eigenen Zeile unter Uhr und Akku.
void titel(int ox, const char *text, uint32_t t) {
  schrift(bild, S_MF);
  int y = hoch() ? HOCH_TITEL_Y : 18;
  int platz = hoch() ? B - 16 : 210;
  if (w.warnung[0] && w.warnStufe >= einst.warnAb && (t / 2500) % 2) {
    uint16_t f = w.warnStufe >= 3 ? farbe(255, 80, 70) : farbe(255, 160, 40);
    float blink = 0.6f + 0.4f * sinf(t * 0.012f);
    zeichneWarnzeichen(bild, ox + 15, y - 7, 15, bild.alphaBlend(255 * blink, f, hintergrund(ox + 15, y - 7)));
    schreibe(bild, kuerzen(TX(w.warnung, w.warnungEn), platz - 20).c_str(), ox + 28, y, f, LINKS);
  } else {
    schreibe(bild, kuerzen(text, platz).c_str(), ox + 8, y, WEISS, LINKS);
  }
}

// ------------------------------------------------------------
// Seite 1: Jetzt
// ------------------------------------------------------------
// hochkant: Bild oben, darunter gross die Temperatur, dann Text, gefuehlt und Wind/Regen
void seiteJetztHoch(int ox, uint32_t t) {
  int mx = ox + B / 2;
  zeichneBild(bild, bildFuerCode(w.code), w.tag, mx, 112, 104, t);
  schrift(bild, S_RIESIG);
  schreibeTemp(bild, w.temp, mx + 6, 222, WEISS, MITTE);

  const char *text = textFuerCode(w.code, w.tag);
  schrift(bild, S_G);
  if (breite(bild, text) > B - 12) schrift(bild, S_M);
  if (breite(bild, text) > B - 12) schrift(bild, S_K);
  schreibe(bild, text, mx, 250, WEISS, MITTE);

  schrift(bild, S_K);
  char z[24];
  snprintf(z, sizeof(z), "%s %d°", TX("Gefühlt", "Feels like"), (int)lroundf(w.gefuehlt));
  schreibe(bild, z, mx, 272, HELL, MITTE);

  if (w.regenArt) {
    snprintf(z, sizeof(z), w.regenArt == 1 ? TX("Regen ab %s", "Rain from %s") : TX("Regen bis %s", "Rain until %s"),
             w.regenZeit);
    int bz = breite(bild, z);
    tropfenSymbol(mx - bz / 2 - 6, 292, farbe(130, 190, 255));
    schreibe(bild, z, mx + 5, 296, farbe(150, 205, 255), MITTE);
    return;
  }
  snprintf(z, sizeof(z), "%d km/h", (int)lroundf(w.wind));
  schreibe(bild, z, ox + 10, 296, HELL, LINKS);
  snprintf(z, sizeof(z), "%d%%", w.feuchte);
  schreibe(bild, z, ox + B - 10, 296, HELL, RECHTS);
  tropfenSymbol(ox + B - 17 - breite(bild, z), 292, farbe(130, 190, 255));
}

void seiteJetzt(int ox, uint32_t t) {
  titel(ox, w.ort, t);
  if (hoch()) return seiteJetztHoch(ox, t);

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
  titel(ox, hoch() ? TX("24 Stunden", "Next 24 hours") : TX("Nächste 24 Stunden", "Next 24 hours"), t);

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

  // Kurve, Regenbalken (Unterkante RB) und Uhrzeiten (UY) - hochkant schmaler und hoeher
  const bool hk = hoch();
  const int X0 = hk ? 16 : 20, X1 = hk ? B - 16 : 300, YO = hk ? 100 : 56, YU = hk ? 220 : 116;
  const int RB = hk ? 262 : 141, RH = hk ? 28 : 18, UY = hk ? 284 : 157;
  const int MARKE = hk ? 8 : 6;  // Beschriftung alle ... Stunden
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
      int h = w.stdRegen[i] * RH / 100;
      if (h <= 0 || px(i) > xEnde) continue;
      int bb = hk ? 4 : 8;  // hochkant liegen die Stunden enger
      bild.fillRect(px(i) - bb / 2, RB - h, bb, h, bild.alphaBlend(200, blau, hintergrund(px(i), RB - 6)));
    }
  } else {
    schrift(bild, S_K);
    schreibe(bild, TX("Kein Regen erwartet", "No rain expected"), ox + B / 2, RB - 3, HELL, MITTE);
  }

  // Punkte alle 6 Stunden mit Temperatur und Uhrzeit
  float puls = 0.5f + 0.5f * sinf(t * 0.005f);
  for (int i = 0; i < n; i++) {
    bool marke = i == 0 || (i >= (hk ? 5 : 3) && w.stdStunde[i] % MARKE == 0);
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
    schreibe(bild, uhr, x, UY, HELL, MITTE);
  }
}

// ------------------------------------------------------------
// Seite 3: heute, morgen, uebermorgen
// ------------------------------------------------------------
// hochkant: die drei Tage untereinander, je eine breite Karte
void seiteTageHoch(int ox, uint32_t t) {
  for (int i = 0; i < 3; i++) {
    int x = ox + 8, y = HOCH_TOP + i * 84, kb = B - 16, kh = 78, my = y + kh / 2;
    uint16_t karte = bild.alphaBlend(i == 0 ? 95 : 70, TFT_BLACK, hintergrund(ox + B / 2, my));
    bild.fillSmoothRoundRect(x, y, kb, kh, 10, karte);

    const char *name = i == 0 ? TX("Heute", "Today") : i == 1 ? TX("Morgen", "Tomorrow") : TX(WOCHENTAG, WOCHENTAG_EN)[w.tagWochentag[i]];
    schrift(bild, S_MF);
    if (breite(bild, name) > 96) name = TX(WOCHENTAG_KURZ, WOCHENTAG_KURZ_EN)[w.tagWochentag[i]];
    schreibe(bild, name, x + 10, y + 22, WEISS, LINKS);

    zeichneBild(bild, bildFuerCode(w.tagCode[i]), true, x + 30, my + 12, 44, t + i * 700);

    schrift(bild, S_GF);
    schreibeTemp(bild, w.tagMax[i], x + kb - 10, y + 32, WEISS, RECHTS);
    schrift(bild, S_K);
    schreibeTemp(bild, w.tagMin[i], x + kb - 10, y + 52, HELL, RECHTS);
    char z[8];
    snprintf(z, sizeof(z), "%d%%", w.tagRegen[i]);
    schreibe(bild, z, x + kb - 10, y + 70, HELL, RECHTS);
    tropfenSymbol(x + kb - 17 - breite(bild, z), y + 66, farbe(130, 190, 255));
  }
}

void seiteTage(int ox, uint32_t t) {
  titel(ox, TX("Die nächsten Tage", "Next days"), t);
  if (hoch()) return seiteTageHoch(ox, t);

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
  // quer: Bogen links, Mond rechts - hochkant: Bogen oben, Mond darunter
  const bool hk = hoch();
  const int CX = hk ? ox + B / 2 : ox + 86, CY = hk ? 150 : 124, R = 64;
  int auf = minuten(w.sonneAuf), unter = minuten(w.sonneUnter);
  struct tm zeit;
  int jetzt = ortszeit(zeit) ? zeit.tm_hour * 60 + zeit.tm_min : -1;
  float frac = unter > auf && jetzt >= 0 ? float(jetzt - auf) / (unter - auf) : -1;

  bild.drawFastHLine(CX - 78, CY, 156, bild.alphaBlend(120, WEISS, hintergrund(CX, CY)));
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
  schreibe(bild, w.sonneAuf, CX - 78, CY + 18, WEISS, LINKS);
  schreibe(bild, w.sonneUnter, CX + 78, CY + 18, WEISS, RECHTS);
  int dauer = unter - auf;
  if (dauer > 0) {
    char z[24];
    snprintf(z, sizeof(z), TX("Tag %d h %02d min", "Day %d h %02d min"), dauer / 60, dauer % 60);
    if (hk) schreibe(bild, z, CX, CY + 38, HELL, MITTE);
    else schreibe(bild, z, ox + 8, 160, HELL, LINKS);
  }

  // Mondphase rechts (hochkant unten)
  time_t sek;
  time(&sek);
  float p = mondphase(sek);
  if (hk) {
    zeichneMondphase(bild, CX, 230, 24, p);
    schrift(bild, S_M);
    schreibe(bild, mondName(p), CX, 276, WEISS, MITTE);
    schrift(bild, S_K);
    char z[40];
    snprintf(z, sizeof(z), TX("%d %% hell · UV %d", "%d %% lit · UV %d"), (int)lroundf((1 - cosf(p * 2 * PI)) / 2 * 100),
             (int)lroundf(w.uv));
    schreibe(bild, z, CX, 298, w.uv >= 6 ? farbe(255, 160, 40) : HELL, MITTE);
    return;
  }
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
  const bool hk = hoch();
  schrift(bild, hk ? S_K : S_M);
  if (!w.luftGueltig) {
    schreibe(bild, TX("Für diesen Ort gibt es", "No air quality data"), ox + B / 2, H / 2 - 8, HELL, MITTE);
    schreibe(bild, TX("keine Luftdaten", "for this place"), ox + B / 2, H / 2 + 14, HELL, MITTE);
    return;
  }

  // Europaeischer Luftqualitaetsindex als Ring
  static const char *STUFE_DE[] = {"Sehr gut", "Gut", "Mäßig", "Schlecht", "Sehr schlecht", "Extrem"};
  static const char *STUFE_EN[] = {"Very good", "Good", "Moderate", "Poor", "Very poor", "Extreme"};
  const char **STUFE = TX(STUFE_DE, STUFE_EN);
  static const uint16_t FARBE[] = {0x4F6A, 0x8F68, 0xFE84, 0xFC64, 0xF9E8, 0xA01F};
  int stufe = constrain(w.aqi / 20, 0, 5);
  uint16_t f = FARBE[stufe];
  // quer: Ring links, Pollen rechts - hochkant: Ring oben, Pollen darunter
  const int CX = hk ? ox + B / 2 : ox + 76, CY = hk ? 112 : 84, R = hk ? 46 : 50;
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
  if (hk) {
    snprintf(z, sizeof(z), TX("Feinstaub %d", "PM2.5: %d"), (int)lroundf(w.pm25));
    schreibe(bild, (String(STUFE[stufe]) + " · " + z).c_str(), CX, CY + R + 22, f, MITTE);
  } else {
    schreibe(bild, STUFE[stufe], CX, 148, f, MITTE);
    snprintf(z, sizeof(z), TX("Feinstaub %d", "PM2.5: %d"), (int)lroundf(w.pm25));
    schreibe(bild, z, CX, 165, HELL, MITTE);
  }

  // Pollen: drei Stufen je Art
  const int PX = hk ? ox + 10 : ox + 158, BX = hk ? ox + B - 84 : ox + 238;  // Name, erster Balken
  const int PY = hk ? 210 : 50, PA = hk ? 20 : 24;                           // erste Zeile, Abstand
  schrift(bild, hk ? S_K : S_M);
  for (int i = 0; i < 5; i++) {
    int y = PY + i * PA;
    float k = w.pollen[i];
    int lvl = k < 1 ? 0 : k <= 10 ? 1 : k <= 50 ? 2 : 3;
    schreibe(bild, pollenName(i), PX, y, lvl ? WEISS : HELL, LINKS);
    static const uint16_t PF[] = {0, 0x8F68, 0xFE84, 0xF9E8};
    for (int s = 0; s < 3; s++) {
      uint16_t c = s < lvl ? PF[lvl] : bild.alphaBlend(50, WEISS, hintergrund(BX + 12, y - 6));
      bild.fillSmoothRoundRect(BX + s * 26, y - 11, 22, 9, 4, c);
    }
  }
}

// ------------------------------------------------------------
// Seite 6: Regenradar (Karte + Radarbilder der letzten Stunde, animiert)
// ------------------------------------------------------------
void seiteRadar(int ox, uint32_t t) {
  titel(ox, TX("Regenradar", "Rain radar"), t);
  RadarStand r;
  // nach dem Wechsel quer/hochkant passt der alte Ausschnitt nicht - bis der neue geladen ist
  if (!radarHolen(r) || (!r.karte && !r.bilder) || r.b != B) {
    schrift(bild, hoch() ? S_K : S_M);
    schreibe(bild, TX("Radar wird geladen …", "Loading radar …"), ox + B / 2, H / 2 + 10, HELL, MITTE);
    return;
  }
  const int Y0 = H - r.h;
  // Bilder nacheinander, das neueste bleibt etwas laenger stehen
  int nr = r.bilder ? min((int)((t / 500) % (r.bilder + 4)), r.bilder - 1) : -1;
  const uint8_t *radar = nr >= 0 ? r.bild[nr] : nullptr;
  int xa = max(0, -ox), xe = min(r.b, B - ox);  // nur der sichtbare Teil (beim Umblaettern)
  for (int y = 0; y < r.h; y++) {
    const uint16_t *kz = r.kartePixel + y * r.b;
    const uint8_t *rz = radar ? radar + y * r.b * 3 : nullptr;
    for (int x = xa; x < xe; x++) {
      uint16_t c = r.karte ? kz[x] : farbe(20, 24, 32);
      if (rz && rz[x * 3 + 2] > 8) c = bild.alphaBlend(rz[x * 3 + 2], rz[x * 3] | (rz[x * 3 + 1] << 8), c);
      bild.drawPixel(ox + x, Y0 + y, c);
    }
  }

  // eigener Ort in der Mitte, pulsierend
  int mx = ox + r.b / 2, my = Y0 + r.h / 2;
  float puls = fmodf(t / 1400.0f, 1.0f);
  bild.drawSmoothCircle(mx, my, 4 + puls * 14, bild.alphaBlend(255 * (1 - puls), farbe(255, 198, 44), hintergrund(mx, my - 10)),
                        hintergrund(mx, my - 10));
  bild.fillSmoothCircle(mx, my, 5, TFT_BLACK);
  bild.fillSmoothCircle(mx, my, 3, farbe(255, 198, 44));

  // trocken im ganzen Ausschnitt: deutlich sagen, statt nur eine leere Karte zu zeigen
  if (r.bilder && !r.regen) {
    const char *txt = TX("Kein Regen in der Nähe", "No rain nearby");
    schrift(bild, hoch() ? S_K : S_M);
    int w = min(B - 6, breite(bild, txt) + 24);
    bild.fillSmoothRoundRect(mx - w / 2, my + 16, w, 26, 13, farbe(16, 22, 34), hintergrund(mx, my + 16));
    schreibe(bild, txt, mx, my + (hoch() ? 34 : 35), WEISS, MITTE);
  }

  // Uhrzeit des gezeigten Bilds und Quellen - hochkant die Uhrzeit oben auf der Karte,
  // weil unten die Seitenpunkte sitzen
  int uy = hoch() ? Y0 + 20 : H - 8;
  if (nr >= 0) {
    struct tm z;
    localtime_r(&r.zeit[nr], &z);
    char uhr[8];
    strftime(uhr, sizeof(uhr), "%H:%M", &z);
    schrift(bild, S_MF);
    schreibe(bild, uhr, ox + 8, uy, nr == r.bilder - 1 ? farbe(255, 198, 44) : WEISS, LINKS);
    // kleine Zeitleiste: ein Strich je Bild
    for (int i = 0; i < r.bilder; i++)
      bild.fillRect(ox + 62 + i * 7, uy - 6, 5, 4, i == nr ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(ox + 62 + i * 7, uy - 6)));
  } else {
    schrift(bild, S_K);
    schreibe(bild, TX("Noch keine Radarbilder", "No radar images yet"), ox + 8, uy, HELL, LINKS);
  }
  schrift(bild, S_K);
  schreibe(bild, "© OSM · RainViewer", ox + B - 6, hoch() ? H - 14 : H - 8, HELL, RECHTS);
}

// ------------------------------------------------------------
// Seite 7: die letzten 7 Tage (Temperatur, Luftdruck, Regen)
// ------------------------------------------------------------
void seiteVerlauf(int ox, uint32_t t) {
  titel(ox, TX("Letzte 7 Tage", "Last 7 days"), t);
  static Verlauf v;  // 2 KB - nicht auf den Stack
  if (!verlaufHolen(v)) {
    schrift(bild, hoch() ? S_K : S_M);
    schreibe(bild, TX("Verlauf wird geladen …", "Loading history …"), ox + B / 2, H / 2 + 10, HELL, MITTE);
    return;
  }
  // Diagramm (YO..YU), Regen (Unterkante RU, Hoehe RH), Wochentage (WY) - hochkant schmaler und hoeher
  const bool hk = hoch();
  const int X0 = hk ? 30 : 36, X1 = hk ? B - 8 : 308, YO = hk ? 76 : 36, YU = hk ? 226 : 116;
  const int RU = hk ? 268 : 140, RH = hk ? 26 : 18, WY = hk ? 290 : 156;
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
  // hochkant ist ein Tag nur ~19 Pixel breit - dort nur der Anfangsbuchstabe
  static const char *WT1_DE[] = {"S", "M", "D", "M", "D", "F", "S"};
  static const char *WT1_EN[] = {"S", "M", "T", "W", "T", "F", "S"};
  const char **WT = hk ? TX(WT1_DE, WT1_EN) : TX(WT_DE, WT_EN);
  schrift(bild, S_K);
  for (int d = 0; d <= 8; d++) {
    int a = max(0, d * 24 - v.ersteStunde), e = min(n - 1, (d + 1) * 24 - v.ersteStunde - 1);
    if (a >= n) break;
    if (a > 0) {
      for (int y = YO; y < RU; y += 3) bild.drawPixel(px(a), y, bild.alphaBlend(90, WEISS, hintergrund(px(a), y)));
    }
    if (e - a >= 8) schreibe(bild, WT[(v.ersterWochentag + d) % 7], (px(a) + px(e)) / 2, WY, HELL, MITTE);
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
  // aktueller Luftdruck = letzter vorhandene Wert (die juengste Stunde fehlt manchmal noch)
  int letzter = n - 1;
  while (letzter > 0 && isnan(v.druck[letzter])) letzter--;
  if (!isnan(v.druck[letzter])) {
    char z[16];
    snprintf(z, sizeof(z), "%d hPa", (int)lroundf(v.druck[letzter]));
    schreibe(bild, z, ox + X1, YO - (hk ? 10 : 4), farbe(140, 200, 255), RECHTS);
  }
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
  if (hoch()) {
    // hochkant: je Ort zwei Zeilen - oben Name, darunter Temperatur und Hoechst-/Tiefstwert
    int zh = min(64, 252 / n);
    for (int i = 0; i < n; i++) {
      int y = HOCH_TOP + i * zh, mitte = y + (zh - 4) / 2;
      bild.fillSmoothRoundRect(ox + 8, y, B - 16, zh - 4, 8, bild.alphaBlend(i ? 70 : 100, TFT_BLACK, hintergrund(ox + B / 2, mitte)));
      const OrtWetter &o = liste[i];
      if (o.gueltig) zeichneBild(bild, bildFuerCode(o.code), o.tag, ox + 32, mitte + 2, min(40, zh - 10), t + i * 500);
      schrift(bild, S_MF);
      schreibe(bild, kuerzen(o.name, B - 72).c_str(), ox + 58, mitte - 6, WEISS, LINKS);
      schrift(bild, S_K);
      if (!o.gueltig) {
        schreibe(bild, "…", ox + 58, mitte + 16, HELL, LINKS);
        continue;
      }
      schrift(bild, S_MF);
      schreibeTemp(bild, o.temp, ox + 58, mitte + 18, WEISS, LINKS);
      schrift(bild, S_K);
      char z[24];
      snprintf(z, sizeof(z), "%d° / %d°", (int)lroundf(o.max), (int)lroundf(o.min));
      schreibe(bild, z, ox + B - 14, mitte + 17, HELL, RECHTS);
    }
    return;
  }
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
  if (ortszeit(zeit)) strftime(uhr, sizeof(uhr), "%H:%M", &zeit);
  schrift(bild, S_MF);
  if (hoch()) schreibe(bild, uhr, 8, 18, WEISS, LINKS);  // hochkant: Uhr links, Titel eine Zeile tiefer
  else schreibe(bild, uhr, B - 37, 18, WEISS, RECHTS);

  // Wetterdienst gerade nicht erreichbar: alte Daten mit "Stand 14:30" kennzeichnen, damit niemand getaeuscht wird;
  // Daten vom Ersatzdienst mit dessen Namen
  char hinweis[24] = "";
  uint16_t hinweisFarbe = farbe(255, 190, 90);
  if (w.stand && time(nullptr) - w.stand > 20 * 60) {
    struct tm s;
    localtime_r(&w.stand, &s);
    strftime(hinweis, sizeof(hinweis), TX("Stand %H:%M", "as of %H:%M"), &s);
  } else if (w.quelle) {
    snprintf(hinweis, sizeof(hinweis), hoch() ? "%s" : "via %s", quellenName(w.quelle));
    hinweisFarbe = farbe(140, 200, 255);
  }
  if (*hinweis) {
    int uhrBreite = breite(bild, uhr);
    schrift(bild, S_K);
    if (hoch()) schreibe(bild, hinweis, 8 + uhrBreite + 8, 18, hinweisFarbe, LINKS);
    else schreibe(bild, hinweis, B - 9, 33, hinweisFarbe, RECHTS);
  }

  // Akku-Symbol
  int x = B - 31, y = 6;
  bild.drawRoundRect(x, y, 22, 12, 2, WEISS);
  bild.fillRect(x + 22, y + 4, 2, 4, WEISS);
  // haengt am USB? Mit Abstand zwischen Ein- und Ausschalten, sonst springt das Symbol hin und her,
  // wenn die Spannung knapp an der Schwelle liegt
  static bool usb = false;
  if (akkuVolt > 4.3f) usb = true;
  else if (akkuVolt < 4.15f) usb = false;
  if (usb) {
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
    bild.fillSmoothCircle(x, H - 5, 2, i == seite ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(x, H - 5)));
    x += 10;
  }
}

void lichtAnzeige(uint32_t t) {
  if ((int32_t)(lichtAnzeigeBis - t) <= 0) return;
  int x = B / 2 - 62, y = H - 44;
  bild.fillSmoothRoundRect(x, y, 124, 26, 13, bild.alphaBlend(200, TFT_BLACK, hintergrund(B / 2, y + 13)));
  for (int k = 0; k < 4; k++) {
    bool an = k < 4 - einst.licht;
    bild.fillSmoothRoundRect(x + 12 + k * 26, y + 8, 22, 10, 4, an ? farbe(255, 210, 70) : farbe(70, 74, 84));
  }
}

// Taste IO0 lange halten: neue Sprache, dazu wo die Einstellungsseite ist
void infoAnzeige(uint32_t t) {
  if ((int32_t)(infoBis - t) <= 0) return;
  int bb = min(260, B - 10), y0 = H / 2 - 62;  // Kasten in der Mitte, quer wie hochkant
  bild.fillSmoothRoundRect(B / 2 - bb / 2, y0, bb, 124, 14, bild.alphaBlend(225, TFT_BLACK, hintergrund(B / 2, H / 2)));
  // oben die (gerade gewaehlte) Sprache, darunter wo die Einstellungsseite ist
  schrift(bild, S_GF);
  schreibe(bild, TX("Deutsch", "English"), B / 2, y0 + 28, farbe(255, 198, 44), MITTE);
  schrift(bild, S_K);
  schreibe(bild, kuerzen(TX("Einstellungen im Browser:", "Settings in your browser:"), bb - 12).c_str(), B / 2, y0 + 50, HELL, MITTE);
  String adresse = String(einst.name) + ".local";
  schrift(bild, S_M);
  if (breite(bild, adresse.c_str()) > bb - 12) schrift(bild, S_K);
  schreibe(bild, kuerzen(adresse, bb - 12).c_str(), B / 2, y0 + 72, WEISS, MITTE);
  schrift(bild, S_K);
  schreibe(bild, (String(TX("oder ", "or ")) + WiFi.localIP().toString()).c_str(), B / 2, y0 + 92, WEISS, MITTE);
  char z[40];
  if (FIRMWARE_BUILD) snprintf(z, sizeof(z), "Build %d", FIRMWARE_BUILD);
  else strcpy(z, TX("eigener Build", "own build"));
  if (updateNeuerBuild()) snprintf(z, sizeof(z), TX("Build %d - neu: %d", "Build %d - new: %d"), FIRMWARE_BUILD, updateNeuerBuild());
  schreibe(bild, z, B / 2, y0 + 112, HELL, MITTE);
}

// ------------------------------------------------------------
// Startbild, solange noch keine Wetterdaten da sind
// ------------------------------------------------------------
// Schlafende Wolke (Wetterdienst macht Pause): schwebt sanft auf und ab, kleine "z" steigen auf.
// keinNetz: wach, grau und rot durchgestrichen
void schlafendeWolke(int cx, int cy, uint32_t t, bool keinNetz) {
  cy += lroundf(3 * sinf(t / 700.0f));
  const uint16_t hell = keinNetz ? farbe(176, 184, 198) : farbe(240, 244, 250);
  const uint16_t schatten = keinNetz ? farbe(110, 118, 134) : farbe(168, 182, 210);
  const uint16_t gesicht = farbe(60, 66, 84);
  for (int o = 3; o >= 0; o -= 3) {  // erst der Schatten leicht versetzt, dann die Wolke
    uint16_t f = o ? schatten : hell;
    bild.fillSmoothRoundRect(cx - 40 + o, cy - 2 + o, 80, 26, 13, f);
    bild.fillSmoothCircle(cx - 18 + o, cy - 2 + o, 17, f);
    bild.fillSmoothCircle(cx + 8 + o, cy - 10 + o, 22, f);
    bild.fillSmoothCircle(cx + 30 + o, cy + 4 + o, 13, f);
  }
  if (keinNetz) {
    bild.fillSmoothCircle(cx - 9, cy + 5, 3, gesicht);
    bild.fillSmoothCircle(cx + 13, cy + 5, 3, gesicht);
    bild.drawWideLine(cx - 4, cy + 15, cx + 8, cy + 15, 2, gesicht);
    bild.drawWideLine(cx - 36, cy + 26, cx + 42, cy - 28, 5, farbe(240, 80, 70));
    return;
  }
  // geschlossene Augen, rote Baeckchen, kleiner offener Mund
  bild.drawSmoothArc(cx - 9, cy + 3, 6, 4, 290, 70, gesicht, hell, true);
  bild.drawSmoothArc(cx + 13, cy + 3, 6, 4, 290, 70, gesicht, hell, true);
  const uint16_t rosa = bild.alphaBlend(110, farbe(255, 140, 150), hell);
  bild.fillSmoothCircle(cx - 20, cy + 12, 4, rosa);
  bild.fillSmoothCircle(cx + 24, cy + 12, 4, rosa);
  bild.fillSmoothCircle(cx + 2, cy + 15, 3, gesicht);
  // drei "z" steigen nacheinander schraeg nach oben und werden dabei groesser und blasser
  bild.setTextDatum(L_BASELINE);
  for (int i = 0; i < 3; i++) {
    float p = ((t + i * 900) % 2700) / 2700.0f;
    int zx = cx + 32 + p * 24 + 3 * sinf(p * 6.3f + i), zy = cy - 22 - p * 30;
    float sichtbar = min(1.0f, min(p * 5, (1 - p) * 2.5f));
    schrift(bild, p < 0.4f ? S_K : S_M);
    bild.setTextColor(bild.alphaBlend(255 * sichtbar, WEISS, hintergrund(zx + 4, zy - 5)));
    bild.drawString("z", zx, zy);
  }
}

// Zeile mittig, bei Bedarf in kleinerer Schrift, damit sie auf den Bildschirm passt
static void zeileMittig(const char *txt, int y, uint16_t f, Schrift s) {
  schrift(bild, s);
  if (breite(bild, txt) > B - 12) schrift(bild, S_M);
  if (breite(bild, txt) > B - 12) schrift(bild, S_K);
  schreibe(bild, kuerzen(txt, B - 8).c_str(), B / 2, y, f, MITTE);
}

void startBild(uint32_t t) {
  zeichneHimmel(bild, HEITER, false, t, false);
  const int dy = hoch() ? 54 : 0;  // hochkant alles etwas tiefer in die Mitte

  // Wetterdienst nicht erreichbar: schlafende Wolke, was los ist und wann der naechste Versuch kommt
  if (wetterStatus() == FEHLER) {
    WetterFehler f = wetterFehler();
    schlafendeWolke(B / 2 - 6, 66 + dy, t, f == KEIN_INTERNET);
    const char *titel, *grund;
    String netz = String(TX("WLAN ok: ", "Wi-Fi OK: ")) + WiFi.SSID();
    if (f == UEBERLASTET) {
      titel = TX("Wetterdienst überlastet", "Weather service overloaded");
      grund = TX("Er macht gerade Pause", "It's taking a short break");
    } else if (f == KEIN_INTERNET) {
      titel = TX("Keine Internetverbindung", "No internet connection");
      grund = netz.c_str();
    } else {
      titel = TX("Wetterdienst nicht erreichbar", "Weather service unreachable");
      grund = TX("Open-Meteo antwortet nicht", "Open-Meteo is not responding");
    }
    zeileMittig(titel, 128 + dy, f == KEIN_INTERNET ? farbe(255, 120, 110) : WEISS, hoch() ? S_MF : S_GF);
    char versuch[40];
    static const char *punkte[] = {"", ".", "..", "..."};
    int s = wetterNaechsterVersuch();
    if (s > 0) snprintf(versuch, sizeof(versuch), TX("neuer Versuch in %d:%02d", "retrying in %d:%02d"), s / 60, s % 60);
    else snprintf(versuch, sizeof(versuch), TX("versuche es gerade%s", "trying now%s"), punkte[(t / 400) % 4]);
    schrift(bild, S_K);
    schreibe(bild, kuerzen(grund, B - 8).c_str(), B / 2, 149 + dy, HELL, MITTE);
    schreibe(bild, versuch, B / 2, 165 + dy, farbe(150, 160, 190), MITTE);
    return;
  }

  zeichneBild(bild, HEITER, true, B / 2, 62 + dy, 100, t);

  schrift(bild, hoch() ? S_MF : S_GF);
  schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, B - 20).c_str(), B / 2, 128 + dy, WEISS, MITTE);

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
      zeile1[0] = 0;
      break;
  }
  schrift(bild, S_K);
  schreibe(bild, kuerzen(zeile1, B - 8).c_str(), B / 2, 149 + dy, HELL, MITTE);
  schreibe(bild, kuerzen(zeile2, B - 8).c_str(), B / 2, 165 + dy, farbe(150, 160, 190), MITTE);
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
  const bool hk = hoch();
  const int dy = hk ? 54 : 0;  // hochkant: Bild + Text tiefer in die Mitte
  schrift(bild, hk ? S_MF : S_GF);

  if (hk && (phase == WLAN_EINRICHTUNG || phase == WLAN_ORT)) {
    // hochkant: Symbol oben, Anleitung in kurzen Zeilen darunter
    const uint16_t gelb = farbe(255, 198, 44);
    bool ort = phase == WLAN_ORT;
    schreibe(bild, ort ? TX("WLAN verbunden!", "Wi-Fi connected!") : TX("WLAN einrichten", "Wi-Fi setup"), B / 2, 30,
             ort ? farbe(90, 220, 110) : WEISS, MITTE);
    if (ort) ortSymbol(B / 2, 88, t);
    else wlanSymbol(B / 2, 112, t);
    struct Zeile { const char *de, *en; bool gelb; };
    static const Zeile EINR[] = {{"1. Mit dem Handy", "1. Connect your phone", false}, {"ins WLAN", "to the Wi-Fi", false},
                                 {WLAN_SETUP_NAME, WLAN_SETUP_NAME, true}, {"2. Die Seite öffnet sich", "2. A page opens", false},
                                 {"selbst: dein WLAN", "by itself: choose", false}, {"wählen, Passwort", "your Wi-Fi, enter", false},
                                 {"eingeben", "the password", false}};
    static const Zeile ORT[] = {{"3. Jetzt am Handy:", "3. Now on your phone:", false}, {"Wo steht die", "Where is the", true},
                                {"Station?", "station?", true}, {"Ort oder PLZ suchen", "Search town or", false},
                                {"oder Vorschlag", "postcode or tap", false}, {"antippen", "the suggestion", false}};
    const Zeile *z = ort ? ORT : EINR;
    int n = ort ? 6 : 7;
    for (int i = 0; i < n; i++) {
      const char *txt = TX(z[i].de, z[i].en);
      schrift(bild, z[i].gelb ? S_MF : S_K);
      if (breite(bild, txt) > B - 8) schrift(bild, S_M);  // "Wetterstation-Setup" ist fett zu breit
      if (breite(bild, txt) > B - 8) schrift(bild, S_K);
      schreibe(bild, txt, B / 2, 162 + i * 21, z[i].gelb ? gelb : HELL, MITTE);
    }
  } else if (phase == WLAN_SUCHE) {
    zeichneBild(bild, HEITER, true, B / 2, 62 + dy, 100, t);
    schreibe(bild, kuerzen(String(TX("Wetter ", "Weather ")) + ortAktuell().name, B - 20).c_str(), B / 2, 128 + dy, WEISS, MITTE);
    schrift(bild, S_K);
    schreibe(bild, TX("Verbinde mit WLAN ...", "Connecting to Wi-Fi ..."), B / 2, 149 + dy, HELL, MITTE);
    schreibe(bild, kuerzen(netz, B - 8).c_str(), B / 2, 165 + dy, farbe(150, 160, 190), MITTE);
  } else if (phase == WLAN_EINRICHTUNG || phase == WLAN_ORT) {
    // quer: Symbol links, Anleitung rechts daneben; zu breite Zeilen in kleinerer Schrift
    auto zeile = [](const char *txt, int y, uint16_t f, Schrift s) {
      schrift(bild, s);
      if (breite(bild, txt) > B - 98) schrift(bild, s == S_MF ? S_M : S_K);
      if (breite(bild, txt) > B - 98) schrift(bild, S_K);
      schreibe(bild, txt, 94, y, f, LINKS);
    };
    const uint16_t gelb = farbe(255, 198, 44);
    if (phase == WLAN_EINRICHTUNG) {
      schreibe(bild, TX("WLAN einrichten", "Wi-Fi setup"), B / 2, 27, WEISS, MITTE);
      wlanSymbol(46, 110, t);
      zeile(TX("1. Handy ins WLAN:", "1. Connect your phone to:"), 60, HELL, S_M);
      zeile(WLAN_SETUP_NAME, 82, gelb, S_MF);
      zeile(TX("2. Seite öffnet sich selbst:", "2. A page opens by itself:"), 112, HELL, S_M);
      zeile(TX("dein WLAN wählen und", "choose your Wi-Fi and"), 134, HELL, S_M);
      zeile(TX("Passwort eingeben", "enter the password"), 156, HELL, S_M);
    } else {
      schreibe(bild, TX("WLAN verbunden!", "Wi-Fi connected!"), B / 2, 27, farbe(90, 220, 110), MITTE);
      ortSymbol(46, 100, t);
      zeile(TX("3. Jetzt am Handy:", "3. Now on your phone:"), 62, HELL, S_M);
      zeile(TX("Wo steht die Station?", "Where is the station?"), 86, gelb, S_MF);
      zeile(TX("Ort oder PLZ suchen", "Search town or postcode"), 118, HELL, S_M);
      zeile(TX("oder Vorschlag antippen", "or tap the suggestion"), 140, HELL, S_M);
    }
  } else if (phase == WLAN_TESTE) {
    zeichneBild(bild, HEITER, true, B / 2, 62 + dy, 100, t);
    schreibe(bild, TX("Verbinde ...", "Connecting ..."), B / 2, 128 + dy, WEISS, MITTE);
    schrift(bild, S_K);
    schreibe(bild, kuerzen(netz, B - 8).c_str(), B / 2, 152 + dy, farbe(255, 198, 44), MITTE);
  } else if (phase == WLAN_OK) {
    zeichneBild(bild, KLAR, true, B / 2, 62 + dy, 100, t);
    schreibe(bild, TX("Fertig!", "Done!"), B / 2, 128 + dy, farbe(90, 220, 110), MITTE);
    schrift(bild, S_K);
    String z = *netz ? String(TX("Wetter für ", "Weather for ")) + netz : String(TX("Handy wieder ins eigene WLAN", "Phone back to your Wi-Fi"));
    schreibe(bild, kuerzen(z, B - 8).c_str(), B / 2, 152 + dy, HELL, MITTE);
  } else {
    zeichneBild(bild, GEWITTER, true, B / 2, 58 + dy, 92, t);
    schreibe(bild, TX("Hat nicht geklappt", "That did not work"), B / 2, 128 + dy, farbe(255, 110, 100), MITTE);
    schrift(bild, S_K);
    if (hk) {
      schreibe(bild, TX("Passwort falsch?", "Wrong password?"), B / 2, 152 + dy, HELL, MITTE);
      schreibe(bild, TX("Nochmal versuchen", "Try again"), B / 2, 172 + dy, HELL, MITTE);
    } else {
      schreibe(bild, TX("Passwort falsch? Nochmal versuchen", "Wrong password? Try again"), B / 2, 152, HELL, MITTE);
    }
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
  const int dy = hoch() ? 75 : 0, bx = hoch() ? 16 : 40, bb = B - 2 * bx;  // hochkant: tiefer und schmaler
  schrift(bild, S_GF);
  schreibe(bild, TX("Neue Version", "New version"), B / 2, 56 + dy, WEISS, MITTE);

  bild.fillSmoothRoundRect(bx, 74 + dy, bb, 16, 8, farbe(40, 48, 80));
  if (prozent > 0) bild.fillSmoothRoundRect(bx, 74 + dy, max(16, bb * prozent / 100), 16, 8, farbe(255, 198, 44));

  char z[16];
  snprintf(z, sizeof(z), "%d %%", prozent);
  schrift(bild, S_M);
  schreibe(bild, z, B / 2, 120 + dy, WEISS, MITTE);
  schrift(bild, S_K);
  schreibe(bild, kuerzen(text, B - 8).c_str(), B / 2, 146 + dy, HELL, MITTE);
  bild.pushSprite(0, 0);
}

void updateStart() {
  ArduinoOTA.setHostname(einst.name);  // meldet auch name.local im Netz an (mDNS)
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
  Serial.printf("Update ueber WLAN bereit: %s.local (%s)\n", einst.name, WiFi.localIP().toString().c_str());
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
  if (!einst.nacht || einst.nachtVon == einst.nachtBis || !ortszeit(zeit)) return false;
  int h = zeit.tm_hour;
  return einst.nachtVon < einst.nachtBis ? h >= einst.nachtVon && h < einst.nachtBis
                                         : h >= einst.nachtVon || h < einst.nachtBis;
}

static int lichtZuletzt = -1;  // -1 = beim naechsten Mal sicher neu setzen

void lichtSetzen(uint32_t t) {
  int &zuletzt = lichtZuletzt;
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
    bool warDunkel = istNacht() && t - letzterTastendruck > 20000;
    if (t - seiteSeitDruck < 1000 && !warDunkel) {  // nachts weckt der erste Druck nur auf (wie IO14)
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

// Ausrichtung uebernehmen: Display drehen, bei quer <-> hochkant den Bildspeicher neu anlegen.
// Lage 0 = quer (USB rechts), 1 = quer gedreht, 2 = hochkant (USB unten), 3 = hochkant (USB oben)
void lageSetzen() {
  static int gesetzt = -1;
  if (einst.lage == gesetzt) return;
  gesetzt = einst.lage;
  static const uint8_t DREHUNG[4] = {1, 3, 0, 2};  // TFT_eSPI setRotation
  tft.setRotation(DREHUNG[einst.lage]);
  tft.fillScreen(TFT_BLACK);
  int nb = einst.lage >= 2 ? 170 : 320, nh = einst.lage >= 2 ? 320 : 170;
  if (bild.created() && nb == B) return;  // nur gedreht - gleiche Groesse
  B = nb, H = nh;
  bild.deleteSprite();
  // Bildspeicher fuer den ganzen Bildschirm (liegt im PSRAM)
  if (!bild.createSprite(B, H)) {
    Serial.println("Zu wenig Speicher - nehme 8 Bit Farbe");
    bild.setColorDepth(8);
    bild.createSprite(B, H);
  }
  bild.setTextWrap(false, false);  // zu lange Zeilen abschneiden statt links neu anfangen
  wechsel = false;  // laufende Schiebe-Animation passt nicht mehr
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
  lageSetzen();  // Ausrichtung (quer/hochkant) und Bildspeicher

  pinMode(LICHT_PIN, OUTPUT);
  lichtStufe(HELLIGKEIT[einst.licht]);

  // GPIO 14 beim Einschalten gedrueckt halten = WLAN neu einrichten
  bool einrichten = digitalRead(TASTER_LICHT) == LOW;
  ortLaden();
  orteLaden();
  wlanVerbinden(einrichten, wlanBild);

  // bis die ersten Wetterdaten da sind: mitteleuropaeische Zeit, danach die Zeitzone des Orts
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.cloudflare.com");
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
  if (hatDaten) zeitzoneSetzen(w.utcAbweichung);
  tasten(t, hatDaten);
  lichtSetzen(t);

  // Ausrichtung wurde auf der Einstellungsseite umgeschaltet -> sofort uebernehmen
  lageSetzen();

  // Neue Version aus dem Internet: auf Wunsch sofort, sonst wenn gerade niemand die Tasten benutzt
  bool gewuenscht = updateInstallierenGewuenscht();
  if (updateNeuerBuild() && (gewuenscht || (einst.autoUpdate && t - letzterTastendruck > 60000))) {
    if (gewuenscht || !istNacht()) lichtStufe(16);  // nachts bleibt das Display beim Selbst-Update dunkel
    updateInstallieren(updateBild);
    lichtZuletzt = -1;  // nur bei Fehler: Helligkeit wieder richtig setzen
  }

  if (t - akkuZeit > 2000) {
    akkuVolt = akkuVolt * 0.7f + akkuSpannung() * 0.3f;  // geglaettet, damit der Balken nicht zappelt
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

  // hoechstens ca. 30 Bilder pro Sekunde: sieht gleich aus, spart Strom und haelt das Board kuehl
  static uint32_t bildStart = 0;
  int32_t rest = 33 - (int32_t)(millis() - bildStart);
  if (rest > 0) delay(rest);
  bildStart = millis();
}

// Zeitzone des Orts (von Open-Meteo, mit Sommerzeit) als feste Abweichung von UTC uebernehmen.
// Wird mit jedem Wetterabruf (alle 15 min) aktualisiert - so stimmt die Uhr auf der ganzen Welt.
void zeitzoneSetzen(int32_t abweichung) {
  static int32_t gesetzt = INT32_MIN;
  if (abweichung == gesetzt) return;
  gesetzt = abweichung;
  int32_t a = abs(abweichung);
  char tz[24];
  // POSIX zaehlt andersherum: "STD-1" heisst UTC+1
  snprintf(tz, sizeof(tz), "STD%c%d:%02d", abweichung >= 0 ? '-' : '+', a / 3600, a % 3600 / 60);
  setenv("TZ", tz, 1);
  tzset();
  Serial.printf("Zeitzone: UTC%+.2g h (%s)\n", abweichung / 3600.0f, tz);
}
