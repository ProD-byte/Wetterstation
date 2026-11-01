#include "wetter.h"
#include "ort.h"
#include "einstellungen.h"
#include "netzupdate.h"
#include "radar.h"
#include "verlauf.h"
#include "orte.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Ort kommt aus ort.cpp (am Handy gewaehlt, sonst ueber die Internet-Adresse, sonst Cottbus)
static const char *URL_REST =
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
    "&minutely_15=precipitation&forecast_minutely_15=8"
    "&hourly=temperature_2m,precipitation_probability"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset,uv_index_max"
    "&timezone=auto&forecast_days=4&forecast_hours=24";

const char *pollenName(int i) {
  static const char *DE[5] = {"Erle", "Birke", "Gräser", "Beifuß", "Ambrosia"};
  static const char *EN[5] = {"Alder", "Birch", "Grass", "Mugwort", "Ragweed"};
  return TX(DE[i], EN[i]);
}

const uint32_t ABSTAND_OK     = 15 * 60 * 1000;  // alle 15 min neu
const uint32_t ABSTAND_FEHLER = 60 * 1000;       // nach Fehler in 1 min nochmal
const uint32_t ABSTAND_LUFT   = 60 * 60 * 1000;  // Luft & Pollen stuendlich
const uint32_t ABSTAND_UPDATE = 6 * 60 * 60 * 1000;
const uint32_t ABSTAND_RADAR  = 10 * 60 * 1000;
const uint32_t ABSTAND_VERLAUF = 60 * 60 * 1000;

static volatile bool orteNeu = false;
void wetterOrteNeu() { orteNeu = true; }
static volatile bool ortAuto = false;
void wetterOrtAutomatisch() { ortAuto = true; }

static Wetter aktuell;
static SemaphoreHandle_t sperre;
static volatile WetterStatus status = VERBINDE;
static volatile bool neuLaden = false;

// Wochentag aus Datum (Sakamoto), 0 = Sonntag
static int wochentag(int j, int m, int t) {
  static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) j--;
  return (j + j / 4 - j / 100 + j / 400 + k[m - 1] + t) % 7;
}

// HTTPS-Abruf und JSON lesen (Zertifikat wird nicht geprueft - reicht fuer Wetterdaten)
static bool holen(const String &url, JsonDocument &doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("Wetterstation");
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Abruf %s: HTTP %d\n", url.substring(8, 40).c_str(), code);
    http.end();
    return false;
  }
  String text = http.getString();
  http.end();
  if (deserializeJson(doc, text)) {
    Serial.println("Antwort nicht lesbar");
    return false;
  }
  return true;
}

