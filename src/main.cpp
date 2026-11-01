// TTGO T-Display - Wetterstation
// Standort: am Handy gewaehlt (Einrichtung), sonst grob ueber die Internet-Adresse, sonst Cottbus
//   Taster GPIO 0:  naechste Seite
//   Taster GPIO 35: kurz = Helligkeit, 3 s halten = WLAN neu einrichten
// Seiten: Jetzt, naechste 24 Stunden, naechste Tage (wechseln auch von allein)
// Wetterdaten: open-meteo.com
// WLAN: Einrichtung per Handy ueber das WLAN "Wetterstation-Setup" (siehe wlan.cpp)
// Updates ueber WLAN: Umgebung "env:tdisplay_wlan" (Passwort in geheim.ini)

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "wetter.h"
#include "bilder.h"
#include "wlan.h"
#include "ort.h"

#ifndef OTA_PASSWORT
#define OTA_PASSWORT ""
#endif
#ifndef FIRMWARE_BUILD
#define FIRMWARE_BUILD 0  // setzt GitHub beim Bauen (Build-Nummer)
#endif

// Pins (siehe INFO.md)
const int TASTER_SEITE = 0;
const int TASTER_LICHT = 35;
const int AKKU_PIN     = 34;
const int AKKU_EIN     = 14;  // schaltet den Spannungsteiler am Akku ein
const int LICHT_PIN    = 4;   // Hintergrundlicht

const float AKKU_FAKTOR = 2.0f;        // Teiler 1:2 - mit Multimeter nachmessen und anpassen
const uint32_t AUTO_WEITER = 12000;    // so lange bleibt eine Seite stehen
const uint32_t WECHSEL_DAUER = 380;    // Schiebe-Animation zwischen den Seiten

const uint16_t WEISS = TFT_WHITE;
const uint16_t HELL  = 0xDF7E;  // helles Blaugrau

TFT_eSPI tft;
TFT_eSprite bild(&tft);  // ganzer Bildschirm im Speicher - so flackert nichts

const int SEITEN = 3;
int seite = 0, alteSeite = 0;
bool wechsel = false;
uint32_t wechselStart = 0;
uint32_t seiteSeit = 0;
uint32_t letzterTastendruck = 0;

const int HELLIGKEIT[] = {255, 140, 60, 15};
int lichtStufe = 0;
uint32_t lichtAnzeigeBis = 0;

float akkuVolt = 0;
uint32_t akkuZeit = 0;

static Wetter w;

static const char *WOCHENTAG[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
static const char *WOCHENTAG_KURZ[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};

// Taster mit Entprellung: true nur beim Druecken, nicht beim Halten
bool gedrueckt(int pin, bool &vorher) {
  bool jetzt = digitalRead(pin) == LOW;
  bool neu = jetzt && !vorher;
  vorher = jetzt;
  return neu;
}

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

// ------------------------------------------------------------
// Seite 1: Jetzt
// ------------------------------------------------------------
void seiteJetzt(int ox, uint32_t t) {
  bild.setTextFont(2);
  schreibe(bild, kuerzen(w.ort, 160).c_str(), ox + 6, 13, WEISS, LINKS);

  zeichneBild(bild, bildFuerCode(w.code), w.tag, ox + 58, 74, 96, t);

  bild.setFreeFont(&FreeSansBold24pt7b);
  schreibeTemp(bild, w.temp, ox + 118, 66, WEISS, LINKS);

  const char *text = textFuerCode(w.code, w.tag);
  bild.setFreeFont(&FreeSans9pt7b);
  if (breite(bild, text) > 118) bild.setTextFont(2);
  schreibe(bild, text, ox + 118, 88, WEISS, LINKS);

  bild.setTextFont(2);
  schreibe(bild, "Gefühlt", ox + 118, 106, HELL, LINKS);
  schreibeTemp(bild, w.gefuehlt, ox + 122 + breite(bild, "Gefühlt"), 106, HELL, LINKS);

  char z[24];
  snprintf(z, sizeof(z), "Wind %d km/h", (int)lroundf(w.wind));
  schreibe(bild, z, ox + 118, 122, HELL, LINKS);
  snprintf(z, sizeof(z), "%d%%", w.feuchte);
  schreibe(bild, z, ox + 236, 122, HELL, RECHTS);
  tropfenSymbol(ox + 230 - breite(bild, z), 118, farbe(130, 190, 255));
}

// ------------------------------------------------------------
// Seite 2: naechste 24 Stunden als Kurve
// ------------------------------------------------------------
void seiteStunden(int ox, uint32_t t) {
  bild.setTextFont(2);
  schreibe(bild, "Nächste 24 Stunden", ox + 6, 13, WEISS, LINKS);

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
    schreibe(bild, "Kein Regen erwartet", ox + 120, 108, HELL, MITTE);
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
    if (i == 0) strcpy(uhr, "jetzt");
    else snprintf(uhr, sizeof(uhr), "%02d", w.stdStunde[i]);
    schreibe(bild, uhr, x, 125, HELL, MITTE);
  }
}

