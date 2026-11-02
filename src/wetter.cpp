#include "wetter.h"
#include "ort.h"
#include "einstellungen.h"
#include "netzupdate.h"
#include "radar.h"
#include "verlauf.h"
#include "orte.h"
#include "ersatz.h"
#include "iss.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_attr.h>

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
// nach Fehlern immer laengere Pausen (1, 2, 5, 10 min) plus ein paar zufaellige Sekunden - wenn der
// Wetterdienst ueberlastet ist, sollen nicht alle Stationen gleichzeitig jede Minute anklopfen
const uint32_t ABSTAND_FEHLER[] = {60 * 1000, 2 * 60 * 1000, 5 * 60 * 1000, 10 * 60 * 1000};
const uint32_t ABSTAND_ERSATZ = 5 * 60 * 1000;   // laeuft der Ersatzdienst: alle 5 min schauen, ob Open-Meteo wieder geht
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
static volatile WetterFehler fehler = KEIN_FEHLER;
static volatile uint32_t naechster = 0;  // millis() des naechsten Wetter-Abrufs
static volatile bool versuchLaeuft = false;

// Letzte Wetterdaten ueberstehen einen Neustart (Update, Absturz) im RTC-Speicher - nicht aber Strom weg.
// Kennung haengt an der Groesse, damit eine Firmware mit anderem Aufbau nichts Falsches liest.
RTC_NOINIT_ATTR static uint8_t rettung[sizeof(Wetter)];
RTC_NOINIT_ATTR static uint32_t rettungKennung, rettungPruefsumme;
static const uint32_t KENNUNG = 0x57455400 ^ sizeof(Wetter);

static uint32_t pruefsumme() {
  uint32_t h = 2166136261u;
  for (uint8_t b : rettung) h = (h ^ b) * 16777619u;
  return h;
}

static void retten(const Wetter &w) {
  memcpy(rettung, &w, sizeof(Wetter));
  rettungPruefsumme = pruefsumme();
  rettungKennung = KENNUNG;
}

static bool zeitBekannt() { return time(nullptr) > 1700000000; }  // per NTP gestellt

// Daten noch frisch genug zum Anzeigen? (Ohne Uhrzeit laesst sich das Alter nicht pruefen)
static bool frisch(const Wetter &w) {
  return w.gueltig && (!w.stand || !zeitBekannt() || time(nullptr) - w.stand <= DATEN_MAX_ALTER);
}

// Wochentag aus Datum (Sakamoto), 0 = Sonntag
static int wochentag(int j, int m, int t) {
  static const int k[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) j--;
  return (j + j / 4 - j / 100 + j / 400 + k[m - 1] + t) % 7;
}

// HTTPS-Abruf und JSON lesen (Zertifikat wird nicht geprueft - reicht fuer Wetterdaten)
// Ergebnis: 200 = ok, sonst HTTP-Code, negativ = keine Verbindung, 0 = Antwort nicht lesbar
static int holen(const String &url, JsonDocument &doc) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  http.setUserAgent("Wetterstation");
  if (!http.begin(client, url)) return -1;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Abruf %s: HTTP %d\n", url.substring(8, 40).c_str(), code);
    http.end();
    return code;
  }
  String text = http.getString();
  http.end();
  if (deserializeJson(doc, text)) {
    Serial.println("Antwort nicht lesbar");
    return 0;
  }
  return 200;
}

// 200 = ok, sonst Fehlercode wie bei holen()
static int wetterAbrufen(Wetter &w, const Ort &ort) {
  JsonDocument doc;
  int code = holen("https://api.open-meteo.com/v1/forecast?latitude=" + String(ort.lat, 4) + "&longitude=" +
                       String(ort.lon, 4) + URL_REST,
                   doc);
  if (code != 200) return code;

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
  w.stand = zeitBekannt() ? time(nullptr) : 0;
  Serial.printf("Wetter %s: %.1f Grad, Code %d\n", ort.name, w.temp, w.code);
  return 200;
}

