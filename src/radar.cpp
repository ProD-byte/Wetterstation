#include "radar.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PNGdec.h>

// Karte: Zoomstufe 8/7/6 (256er-Kacheln) je nach Einstellung - Ausschnitt ca. 120 x 55, 240 x 110 oder 480 x 220 km.
// Radar: eine Stufe kleiner mit 512er-Kachel auf den Ort zentriert - das ist derselbe Massstab,
// die Bildmitte liegt genau auf dem Ort.
static int karteZoom(int stufe) { return 8 - constrain(stufe, 0, 2); }
// OpenStreetMap-Kacheln (eigene Kennung, nur 4 Kacheln je Ortswechsel - passt zur Nutzungsrichtlinie).
// Die Station macht daraus eine dunkle Karte (invertierte Graustufen), damit das Radar leuchtet.
static const char *KARTE_URL = "https://tile.openstreetmap.org/%d/%d/%d.png";
static const char *AGENT = "Wetterstation (github.com/ProD-byte/Wetterstation)";

struct Satz {
  uint16_t *karte = nullptr;
  uint8_t *bild[RADAR_BILDER] = {};
  time_t zeit[RADAR_BILDER] = {};
  int bilder = 0;
  int regen = 0;  // Pixel mit Regen im Ausschnitt (alle Bilder zusammen)
  bool karteDa = false;
  float lat = 0, lon = 0;
  int zoom = 0;
};

static Satz satz[2];
static volatile int aktiv = -1;  // welcher Satz gerade angezeigt wird (-1 = keiner)
static SemaphoreHandle_t sperre = xSemaphoreCreateMutex();
static PNG *png = nullptr;
static uint8_t *datei = nullptr;  // Puffer fuer eine heruntergeladene PNG-Datei
static const size_t DATEI_MAX = 300 * 1024;

static bool speicherHolen() {
  if (datei) return true;
  datei = (uint8_t *)ps_malloc(DATEI_MAX);
  png = new PNG();
  for (auto &s : satz) {
    s.karte = (uint16_t *)ps_malloc(RADAR_B * RADAR_H * 2);
    for (auto &b : s.bild) b = (uint8_t *)ps_malloc(RADAR_B * RADAR_H * 3);
    if (!s.karte || !s.bild[RADAR_BILDER - 1]) return false;
  }
  return datei && png;
}

// Datei ueber HTTPS in den Puffer laden, Laenge zurueck (0 = Fehler)
static size_t herunterladen(const String &url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent(AGENT);
  if (!http.begin(client, url)) return 0;
  size_t n = 0;
  if (http.GET() == 200) {
    WiFiClient *strom = http.getStreamPtr();
    int rest = http.getSize();  // -1 = unbekannt
    uint32_t start = millis();
    // weiterlesen, solange noch Daten kommen koennen - auch wenn der Server schon getrennt hat,
    // aber noch Empfangenes im Puffer liegt
    while ((rest > 0 || rest == -1) && n < DATEI_MAX && millis() - start < 15000) {
      int da = strom->available();
      if (!da) {
        if (!http.connected()) break;
        delay(2);
        continue;
      }
      int r = strom->readBytes(datei + n, min((size_t)da, DATEI_MAX - n));
      n += r;
      if (rest > 0) rest -= r;
    }
  } else {
    Serial.printf("Radar: %s -> Fehler\n", url.c_str());
  }
  http.end();
  return n;
}

// ---------- PNG entpacken: Zeile fuer Zeile in den Ausschnitt schreiben ----------

struct Ziel {
  int x0, y0;         // wo die Kachel im Ausschnitt liegt (kann negativ sein)
  uint16_t *karte;    // Kartenziel (RGB565) ...
  uint8_t *radar;     // ... oder Radarziel (RGB565 + Deckkraft)
  int regen;          // gezaehlte Regenpixel (bei Ziel{...} automatisch 0)
};

