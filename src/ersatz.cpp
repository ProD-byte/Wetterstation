#include "ersatz.h"
#include "netzupdate.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

const char *quellenName(uint8_t q) { return q == BRIGHT_SKY ? "Bright Sky" : q == MET_NORWAY ? "MET Norway" : "Open-Meteo"; }

// HTTPS-Abruf mit Filter (die Antworten sind 40 KB gross, wir brauchen nur wenige Felder)
static int holen(const String &url, JsonDocument &doc, JsonDocument &filter) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);
  // MET Norway verlangt, dass sich jede Anwendung mit Namen und Kontakt meldet
  http.setUserAgent("Wetterstation/" + String(FIRMWARE_BUILD) + " github.com/ProD-byte/Wetterstation");
  if (!http.begin(client, url)) return -1;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Ersatz %s: HTTP %d\n", url.substring(8, 40).c_str(), code);
    http.end();
    return code;
  }
  String text = http.getString();
  http.end();
  if (deserializeJson(doc, text, DeserializationOption::Filter(filter))) return 0;
  return 200;
}

// "2026-11-02T14:00:00+01:00" oder "...Z" -> Unix-Zeit; abw = Zeitzone aus dem Text in Sekunden
static time_t isoZeit(const char *s, int32_t *abw = nullptr) {
  if (!s || strlen(s) < 19) return 0;
  int j = atoi(s), m = atoi(s + 5), t = atoi(s + 8), h = atoi(s + 11), mi = atoi(s + 14);
  // Tage seit 1970 (Howard Hinnant, days_from_civil)
  j -= m <= 2;
  int era = j / 400, yoe = j - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + t - 1;
  long tage = era * 146097L + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
  int32_t versatz = 0;
  const char *z = s + 19;
  if (*z == '+' || *z == '-') versatz = (*z == '-' ? -1 : 1) * (atoi(z + 1) * 3600 + atoi(z + 4) * 60);
  if (abw) *abw = versatz;
  return tage * 86400 + h * 3600 + mi * 60 - versatz;
}

// Sonnenauf- und -untergang (Sonnengleichung, auf eine Minute genau) fuer den Tag um 'jetzt'
// Ergebnis: 1 = normaler Tag, 0 = Polarnacht, 2 = Mitternachtssonne
static int sonne(float lat, float lon, time_t jetzt, time_t &auf, time_t &unter) {
  const double rad = M_PI / 180;
  double n = floor(jetzt / 86400.0 + 2440587.5 - 2451545.0 + 0.0008 + 0.5);
  double js = n - lon / 360.0;
  double M = fmod(357.5291 + 0.98560028 * js, 360);
  double C = 1.9148 * sin(M * rad) + 0.02 * sin(2 * M * rad) + 0.0003 * sin(3 * M * rad);
  double l = fmod(M + C + 180 + 102.9372, 360);
  double transit = 2451545.0 + js + 0.0053 * sin(M * rad) - 0.0069 * sin(2 * l * rad);
  double dekl = asin(sin(l * rad) * sin(23.4397 * rad));
  double cw = (sin(-0.833 * rad) - sin(lat * rad) * sin(dekl)) / (cos(lat * rad) * cos(dekl));
  if (cw > 1) return 0;
  if (cw < -1) return 2;
  double w0 = acos(cw) / rad;
  auf = (time_t)((transit - w0 / 360 - 2440587.5) * 86400);
  unter = (time_t)((transit + w0 / 360 - 2440587.5) * 86400);
  return 1;
}

// Gefuehlte Temperatur wie bei Open-Meteo (Steadman): Feuchte waermt, Wind kuehlt
static float gefuehlt(float t, float feuchte, float windKmh) {
  float e = feuchte / 100 * 6.105f * expf(17.27f * t / (237.7f + t));
  return t + 0.33f * e - 0.70f * windKmh / 3.6f - 4.0f;
}

// Stundenwerte beider Dienste in einheitlicher Form
struct Stunde {
  time_t zeit;
  float temp, feuchte, wind, regen;  // wind km/h, regen mm
  int chance;                        // Regenwahrscheinlichkeit %, -1 = unbekannt
  int code;                          // WMO-Code wie bei Open-Meteo
};

static int codeRegen(float mm, int leicht, int mittel, int stark) { return mm < 0.5f ? leicht : mm < 4 ? mittel : stark; }