static bool luftAbrufen(Wetter &w, const Ort &ort) {
  JsonDocument doc;
  if (200 != holen("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(ort.lat, 4) +
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
  uint32_t naechsteLuft = 0, naechstesUpdate = 0, naechstesRadar = 0, naechsterVerlauf = 0,
           naechsteOrte = 0;
  bool ortGeprueft = false;
  int fehlversuche = 0;
  for (;;) {
    // Gerettete Daten vom letzten Lauf: erst wenn die Uhr stimmt, denn nur dann ist ihr Alter bekannt
    if (rettungKennung == KENNUNG && zeitBekannt() && !aktuell.gueltig) {
      Wetter alt;
      memcpy(&alt, rettung, sizeof(Wetter));
      bool passt = pruefsumme() == rettungPruefsumme && alt.gueltig && alt.stand &&
                   !strcmp(alt.ort, ortAktuell().name) && frisch(alt);
      rettungKennung = 0;  // nur einmal pro Start
      if (passt) {
        Serial.printf("Wetter vom letzten Lauf uebernommen (%d min alt)\n", (int)((time(nullptr) - alt.stand) / 60));
        alt.abruf = millis();
        xSemaphoreTake(sperre, portMAX_DELAY);
        aktuell = alt;
        xSemaphoreGive(sperre);
        retten(alt);  // ueberlebt so auch den naechsten Neustart
        status = FERTIG;
      }
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (!frisch(aktuell)) status = VERBINDE;
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    // Kein Ort am Handy gewaehlt -> einmal pro Start grob ueber die Internet-Adresse bestimmen
    if (!ortGeprueft) {
      ortGeprueft = true;
      Ort o = ortAktuell();
      if (!o.vomNutzer) {
        if (!frisch(aktuell)) status = ORTUNG;
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
      if (!frisch(aktuell) && !fehlversuche) status = LADE;  // nach Fehlern bleibt der Hinweis stehen
      versuchLaeuft = true;
      Ort ort = ortAktuell();
      Wetter neu;
      xSemaphoreTake(sperre, portMAX_DELAY);
      neu = aktuell;  // Luft & Warnung behalten, falls deren Abruf gleich scheitert
      xSemaphoreGive(sperre);
      strlcpy(neu.ort, ort.name, sizeof(neu.ort));
      int code = wetterAbrufen(neu, ort);
      if (code == 200) {
        neu.quelle = OPEN_METEO;
      } else {
        // Open-Meteo hakt: Ersatzdienst. Deutsche Sprache -> Station steht wohl in Deutschland -> zuerst
        // Bright Sky (Deutscher Wetterdienst), sonst bzw. danach MET Norway (weltweit)
        const Quelle reihe[] = {BRIGHT_SKY, MET_NORWAY};
        for (int i = einst.sprache == 0 ? 0 : 1; i < 2; i++) {
          if (ersatzAbrufen(neu, ort, reihe[i]) != 200) continue;
          Serial.printf("Wetter %s ueber %s: %.1f Grad, Code %d\n", ort.name, quellenName(reihe[i]), neu.temp, neu.code);
          neu.quelle = reihe[i];
          neu.gueltig = true;
          neu.abruf = millis();
          neu.stand = time(nullptr);
          code = 200;
          break;
        }
      }
      if (code == 200) {
        warnungAbrufen(neu, ort);
        if ((int32_t)(millis() - naechsteLuft) >= 0 && luftAbrufen(neu, ort)) naechsteLuft = millis() + ABSTAND_LUFT;
        xSemaphoreTake(sperre, portMAX_DELAY);
        aktuell = neu;
        xSemaphoreGive(sperre);
        retten(neu);
        status = FERTIG;
        fehler = KEIN_FEHLER;
        fehlversuche = 0;
        naechster = millis() + (neu.quelle == OPEN_METEO ? ABSTAND_OK : ABSTAND_ERSATZ);
      } else {
        IPAddress ip;
        if (code == 429 || code == 502 || code == 503 || code == 504) fehler = UEBERLASTET;
        else if (code < 0 && !WiFi.hostByName("api.open-meteo.com", ip)) fehler = KEIN_INTERNET;
        else fehler = NICHT_ERREICHBAR;
        if (!frisch(aktuell)) status = FEHLER;
        naechster = millis() + ABSTAND_FEHLER[min(fehlversuche, 3)] + esp_random() % 15000;
        fehlversuche++;
      }
      versuchLaeuft = false;
    }
    // Daten zu alt geworden, waehrend der Wetterdienst weg ist -> Hinweis statt alter Werte
    if (aktuell.gueltig && !frisch(aktuell) && status == FERTIG) status = fehlversuche ? FEHLER : LADE;

    // Verlauf der letzten 7 Tage stuendlich, weitere Orte zusammen mit dem Wetter
    if (aktuell.gueltig && (int32_t)(millis() - naechsterVerlauf) >= 0) {
      Ort ort = ortAktuell();
      verlaufAktualisieren(ort.lat, ort.lon);
      naechsterVerlauf = millis() + ABSTAND_VERLAUF;
    }
    // ISS-Ueberfluege (Bahndaten taeglich, gerechnet wird nur bei Bedarf)
    if (aktuell.gueltig) {
      Ort ort = ortAktuell();
      issAktualisieren(ort.lat, ort.lon);
    }
    if (orteNeu || (aktuell.gueltig && (int32_t)(millis() - naechsteOrte) >= 0)) {
      orteNeu = false;
      orteAktualisieren();
      naechsteOrte = millis() + ABSTAND_OK;
    }

    // Regenradar alle 10 Minuten (RainViewer erneuert seine Bilder in diesem Takt) - und sofort, wenn der Zoom geaendert wurde
    // (oder hochkant/quer gewechselt wurde - der Ausschnitt hat dann ein anderes Format)
    static int radarZoom = einst.radarZoom;
    static bool radarHoch = einst.lage >= 2;
    if (radarZoom != einst.radarZoom || radarHoch != (einst.lage >= 2)) {
      radarZoom = einst.radarZoom, radarHoch = einst.lage >= 2;
      naechstesRadar = millis();
    }
    if (aktuell.gueltig && (int32_t)(millis() - naechstesRadar) >= 0) {
      Ort ort = ortAktuell();
      radarAktualisieren(ort.lat, ort.lon, radarZoom, radarHoch);
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
  return frisch(w);
}

WetterStatus wetterStatus() { return status; }
WetterFehler wetterFehler() { return fehler; }

int wetterNaechsterVersuch() {
  if (versuchLaeuft) return 0;
  int32_t rest = (int32_t)(naechster - millis());
  return rest > 0 ? (rest + 999) / 1000 : 0;
}

void wetterNeuLaden() { neuLaden = true; }