// ------------------------------------------------------------
// Seite 3: heute, morgen, uebermorgen
// ------------------------------------------------------------
void seiteTage(int ox, uint32_t t) {
  bild.setTextFont(2);
  schreibe(bild, "Die nächsten Tage", ox + 6, 13, WEISS, LINKS);

  for (int i = 0; i < 3; i++) {
    int x = ox + 6 + i * 78, mx = x + 36;
    uint16_t karte = bild.alphaBlend(i == 0 ? 95 : 70, TFT_BLACK, hintergrund(mx, 70));
    bild.fillSmoothRoundRect(x, 20, 72, 106, 8, karte);

    const char *name = i == 0 ? "Heute" : i == 1 ? "Morgen" : WOCHENTAG[w.tagWochentag[i]];
    bild.setTextFont(2);
    if (breite(bild, name) > 66) name = WOCHENTAG_KURZ[w.tagWochentag[i]];
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

void zeichneSeite(int s, int ox, uint32_t t) {
  if (ox <= -240 || ox >= 240) return;
  switch (s) {
    case 0: seiteJetzt(ox, t); break;
    case 1: seiteStunden(ox, t); break;
    case 2: seiteTage(ox, t); break;
  }
}

// ------------------------------------------------------------
// Immer sichtbar: Uhrzeit, Akku, Seitenpunkte, Helligkeit
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

void seitenPunkte() {
  for (int i = 0; i < SEITEN; i++) {
    int x = 112 + i * 8;
    bild.fillSmoothCircle(x, 131, 2, i == seite ? WEISS : bild.alphaBlend(90, WEISS, hintergrund(x, 131)));
  }
}

void lichtAnzeige(uint32_t t) {
  if ((int32_t)(lichtAnzeigeBis - t) <= 0) return;
  bild.fillSmoothRoundRect(70, 104, 100, 20, 10, bild.alphaBlend(200, TFT_BLACK, hintergrund(120, 114)));
  for (int k = 0; k < 4; k++) {
    bool an = k < 4 - lichtStufe;
    bild.fillSmoothRoundRect(80 + k * 21, 110, 17, 8, 3, an ? farbe(255, 210, 70) : farbe(70, 74, 84));
  }
}

// ------------------------------------------------------------
// Startbild, solange noch keine Wetterdaten da sind
// ------------------------------------------------------------
void startBild(uint32_t t) {
  zeichneHimmel(bild, HEITER, false, t, false);
  zeichneBild(bild, HEITER, true, 120, 50, 76, t);

  bild.setFreeFont(&FreeSansBold9pt7b);
  schreibe(bild, kuerzen(String("Wetter ") + ortAktuell().name, 230).c_str(), 120, 100, WEISS, MITTE);

  char zeile1[40], zeile2[40] = "";
  const char *punkte[] = {"", ".", "..", "..."};
  const char *p = punkte[(t / 400) % 4];
  switch (wetterStatus()) {
    case VERBINDE:
      snprintf(zeile1, sizeof(zeile1), "Verbinde mit WLAN%s", p);
      snprintf(zeile2, sizeof(zeile2), "%s", WiFi.SSID().c_str());
      break;
    case ORTUNG:
      snprintf(zeile1, sizeof(zeile1), "Suche Standort%s", p);
      break;
    case LADE:
      snprintf(zeile1, sizeof(zeile1), "Hole Wetterdaten%s", p);
      break;
    default:
      strcpy(zeile1, "Abruf fehlgeschlagen");
      strcpy(zeile2, "neuer Versuch in 1 min");
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
    schreibe(bild, kuerzen(String("Wetter ") + ortAktuell().name, 230).c_str(), 120, 100, WEISS, MITTE);
    bild.setTextFont(2);
    schreibe(bild, "Verbinde mit WLAN ...", 120, 117, HELL, MITTE);
    schreibe(bild, netz, 120, 132, farbe(150, 160, 190), MITTE);
  } else if (phase == WLAN_EINRICHTUNG) {
    schreibe(bild, "WLAN einrichten", 120, 20, WEISS, MITTE);
    wlanSymbol(34, 82, t);
    bild.setTextFont(2);
    schreibe(bild, "1. Mit dem Handy ins WLAN", 64, 46, HELL, LINKS);
    schreibe(bild, WLAN_SETUP_NAME, 64, 64, farbe(255, 198, 44), LINKS);
    schreibe(bild, "2. Seite öffnet sich selbst:", 64, 88, HELL, LINKS);
    schreibe(bild, "dein WLAN wählen und", 64, 104, HELL, LINKS);
    schreibe(bild, "Passwort eingeben", 64, 120, HELL, LINKS);
  } else if (phase == WLAN_TESTE) {
    zeichneBild(bild, HEITER, true, 120, 50, 76, t);
    schreibe(bild, "Verbinde ...", 120, 100, WEISS, MITTE);
    bild.setTextFont(2);
    schreibe(bild, netz, 120, 120, farbe(255, 198, 44), MITTE);
  } else if (phase == WLAN_ORT) {
    schreibe(bild, "WLAN verbunden!", 120, 20, farbe(90, 220, 110), MITTE);
    ortSymbol(34, 80, t);
    bild.setTextFont(2);
    schreibe(bild, "3. Jetzt am Handy:", 64, 50, HELL, LINKS);
    schreibe(bild, "Wo steht die Station?", 64, 68, farbe(255, 198, 44), LINKS);
    schreibe(bild, "Ort oder PLZ suchen", 64, 92, HELL, LINKS);
    schreibe(bild, "oder Vorschlag antippen", 64, 108, HELL, LINKS);
  } else if (phase == WLAN_OK) {
    zeichneBild(bild, KLAR, true, 120, 50, 76, t);
    schreibe(bild, "Fertig!", 120, 100, farbe(90, 220, 110), MITTE);
    bild.setTextFont(2);
    schreibe(bild, *netz ? kuerzen(String("Wetter für ") + netz, 230).c_str() : "Handy wieder ins eigene WLAN", 120, 120,
             HELL, MITTE);
  } else {
    zeichneBild(bild, GEWITTER, true, 120, 48, 70, t);
    schreibe(bild, "Hat nicht geklappt", 120, 100, farbe(255, 110, 100), MITTE);
    bild.setTextFont(2);
    schreibe(bild, "Passwort falsch? Nochmal versuchen", 120, 120, HELL, MITTE);
  }
  bild.pushSprite(0, 0);
}

// ------------------------------------------------------------
// Update ueber WLAN
// ------------------------------------------------------------
// Waehrend des Updates laeuft loop() nicht weiter - deshalb zeichnet onProgress selbst.
void updateBild(int prozent, const char *text) {
  uint32_t t = millis();
  zeichneHimmel(bild, KLAR, false, t, false);
  bild.setFreeFont(&FreeSansBold9pt7b);
  schreibe(bild, "Update über WLAN", 120, 48, WEISS, MITTE);

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
    updateBild(0, "nicht ausschalten");
  });
  ArduinoOTA.onProgress([](unsigned int fertig, unsigned int gesamt) {
    static int zuletzt = -1;
    int p = fertig * 100 / gesamt;
    if (p != zuletzt) {
      zuletzt = p;
      updateBild(p, "nicht ausschalten");
    }
  });
  ArduinoOTA.onEnd([]() { updateBild(100, "fertig - starte neu"); });
  ArduinoOTA.onError([](ota_error_t fehler) {
    Serial.printf("Update-Fehler %u\n", fehler);
    updateBild(0, "Fehler - nochmal versuchen");
    delay(2000);
    ledcWrite(LICHT_PIN, HELLIGKEIT[lichtStufe]);
  });
  ArduinoOTA.begin();
  Serial.printf("Update ueber WLAN bereit: wetterstation.local (%s)\n", WiFi.localIP().toString().c_str());
}

