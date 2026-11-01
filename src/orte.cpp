#include "orte.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static OrtWetter orte[ORTE_MAX];
static int anzahl = 0;
static SemaphoreHandle_t sperre = xSemaphoreCreateMutex();

struct Gespeichert {
  char name[40];
  float lat, lon;
};

static void speichern() {
  Gespeichert g[ORTE_MAX] = {};
  for (int i = 0; i < anzahl; i++) {
    strlcpy(g[i].name, orte[i].name, sizeof(g[i].name));
    g[i].lat = orte[i].lat, g[i].lon = orte[i].lon;
  }
  Preferences p;
  p.begin("orte", false);
  p.putInt("n", anzahl);
  p.putBytes("liste", g, sizeof(g));
  p.end();
}

void orteLaden() {
  Preferences p;
  p.begin("orte", true);
  Gespeichert g[ORTE_MAX] = {};
  int n = min((int)p.getInt("n", 0), ORTE_MAX);
  if (n) p.getBytes("liste", g, sizeof(g));
  p.end();
  xSemaphoreTake(sperre, portMAX_DELAY);
  anzahl = n;
  for (int i = 0; i < n; i++) {
    orte[i] = OrtWetter();
    strlcpy(orte[i].name, g[i].name, sizeof(orte[i].name));
    orte[i].lat = g[i].lat, orte[i].lon = g[i].lon;
  }
  xSemaphoreGive(sperre);
}

int orteAnzahl() { return anzahl; }

int orteHolen(OrtWetter *liste) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  int n = anzahl;
  for (int i = 0; i < n; i++) liste[i] = orte[i];
  xSemaphoreGive(sperre);
  return n;
}

bool orteHinzu(const char *name, float lat, float lon) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  bool ok = anzahl < ORTE_MAX;
  if (ok) {
    orte[anzahl] = OrtWetter();
    strlcpy(orte[anzahl].name, name, sizeof(orte[anzahl].name));
    orte[anzahl].lat = lat, orte[anzahl].lon = lon;
    anzahl++;
    speichern();
  }
  xSemaphoreGive(sperre);
  return ok;
}

void orteEntfernen(int i) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  if (i >= 0 && i < anzahl) {
    for (int k = i; k < anzahl - 1; k++) orte[k] = orte[k + 1];
    anzahl--;
    speichern();
  }
  xSemaphoreGive(sperre);
}

// Open-Meteo kann mehrere Orte in einer Abfrage: latitude=a,b,c&longitude=x,y,z -> Liste
void orteAktualisieren() {
  OrtWetter liste[ORTE_MAX];
  int n = orteHolen(liste);
  if (!n) return;
  String la, lo;
  for (int i = 0; i < n; i++) {
    la += (i ? "," : "") + String(liste[i].lat, 4);
    lo += (i ? "," : "") + String(liste[i].lon, 4);
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + la + "&longitude=" + lo +
               "&current=temperature_2m,weather_code,is_day&daily=temperature_2m_max,temperature_2m_min"
               "&forecast_days=1&timezone=auto";
  if (!http.begin(client, url)) return;
  JsonDocument doc;
  bool ok = http.GET() == 200 && !deserializeJson(doc, http.getString());
  http.end();
  if (!ok) return;

  xSemaphoreTake(sperre, portMAX_DELAY);
  for (int i = 0; i < n && i < anzahl; i++) {
    JsonVariant e = doc.is<JsonArray>() ? doc[i].as<JsonVariant>() : doc.as<JsonVariant>();  // ein Ort -> kein Array
    if (e.isNull()) continue;
    orte[i].temp = e["current"]["temperature_2m"] | 0.0f;
    orte[i].code = e["current"]["weather_code"] | 0;
    orte[i].tag = (e["current"]["is_day"] | 1) == 1;
    orte[i].max = e["daily"]["temperature_2m_max"][0] | 0.0f;
    orte[i].min = e["daily"]["temperature_2m_min"][0] | 0.0f;
    orte[i].gueltig = true;
  }
  xSemaphoreGive(sperre);
}

String orteJson() {
  OrtWetter liste[ORTE_MAX];
  int n = orteHolen(liste);
  String j = "[";
  for (int i = 0; i < n; i++) {
    String name = liste[i].name;
    name.replace("\"", "'");
    j += String(i ? "," : "") + "{\"name\":\"" + name + "\",\"temp\":" + (liste[i].gueltig ? String(liste[i].temp, 1) : "null") + "}";
  }
  return j + "]";
}