static bool wetterAbrufen(Wetter &w, const Ort &ort) {
  JsonDocument doc;
  if (!holen("https://api.open-meteo.com/v1/forecast?latitude=" + String(ort.lat, 4) + "&longitude=" +
                 String(ort.lon, 4) + URL_REST,
             doc)) {
    return false;
  }

  JsonObject c = doc["current"];
  w.temp     = c["temperature_2m"] | 0.0f;
  w.gefuehlt = c["apparent_temperature"] | 0.0f;
  w.wind     = c["wind_speed_10m"] | 0.0f;
  w.feuchte  = c["relative_humidity_2m"] | 0;
  w.code     = c["weather_code"] | 0;
  w.tag      = (c["is_day"] | 1) == 1;

  // Regen in den naechsten 2 Stunden (Viertelstunden): Beginn oder Ende ankuendigen
  JsonArray zeit15 = doc["minutely_15"]["time"];
  JsonArray regen15 = doc["minutely_15"]["precipitation"];
  w.regenArt = 0;
  int n15 = min((int)regen15.size(), 8);
  if (n15 > 1) {
    bool jetztNass = (regen15[0] | 0.0f) >= 0.1f;
    for (int i = 1; i < n15; i++) {
      bool nass = (regen15[i] | 0.0f) >= 0.1f;
      const char *z = zeit15[i];
      if (nass != jetztNass && z) {
        w.regenArt = jetztNass ? 2 : 1;
        strlcpy(w.regenZeit, z + 11, sizeof(w.regenZeit));
        break;
      }
    }
  }

  JsonObject h = doc["hourly"];
  JsonArray zeiten = h["time"];
  w.stunden = min((int)zeiten.size(), 24);
  for (int i = 0; i < w.stunden; i++) {
    const char *z = zeiten[i];  // "2026-11-01T07:00"
    w.stdStunde[i] = z ? atoi(z + 11) : 0;
    w.stdTemp[i]   = h["temperature_2m"][i] | 0.0f;
    w.stdRegen[i]  = h["precipitation_probability"][i] | 0;
  }

  JsonObject d = doc["daily"];
  for (int i = 0; i < 4; i++) {
    const char *z = d["time"][i];  // "2026-11-01"
    w.tagWochentag[i] = z ? wochentag(atoi(z), atoi(z + 5), atoi(z + 8)) : 0;
    w.tagCode[i]  = d["weather_code"][i] | 0;
    w.tagMax[i]   = d["temperature_2m_max"][i] | 0.0f;
    w.tagMin[i]   = d["temperature_2m_min"][i] | 0.0f;
    w.tagRegen[i] = d["precipitation_probability_max"][i] | 0;
  }
  const char *auf = d["sunrise"][0], *unter = d["sunset"][0];  // "2026-11-01T06:56"
  if (auf) strlcpy(w.sonneAuf, auf + 11, sizeof(w.sonneAuf));
  if (unter) strlcpy(w.sonneUnter, unter + 11, sizeof(w.sonneUnter));
  w.uv = d["uv_index_max"][0] | 0.0f;
  w.utcAbweichung = doc["utc_offset_seconds"] | w.utcAbweichung;  // timezone=auto: Zeitzone des Orts

  w.gueltig = true;
  w.abruf = millis();
  Serial.printf("Wetter %s: %.1f Grad, Code %d\n", ort.name, w.temp, w.code);
  return true;
}

static bool luftAbrufen(Wetter &w, const Ort &ort) {
  JsonDocument doc;
  if (!holen("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(ort.lat, 4) +
                 "&longitude=" + String(ort.lon, 4) +
                 "&current=european_aqi,pm2_5,alder_pollen,birch_pollen,grass_pollen,mugwort_pollen,ragweed_pollen"
                 "&timezone=auto",
             doc)) {
    return false;
  }
  JsonObject c = doc["current"];
  w.aqi = c["european_aqi"] | 0;
  w.pm25 = c["pm2_5"] | 0.0f;
  const char *namen[5] = {"alder_pollen", "birch_pollen", "grass_pollen", "mugwort_pollen", "ragweed_pollen"};
  for (int i = 0; i < 5; i++) w.pollen[i] = c[namen[i]] | 0.0f;  // ausserhalb Europas: null -> 0
  w.luftGueltig = !c["european_aqi"].isNull();
  return true;
}

// Unwetterwarnungen des DWD (nur Deutschland, sonst einfach keine)
static void warnungAbrufen(Wetter &w, const Ort &ort) {
  JsonDocument filter;
  filter["alerts"][0]["event_de"] = true;
  filter["alerts"][0]["event_en"] = true;
  filter["alerts"][0]["severity"] = true;
  JsonDocument doc;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  String url = "https://api.brightsky.dev/alerts?lat=" + String(ort.lat, 4) + "&lon=" + String(ort.lon, 4);
  if (!http.begin(client, url)) return;
  if (http.GET() == 200 && !deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter))) {
    w.warnung[0] = 0;
    w.warnungEn[0] = 0;
    w.warnStufe = 0;
    for (JsonObject a : doc["alerts"].as<JsonArray>()) {
      String s = a["severity"] | "";
      int stufe = s == "extreme" ? 4 : s == "severe" ? 3 : s == "moderate" ? 2 : 1;
      if (stufe > w.warnStufe) {
        w.warnStufe = stufe;
        strlcpy(w.warnung, a["event_de"] | "Warnung", sizeof(w.warnung));
        strlcpy(w.warnungEn, a["event_en"] | "Weather warning", sizeof(w.warnungEn));
      }
    }
  }
  http.end();
}

