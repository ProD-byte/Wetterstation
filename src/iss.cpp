#include "iss.h"
#include "einstellungen.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Sgp4.h>

static SemaphoreHandle_t sperre = xSemaphoreCreateMutex();
static IssUeberflug naechster;  // fuer die Anzeige (unter sperre)
static Sgp4 sat;
static char tle[3][80];
static bool tleDa = false;
static uint32_t tleZeit = 0, tleVersuch = 0;
static time_t gerechnetBis = 0;  // Ueberflug fuer diesen Zeitraum ist berechnet
static float ortLat = NAN, ortLon = NAN;

const uint32_t TLE_ABSTAND = 24UL * 3600 * 1000;  // CelesTrak bittet: nicht oefter als noetig
const uint32_t TLE_FEHLER = 60UL * 60 * 1000;

const char *issRichtung(float az) {
  static const char *DE[] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};
  static const char *EN[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int i = (int)lroundf(fmodf(az + 360, 360) / 45) % 8;
  return TX(DE[i], EN[i]);
}

// Bahndaten der ISS (NORAD 25544): drei Zeilen Text
static bool tleHolen() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("Wetterstation github.com/ProD-byte/Wetterstation");
  if (!http.begin(client, "https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE")) return false;
  int code = http.GET();
  String text = code == 200 ? http.getString() : "";
  http.end();
  int a = text.indexOf('\n'), b = text.indexOf('\n', a + 1);
  if (a < 0 || b < 0) {
    Serial.printf("ISS-Bahndaten: HTTP %d\n", code);
    return false;
  }
  String z1 = text.substring(a + 1, b), z2 = text.substring(b + 1);
  z1.trim();
  z2.trim();
  if (!z1.startsWith("1 25544") || !z2.startsWith("2 25544")) return false;
  strlcpy(tle[0], "ISS", sizeof(tle[0]));
  strlcpy(tle[1], z1.c_str(), sizeof(tle[1]));
  strlcpy(tle[2], z2.c_str(), sizeof(tle[2]));
  return true;
}

// Ueberfluege ab 'ab' durchgehen, bis einer sichtbar ist (hoechstens 2 Tage voraus)
static void berechnen(time_t ab) {
  char z1[130], z2[130];
  strlcpy(z1, tle[1], sizeof(z1));
  strlcpy(z2, tle[2], sizeof(z2));
  sat.site(ortLat, ortLon, 0);
  sat.init(tle[0], z1, z2);
  uint32_t t0 = millis();
  IssUeberflug u;
  // etwas vor 'ab' anfangen, damit ein gerade laufender Ueberflug gefunden wird
  sat.initpredpoint((unsigned long)(ab - 20 * 60), 0.0);
  passinfo p;
  for (int n = 0; n < 40 && sat.nextpass(&p, 20); n++) {
    time_t start = (time_t)((p.jdstart - 2440587.5) * 86400), ende = (time_t)((p.jdstop - 2440587.5) * 86400);
    if (start > ab + 2 * 86400) break;
    if (ende < ab || ende - start > 20 * 60) continue;
    // Bahn abtasten: wo steht sie, und wann ist sie zu sehen?
    u = IssUeberflug();
    u.start = start;
    u.ende = ende;
    float azAnfang = 0, azEnde = 0;
    for (time_t t = start; t <= ende && u.punkte < 120; t += ISS_SCHRITT) {
      sat.findsat((unsigned long)t);
      IssPunkt &q = u.bahn[u.punkte++];
      q.az10 = lroundf(sat.satAz * 10);
      q.el10 = lroundf(max(0.0, sat.satEl) * 10);
      q.sichtbar = sat.satVis >= 1000 && sat.satEl > 0;
      if (!q.sichtbar) continue;
      if (!u.sichtAb) u.sichtAb = t, azAnfang = sat.satAz;
      u.sichtBis = t, azEnde = sat.satAz;
      u.maxHoehe = max(u.maxHoehe, (float)sat.satEl);
    }
    // kaum ueber dem Horizont oder nur ein paar Sekunden: lohnt nicht zum Rausgehen
    if (!u.sichtAb || u.maxHoehe < 10 || u.sichtBis - u.sichtAb < 60) continue;
    u.azAb = azAnfang;
    u.azBis = azEnde;
    float d = fmodf(azEnde - azAnfang + 540, 360) - 180;  // kuerzester Weg von Anfang zu Ende
    u.azMitte = fmodf(azAnfang + d / 2 + 360, 360);
    u.gueltig = true;
    break;
  }
  if (!u.gueltig) u = IssUeberflug();
  Serial.printf("ISS: %s in %lu ms\n", u.gueltig ? "Ueberflug berechnet" : "kein sichtbarer Ueberflug", millis() - t0);
  if (u.gueltig) {
    struct tm z;
    localtime_r(&u.sichtAb, &z);
    Serial.printf("ISS: %02d.%02d. %02d:%02d, %d s sichtbar, max %.0f Grad, %s -> %s\n", z.tm_mday, z.tm_mon + 1,
                  z.tm_hour, z.tm_min, (int)(u.sichtBis - u.sichtAb), u.maxHoehe, issRichtung(u.azAb), issRichtung(u.azBis));
  }
  xSemaphoreTake(sperre, portMAX_DELAY);
  naechster = u;
  xSemaphoreGive(sperre);
  // neu rechnen, wenn dieser vorbei ist - sonst in 6 Stunden (neue Bahndaten, Uhr erst spaet gestellt ...)
  gerechnetBis = u.gueltig ? u.ende : ab + 6 * 3600;
}

void issAktualisieren(float lat, float lon) {
  time_t jetzt = time(nullptr);
  if (jetzt < 1700000000) return;  // Uhr noch nicht gestellt
  bool neuerOrt = lat != ortLat || lon != ortLon;
  ortLat = lat, ortLon = lon;
  // Bahndaten taeglich erneuern; klappt es nicht, reichen die alten noch ein paar Tage
  uint32_t ms = millis();
  bool veraltet = !tleDa || ms - tleZeit > TLE_ABSTAND;
  if (veraltet && (!tleVersuch || ms - tleVersuch > (tleDa ? TLE_FEHLER : 5 * 60 * 1000UL))) {
    tleVersuch = ms;
    if (tleHolen()) {
      tleDa = true;
      tleZeit = ms;
      gerechnetBis = 0;
    }
  }
  if (!tleDa) return;
  if (neuerOrt || jetzt >= gerechnetBis) berechnen(jetzt);
}

bool issHolen(IssUeberflug &u) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  u = naechster;
  xSemaphoreGive(sperre);
  return u.gueltig && time(nullptr) <= u.ende;
}
