#include "wetter.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// 03046 Cottbus
static const char *URL =
    "https://api.open-meteo.com/v1/forecast?latitude=51.7563&longitude=14.3329"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
    "&hourly=temperature_2m,precipitation_probability"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&timezone=Europe%2FBerlin&forecast_days=4&forecast_hours=24";

const uint32_t ABSTAND_OK     = 15 * 60 * 1000;  // alle 15 min neu
const uint32_t ABSTAND_FEHLER = 60 * 1000;       // nach Fehler in 1 min nochmal

static Wetter aktuell;
static SemaphoreHandle_t sperre;
static volatile WetterStatus status = VERBINDE;

// Wochentag aus Datum (Sakamoto), 0 = Sonntag
static int wochentag(int j, int m, int t) {
  static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) j--;
  return (j + j / 4 - j / 100 + j / 400 + k[m - 1] + t) % 7;
}

static bool abrufen(Wetter &w) {
  WiFiClientSecure client;
  client.setInsecure();  // Zertifikat nicht pruefen - reicht fuer Wetterdaten
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, URL)) return false;

  int code = http.GET();
  if (code != 200) {
    Serial.printf("Wetter: HTTP %d\n", code);
    http.end();
    return false;
  }
  String text = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, text)) {
    Serial.println("Wetter: Antwort nicht lesbar");
    return false;
  }

  JsonObject c = doc["current"];
  w.temp     = c["temperature_2m"] | 0.0f;
  w.gefuehlt = c["apparent_temperature"] | 0.0f;
  w.wind     = c["wind_speed_10m"] | 0.0f;
  w.feuchte  = c["relative_humidity_2m"] | 0;
  w.code     = c["weather_code"] | 0;
  w.tag      = (c["is_day"] | 1) == 1;

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

  w.gueltig = true;
  w.abruf = millis();
  Serial.printf("Wetter: %.1f Grad, Code %d, %d Stunden\n", w.temp, w.code, w.stunden);
  return true;
}

static void aufgabe(void *) {
  uint32_t naechster = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (!aktuell.gueltig) status = VERBINDE;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    if ((int32_t)(millis() - naechster) >= 0) {
      if (!aktuell.gueltig) status = LADE;
      Wetter neu;
      if (abrufen(neu)) {
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
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void wetterStart() {
  sperre = xSemaphoreCreateMutex();
  // Kern 0, damit Kern 1 ungestoert zeichnen kann
  xTaskCreatePinnedToCore(aufgabe, "wetter", 16384, nullptr, 1, nullptr, 0);
}

bool wetterHolen(Wetter &w) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  w = aktuell;
  xSemaphoreGive(sperre);
  return w.gueltig;
}

WetterStatus wetterStatus() { return status; }