static void aufgabe(void *) {
  uint32_t naechster = 0, naechsteLuft = 0, naechstesUpdate = 0, naechstesRadar = 0, naechsterVerlauf = 0,
           naechsteOrte = 0;
  bool ortGeprueft = false;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (!aktuell.gueltig) status = VERBINDE;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    // Kein Ort am Handy gewaehlt -> einmal pro Start grob ueber die Internet-Adresse bestimmen
    if (!ortGeprueft) {
      ortGeprueft = true;
      Ort o = ortAktuell();
      if (!o.vomNutzer) {
        if (!aktuell.gueltig) status = ORTUNG;
        if (ortPerIp(o)) ortSpeichern(o);
      }
    }
    // Einstellungsseite: "Ort wieder automatisch bestimmen"
    if (ortAuto) {
      ortAuto = false;
      Ort o = ortAktuell();
      if (ortPerIp(o)) {
        ortSpeichern(o);
        neuLaden = true;
      }
    }
    if (neuLaden) {
      neuLaden = false;
      naechster = naechsteLuft = naechstesRadar = naechsterVerlauf = millis();
    }

    if ((int32_t)(millis() - naechster) >= 0) {
      if (!aktuell.gueltig) status = LADE;
      Ort ort = ortAktuell();
      Wetter neu;
      xSemaphoreTake(sperre, portMAX_DELAY);
      neu = aktuell;  // Luft & Warnung behalten, falls deren Abruf gleich scheitert
      xSemaphoreGive(sperre);
      strlcpy(neu.ort, ort.name, sizeof(neu.ort));
      if (wetterAbrufen(neu, ort)) {
        warnungAbrufen(neu, ort);
        if ((int32_t)(millis() - naechsteLuft) >= 0 && luftAbrufen(neu, ort)) naechsteLuft = millis() + ABSTAND_LUFT;
        xSemaphoreTake(sperre, portMAX_DELAY);
        aktuell = neu;
        xSemaphoreGive(sperre);
        status = FERTIG;
        naechster = millis() + ABSTAND_OK;
      } else {
        if (!aktuell.gueltig) status = FEHLER;
        naechster = millis() + ABSTAND_FEHLER;
      }
    }

    // Verlauf der letzten 7 Tage stuendlich, weitere Orte zusammen mit dem Wetter
    if (aktuell.gueltig && (int32_t)(millis() - naechsterVerlauf) >= 0) {
      Ort ort = ortAktuell();
      verlaufAktualisieren(ort.lat, ort.lon);
      naechsterVerlauf = millis() + ABSTAND_VERLAUF;
    }
    if (orteNeu || (aktuell.gueltig && (int32_t)(millis() - naechsteOrte) >= 0)) {
      orteNeu = false;
      orteAktualisieren();
      naechsteOrte = millis() + ABSTAND_OK;
    }

    // Regenradar alle 10 Minuten (RainViewer erneuert seine Bilder in diesem Takt) - und sofort, wenn der Zoom geaendert wurde
    static int radarZoom = einst.radarZoom;
    if (radarZoom != einst.radarZoom) radarZoom = einst.radarZoom, naechstesRadar = millis();
    if (aktuell.gueltig && (int32_t)(millis() - naechstesRadar) >= 0) {
      Ort ort = ortAktuell();
      radarAktualisieren(ort.lat, ort.lon, radarZoom);
      naechstesRadar = millis() + ABSTAND_RADAR;
    }

    // Neue Firmware auf GitHub? (erst wenn das Wetter laeuft, dann alle 6 Stunden oder auf Wunsch)
    if (aktuell.gueltig && ((int32_t)(millis() - naechstesUpdate) >= 0 || updateSucheGewuenscht())) {
      updatePruefen();
      naechstesUpdate = millis() + ABSTAND_UPDATE;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void wetterStart() {
  sperre = xSemaphoreCreateMutex();
  // Kern 0, damit Kern 1 ungestoert zeichnen kann
  xTaskCreatePinnedToCore(aufgabe, "wetter", 20480, nullptr, 1, nullptr, 0);
}

bool wetterHolen(Wetter &w) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  w = aktuell;
  xSemaphoreGive(sperre);
  return w.gueltig;
}

WetterStatus wetterStatus() { return status; }

void wetterNeuLaden() { neuLaden = true; }