void naechsteSeite(uint32_t t) {
  alteSeite = seite;
  seite = (seite + 1) % SEITEN;
  wechsel = true;
  wechselStart = t;
  seiteSeit = t;
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

  tft.init();
  tft.setRotation(1);  // quer
  tft.fillScreen(TFT_BLACK);

  ledcAttach(LICHT_PIN, 5000, 8);
  ledcWrite(LICHT_PIN, HELLIGKEIT[lichtStufe]);

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
  wetterStart();
}

void loop() {
  static bool seiteVorher = false;
  static uint32_t lichtGedruecktSeit = 0;
  uint32_t t = millis();

  ArduinoOTA.handle();

  bool hatDaten = wetterHolen(w);

  if (gedrueckt(TASTER_SEITE, seiteVorher) && hatDaten && !wechsel) {
    naechsteSeite(t);
    letzterTastendruck = t;
  }

  // GPIO 35: kurz = Helligkeit (beim Loslassen), 3 s halten = WLAN neu einrichten
  bool licht = digitalRead(TASTER_LICHT) == LOW;
  if (licht && !lichtGedruecktSeit) lichtGedruecktSeit = t ? t : 1;
  if (licht && t - lichtGedruecktSeit > 3000) {
    wlanBild(WLAN_EINRICHTUNG, "");
    wlanEinrichtungAnfordern();
  }
  if (!licht && lichtGedruecktSeit) {
    if (t - lichtGedruecktSeit < 1000) {
      lichtStufe = (lichtStufe + 1) % 4;
      ledcWrite(LICHT_PIN, HELLIGKEIT[lichtStufe]);
      lichtAnzeigeBis = t + 1500;
      letzterTastendruck = t;
    }
    lichtGedruecktSeit = 0;
  }

  if (t - akkuZeit > 2000) {
    akkuVolt = akkuSpannung();
    akkuZeit = t;
  }

  if (!hatDaten) {
    startBild(t);
  } else {
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

    if (!wechsel && t - seiteSeit > AUTO_WEITER && t - letzterTastendruck > 30000) naechsteSeite(t);
  }
  lichtAnzeige(t);

  bild.pushSprite(0, 0);
}