// MET Norway: "lightrainshowers_day" usw.
static int codeMet(const char *s, float mm) {
  String c = s ? s : "";
  if (c.indexOf("thunder") >= 0) return 95;
  if (c.indexOf("sleet") >= 0) return c.startsWith("light") ? 66 : 67;
  bool schauer = c.indexOf("showers") >= 0;
  if (c.indexOf("snow") >= 0) return schauer ? (c.startsWith("heavy") ? 86 : 85) : c.startsWith("light") ? 71 : c.startsWith("heavy") ? 75 : 73;
  if (c.indexOf("rain") >= 0) return schauer ? (c.startsWith("light") ? 80 : c.startsWith("heavy") ? 82 : 81)
                                              : c.startsWith("light") ? 61 : c.startsWith("heavy") ? 65 : 63;
  if (c.startsWith("fog")) return 45;
  if (c.startsWith("clearsky")) return 0;
  if (c.startsWith("fair")) return 1;
  if (c.startsWith("partlycloudy")) return 2;
  return 3;
}

// Bright Sky: Symbol + Regenmenge
static int codeBrightSky(const char *s, float mm) {
  String c = s ? s : "";
  if (c == "thunderstorm") return 95;
  if (c == "hail") return 96;
  if (c == "sleet") return 67;
  if (c == "snow") return codeRegen(mm, 71, 73, 75);
  if (c == "rain") return codeRegen(mm, 61, 63, 65);
  if (c == "fog") return 45;
  if (c.startsWith("clear")) return 0;
  if (c.startsWith("partly")) return 2;
  return 3;
}

// Regenwahrscheinlichkeit grob aus der Menge, wenn der Dienst keine liefert (MET Norway)
static int chanceAusMenge(float mm) { return mm < 0.05f ? 5 : constrain((int)(40 + mm * 30), 40, 90); }

static int abrufBrightSky(const Ort &ort, Stunde *s, int &n, int32_t &abw) {
  // Tag "heute" in deutscher Zeit; tz=Europe/Berlin -> Zeiten mit Versatz, daraus die Zeitzone
  time_t jetzt = time(nullptr);
  struct tm z;
  time_t lokal = jetzt + abw;
  gmtime_r(&lokal, &z);
  char von[12], bis[12];
  strftime(von, sizeof(von), "%Y-%m-%d", &z);
  lokal += 4 * 86400;
  gmtime_r(&lokal, &z);
  strftime(bis, sizeof(bis), "%Y-%m-%d", &z);
  JsonDocument filter;
  JsonObject f = filter["weather"][0].to<JsonObject>();
  for (const char *k : {"timestamp", "temperature", "relative_humidity", "wind_speed", "precipitation", "precipitation_probability", "icon"})
    f[k] = true;
  JsonDocument doc;
  int code = holen("https://api.brightsky.dev/weather?lat=" + String(ort.lat, 4) + "&lon=" + String(ort.lon, 4) +
                       "&date=" + von + "&last_date=" + bis + "&tz=Europe/Berlin",
                   doc, filter);
  if (code != 200) return code;
  n = 0;
  for (JsonObject h : doc["weather"].as<JsonArray>()) {
    if (n >= 120) break;
    Stunde &x = s[n];
    x.zeit = isoZeit(h["timestamp"], &abw);
    x.temp = h["temperature"] | NAN;
    if (!x.zeit || isnan(x.temp)) continue;
    x.feuchte = h["relative_humidity"] | 70.0f;
    x.wind = h["wind_speed"] | 0.0f;  // Bright Sky: km/h
    x.regen = h["precipitation"] | 0.0f;
    x.chance = h["precipitation_probability"] | -1;
    x.code = codeBrightSky(h["icon"], x.regen);
    n++;
  }
  return n ? 200 : 0;
}

static int abrufMet(const Ort &ort, Stunde *s, int &n) {
  JsonDocument filter;
  JsonObject f = filter["properties"]["timeseries"][0].to<JsonObject>();
  f["time"] = true;
  f["data"]["instant"]["details"]["air_temperature"] = true;
  f["data"]["instant"]["details"]["relative_humidity"] = true;
  f["data"]["instant"]["details"]["wind_speed"] = true;
  for (const char *k : {"next_1_hours", "next_6_hours"}) {
    f["data"][k]["summary"]["symbol_code"] = true;
    f["data"][k]["details"]["precipitation_amount"] = true;
  }
  JsonDocument doc;
  // MET Norway moechte hoechstens 4 Nachkommastellen
  int code = holen("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=" + String(ort.lat, 4) +
                       "&lon=" + String(ort.lon, 4),
                   doc, filter);
  if (code != 200) return code;
  n = 0;
  for (JsonObject t : doc["properties"]["timeseries"].as<JsonArray>()) {
    if (n >= 120) break;
    Stunde &x = s[n];
    x.zeit = isoZeit(t["time"]);
    JsonObject d = t["data"]["instant"]["details"];
    x.temp = d["air_temperature"] | NAN;
    if (!x.zeit || isnan(x.temp)) continue;
    x.feuchte = d["relative_humidity"] | 70.0f;
    x.wind = (d["wind_speed"] | 0.0f) * 3.6f;  // m/s -> km/h
    // erst stuendlich, spaeter nur noch 6-Stunden-Werte
    JsonObject naechste = t["data"]["next_1_hours"].isNull() ? t["data"]["next_6_hours"] : t["data"]["next_1_hours"];
    x.regen = naechste["details"]["precipitation_amount"] | 0.0f;
    if (t["data"]["next_1_hours"].isNull()) x.regen /= 6;
    x.chance = chanceAusMenge(x.regen);
    x.code = codeMet(naechste["summary"]["symbol_code"], x.regen);
    n++;
  }
  return n ? 200 : 0;
}

