#include "verlauf.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static Verlauf verlauf;
static SemaphoreHandle_t sperre = xSemaphoreCreateMutex();

static int wochentag(int j, int m, int t) {
  static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) j--;
  return (j + j / 4 - j / 100 + j / 400 + k[m - 1] + t) % 7;
}

void verlaufAktualisieren(float lat, float lon) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) + "&longitude=" + String(lon, 4) +
               "&current=temperature_2m&hourly=temperature_2m,pressure_msl,precipitation"
               "&past_days=7&forecast_days=1&timezone=auto";
  if (!http.begin(client, url)) return;
  JsonDocument doc;
  bool ok = http.GET() == 200 && !deserializeJson(doc, http.getString());
  http.end();
  if (!ok) {
    Serial.println("Verlauf: Abruf fehlgeschlagen");
    return;
  }

  // nur die Stunden bis jetzt (die Antwort reicht bis heute 23 Uhr)
  String jetzt = doc["current"]["time"] | "";  // "2026-11-01T12:15"
  jetzt = jetzt.substring(0, 13);              // "2026-11-01T12"
  JsonObject h = doc["hourly"];
  JsonArray zeiten = h["time"];
  int ende = 0;
  for (int i = 0; i < (int)zeiten.size(); i++) {
    const char *z = zeiten[i];
    if (z && strncmp(z, jetzt.c_str(), 13) <= 0) ende = i + 1;
  }
  int anfang = max(0, ende - VERLAUF_MAX);

  Verlauf v;
  for (int i = anfang; i < ende; i++) {
    v.temp[v.n] = h["temperature_2m"][i] | NAN;
    v.druck[v.n] = h["pressure_msl"][i] | NAN;
    v.regen[v.n] = h["precipitation"][i] | 0.0f;
    v.n++;
  }
  const char *z0 = zeiten[anfang];
  if (z0) {
    v.ersteStunde = atoi(z0 + 11);
    v.ersterWochentag = wochentag(atoi(z0), atoi(z0 + 5), atoi(z0 + 8));
  }
  xSemaphoreTake(sperre, portMAX_DELAY);
  verlauf = v;
  xSemaphoreGive(sperre);
  Serial.printf("Verlauf: %d Stunden\n", v.n);
}

bool verlaufHolen(Verlauf &v) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  v = verlauf;
  xSemaphoreGive(sperre);
  return v.n > 1;
}

String verlaufJson() {
  Verlauf v;
  verlaufHolen(v);
  String j = "{\"stunde\":" + String(v.ersteStunde) + ",\"wochentag\":" + v.ersterWochentag + ",\"temp\":[";
  for (int i = 0; i < v.n; i++) j += (i ? "," : "") + (isnan(v.temp[i]) ? String("null") : String(v.temp[i], 1));
  j += "],\"druck\":[";
  for (int i = 0; i < v.n; i++) j += (i ? "," : "") + (isnan(v.druck[i]) ? String("null") : String(v.druck[i], 1));
  j += "],\"regen\":[";
  for (int i = 0; i < v.n; i++) j += (i ? "," : "") + String(v.regen[i], 1);
  return j + "]}";
}