static int zeileKarte(PNGDRAW *d) {
  Ziel *z = (Ziel *)d->pUser;
  int y = z->y0 + d->y;
  if (y < 0 || y >= RADAR_H) return 1;
  uint16_t zeile[512];
  png->getLineAsRGB565(d, zeile, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  for (int x = 0; x < d->iWidth; x++) {
    int zx = z->x0 + x;
    if (zx < 0 || zx >= RADAR_B) continue;
    // Nachtkarte: Helligkeit umkehren und abdunkeln, leicht blaeulich - Schrift und Strassen werden hell
    uint16_t c = zeile[x];
    int r = (c >> 8) & 0xF8, g = (c >> 3) & 0xFC, b = (c << 3) & 0xF8;
    int l = (r * 77 + g * 150 + b * 29) >> 8;
    float n = (255 - l) * 0.62f + 14;
    int rn = n * 0.86f, gn = n * 0.93f, bn = min(255, (int)(n * 1.12f));
    z->karte[y * RADAR_B + zx] = ((rn & 0xF8) << 8) | ((gn & 0xFC) << 3) | (bn >> 3);
  }
  return 1;
}

static int zeileRadar(PNGDRAW *d) {
  Ziel *z = (Ziel *)d->pUser;
  int y = z->y0 + d->y;
  if (y < 0 || y >= RADAR_H) return 1;
  for (int x = 0; x < d->iWidth; x++) {
    int zx = z->x0 + x;
    if (zx < 0 || zx >= RADAR_B) continue;
    uint8_t r = 0, g = 0, b = 0, a = 0;
    if (d->iPixelType == PNG_PIXEL_TRUECOLOR_ALPHA && d->iBpp == 8) {
      const uint8_t *p = d->pPixels + x * 4;
      r = p[0], g = p[1], b = p[2], a = p[3];
    } else if (d->iPixelType == PNG_PIXEL_INDEXED && d->iBpp == 8) {
      uint8_t i = d->pPixels[x];
      r = d->pPalette[i * 3], g = d->pPalette[i * 3 + 1], b = d->pPalette[i * 3 + 2];
      a = d->iHasAlpha ? d->pPalette[768 + i] : 255;
    }
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    uint8_t *ziel = z->radar + (y * RADAR_B + zx) * 3;
    ziel[0] = c & 0xFF, ziel[1] = c >> 8, ziel[2] = a;
    if (a > 8) z->regen++;
  }
  return 1;
}

static bool entpacken(size_t laenge, Ziel &z, PNG_DRAW_CALLBACK *zeile) {
  if (!laenge || png->openRAM(datei, laenge, zeile) != PNG_SUCCESS) return false;
  bool ok = png->decode(&z, 0) == PNG_SUCCESS;
  png->close();
  return ok;
}

// ---------- Karte ----------

static bool karteLaden(Satz &s, float lat, float lon, int zoom) {
  // Mitte des Ausschnitts als Weltpixel (Web-Mercator) bei dieser Zoomstufe
  double n = 256.0 * (1 << zoom);
  double la = lat * PI / 180;
  double X = (lon + 180.0) / 360.0 * n;
  double Y = (1 - log(tan(la) + 1 / cos(la)) / PI) / 2 * n;
  int x0 = lround(X) - RADAR_B / 2, y0 = lround(Y) - RADAR_H / 2;
  memset(s.karte, 0, RADAR_B * RADAR_H * 2);
  for (int ty = y0 / 256; ty <= (y0 + RADAR_H - 1) / 256; ty++) {
    for (int tx = x0 / 256; tx <= (x0 + RADAR_B - 1) / 256; tx++) {
      char url[96];
      snprintf(url, sizeof(url), KARTE_URL, zoom, tx, ty);
      Ziel z{tx * 256 - x0, ty * 256 - y0, s.karte, nullptr};
      if (!entpacken(herunterladen(url), z, zeileKarte)) return false;
    }
  }
  s.lat = lat, s.lon = lon, s.zoom = zoom, s.karteDa = true;
  return true;
}

// ---------- Radarbilder ----------

static int radarLaden(Satz &s, float lat, float lon, int zoom) {
  JsonDocument doc;
  {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(8000);
    if (!http.begin(client, "https://api.rainviewer.com/public/weather-maps.json")) return 0;
    bool ok = http.GET() == 200 && !deserializeJson(doc, http.getString());
    http.end();
    if (!ok) return 0;
  }
  String host = doc["host"] | "https://tilecache.rainviewer.com";
  JsonArray vergangen = doc["radar"]["past"];
  int gesamt = vergangen.size();
  int anfang = max(0, gesamt - RADAR_BILDER);
  int n = 0;
  s.regen = 0;
  for (int i = anfang; i < gesamt; i++) {
    const char *pfad = vergangen[i]["path"];
    if (!pfad) continue;
    // auf den Ort zentrierte 512er-Kachel; Farbschema 2, geglaettet, Schnee eigene Farbe
    String url = host + pfad + "/512/" + String(zoom - 1) + "/" + String(lat, 4) + "/" + String(lon, 4) + "/2/1_1.png";
    memset(s.bild[n], 0, RADAR_B * RADAR_H * 3);
    Ziel z{RADAR_B / 2 - 256, RADAR_H / 2 - 256, nullptr, s.bild[n]};
    if (!entpacken(herunterladen(url), z, zeileRadar)) continue;
    s.zeit[n] = vergangen[i]["time"] | 0;
    s.regen += z.regen;
    n++;
  }
  return n;
}

void radarAktualisieren(float lat, float lon, int stufe) {
  if (!psramFound() || !speicherHolen()) return;
  int zoom = karteZoom(stufe);
  int neu = aktiv == 0 ? 1 : 0;
  Satz &s = satz[neu];
  const Satz *alt = aktiv >= 0 ? &satz[aktiv] : nullptr;

  // Karte nur neu laden, wenn sich Ort oder Zoom geaendert haben
  if (alt && alt->karteDa && alt->zoom == zoom && fabsf(alt->lat - lat) < 0.001f && fabsf(alt->lon - lon) < 0.001f) {
    memcpy(s.karte, alt->karte, RADAR_B * RADAR_H * 2);
    s.lat = lat, s.lon = lon, s.zoom = zoom, s.karteDa = true;
  } else {
    s.karteDa = false;
    if (!karteLaden(s, lat, lon, zoom)) Serial.println("Radar: Karte nicht geladen");
  }
  s.bilder = radarLaden(s, lat, lon, zoom);
  Serial.printf("Radar: %d Bilder, %d Regenpixel, Karte %s\n", s.bilder, s.regen, s.karteDa ? "ok" : "fehlt");
  if (!s.bilder && !s.karteDa) return;

  xSemaphoreTake(sperre, portMAX_DELAY);
  aktiv = neu;
  xSemaphoreGive(sperre);
}

bool radarHolen(RadarStand &r) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  int a = aktiv;
  xSemaphoreGive(sperre);
  if (a < 0) return false;
  const Satz &s = satz[a];
  r.karte = s.karteDa;
  r.bilder = s.bilder;
  r.regen = s.regen;
  r.kartePixel = s.karte;
  for (int i = 0; i < RADAR_BILDER; i++) {
    r.bild[i] = s.bild[i];
    r.zeit[i] = s.zeit[i];
  }
  return true;
}