int ersatzAbrufen(Wetter &w, const Ort &ort, Quelle q) {
  if (time(nullptr) < 1700000000) return 0;  // ohne Uhrzeit lassen sich die Stunden nicht zuordnen
  static Stunde s[120];                       // nur die Wetter-Aufgabe ruft das auf
  int n = 0;
  int32_t abw = w.utcAbweichung;  // MET Norway liefert UTC: Zeitzone vom letzten Open-Meteo-Abruf
  int code = q == BRIGHT_SKY ? abrufBrightSky(ort, s, n, abw) : abrufMet(ort, s, n);
  if (code != 200) return code;

  time_t jetzt = time(nullptr);
  int i0 = 0;  // aktuelle Stunde
  while (i0 + 1 < n && s[i0 + 1].zeit <= jetzt) i0++;
  const Stunde &a = s[i0];

  time_t auf = 0, unter = 0;
  int art = sonne(ort.lat, ort.lon, jetzt, auf, unter);
  w.tag = art == 1 ? (jetzt >= auf && jetzt < unter) : art == 2;
  w.utcAbweichung = abw;
  if (art == 1) {
    struct tm z;
    time_t l = auf + abw;
    gmtime_r(&l, &z);
    strftime(w.sonneAuf, sizeof(w.sonneAuf), "%H:%M", &z);
    l = unter + abw;
    gmtime_r(&l, &z);
    strftime(w.sonneUnter, sizeof(w.sonneUnter), "%H:%M", &z);
  } else {
    strcpy(w.sonneAuf, "--:--");
    strcpy(w.sonneUnter, "--:--");
  }

  w.temp = a.temp;
  w.feuchte = lroundf(a.feuchte);
  w.wind = a.wind;
  w.gefuehlt = gefuehlt(a.temp, a.feuchte, a.wind);
  w.code = a.code;

  // Regen in den naechsten 2 Stunden (hier nur stuendlich)
  w.regenArt = 0;
  bool jetztNass = a.regen >= 0.1f;
  for (int i = i0 + 1; i < n && s[i].zeit <= jetzt + 2 * 3600; i++) {
    if ((s[i].regen >= 0.1f) != jetztNass) {
      w.regenArt = jetztNass ? 2 : 1;
      time_t l = s[i].zeit + abw;
      struct tm z;
      gmtime_r(&l, &z);
      strftime(w.regenZeit, sizeof(w.regenZeit), "%H:%M", &z);
      break;
    }
  }

  // naechste 24 Stunden
  w.stunden = 0;
  for (int i = i0; i < n && w.stunden < 24; i++) {
    time_t l = s[i].zeit + abw;
    w.stdStunde[w.stunden] = l / 3600 % 24;
    w.stdTemp[w.stunden] = s[i].temp;
    w.stdRegen[w.stunden] = s[i].chance >= 0 ? s[i].chance : chanceAusMenge(s[i].regen);
    w.stunden++;
  }

  // heute + 3 Tage aus den Stunden: Min/Max, hoechste Regenchance, "schlimmstes" Wetter tagsueber
  long heute = (jetzt + abw) / 86400;
  for (int t = 0; t < 4; t++) {
    float mx = -99, mn = 99;
    int chance = 0, code = -1;
    for (int i = 0; i < n; i++) {
      time_t l = s[i].zeit + abw;
      if (l / 86400 != heute + t) continue;
      mx = max(mx, s[i].temp);
      mn = min(mn, s[i].temp);
      chance = max(chance, s[i].chance >= 0 ? s[i].chance : chanceAusMenge(s[i].regen));
      int std = l / 3600 % 24;
      if (std >= 7 && std <= 20) code = max(code, s[i].code);
    }
    if (mx < -90) return t ? 200 : 0;  // keine Daten mehr fuer diesen Tag - die frueheren Tage stimmen
    w.tagMax[t] = mx;
    w.tagMin[t] = mn;
    w.tagRegen[t] = chance;
    w.tagCode[t] = code >= 0 ? code : 3;
    w.tagWochentag[t] = (heute + t + 4) % 7;  // 1.1.1970 war ein Donnerstag
  }
  return 200;
}
