#include "web.h"
#include "web_html.h"
#include "einstellungen.h"
#include "ort.h"
#include "netzupdate.h"
#include "wetter.h"
#include "wlan.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(80);

static String jsonText(const String &s) {
  String aus;
  for (char c : s) {
    if (c == '"' || c == '\\') aus += '\\';
    if ((uint8_t)c >= 0x20) aus += c;
  }
  return aus;
}

static void seite() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", WEB_HTML);
}

static void status() {
  Ort o = ortAktuell();
  String j = "{\"ort\":\"" + jsonText(o.name) + "\",\"gewaehlt\":" + (o.vomNutzer ? "true" : "false") +
             ",\"build\":" + FIRMWARE_BUILD + ",\"neu\":" + updateNeuerBuild() + ",\"ip\":\"" +
             WiFi.localIP().toString() + "\",\"wlan\":\"" + jsonText(WiFi.SSID()) + "\",\"rssi\":" + WiFi.RSSI() +
             ",\"einst\":{\"licht\":" + einst.licht + ",\"seiten\":" + einst.seiten + ",\"wechsel\":" + einst.wechsel +
             ",\"nacht\":" + (einst.nacht ? "true" : "false") + ",\"von\":" + einst.nachtVon + ",\"bis\":" +
             einst.nachtBis + ",\"auto\":" + (einst.autoUpdate ? "true" : "false") + ",\"sprache\":" + einst.sprache +
             ",\"gedreht\":" + (einst.gedreht ? "true" : "false") + "}}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

static void einstellungenSetzen() {
  auto zahl = [](const char *n, int alt) { return server.hasArg(n) ? server.arg(n).toInt() : alt; };
  einst.licht = constrain(zahl("licht", einst.licht), 0, 3);
  einst.seiten = zahl("seiten", einst.seiten) & 0x1F;
  if (!einst.seiten) einst.seiten = 1;
  einst.wechsel = constrain(zahl("wechsel", einst.wechsel), 0, 120);
  // Kontrollkaestchen kommen als "true"/"false" oder 1/0
  auto janein = [](const char *n, bool alt) {
    if (!server.hasArg(n)) return alt;
    String v = server.arg(n);
    return v == "1" || v == "true";
  };
  einst.nacht = janein("nacht", einst.nacht);
  einst.nachtVon = constrain(zahl("von", einst.nachtVon), 0, 23);
  einst.nachtBis = constrain(zahl("bis", einst.nachtBis), 0, 23);
  einst.autoUpdate = janein("auto", einst.autoUpdate);
  einst.sprache = constrain(zahl("sprache", einst.sprache), 0, 1);
  einst.gedreht = janein("gedreht", einst.gedreht);
  einstellungenSpeichern();
  server.send(200, "text/plain", "ok");
}

static void orteSuchen() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", ortSuchen(server.arg("q")));
}

static void ortSetzen() {
  Ort o;
  o.lat = server.arg("lat").toFloat();
  o.lon = server.arg("lon").toFloat();
  strlcpy(o.name, server.arg("name").c_str(), sizeof(o.name));
  o.vomNutzer = true;
  ortSpeichern(o);
  wetterNeuLaden();
  server.send(200, "text/plain", "ok");
}

static void ortAutomatisch() {
  server.send(200, "text/plain", "ok");
  Ort o = ortAktuell();
  if (ortPerIp(o)) {
    ortSpeichern(o);
    wetterNeuLaden();
  }
}

void webStart() {
  server.on("/", HTTP_GET, seite);
  server.on("/api/status", HTTP_GET, status);
  server.on("/api/einstellungen", HTTP_POST, einstellungenSetzen);
  server.on("/orte", HTTP_GET, orteSuchen);
  server.on("/api/ort", HTTP_POST, ortSetzen);
  server.on("/api/ort-auto", HTTP_POST, ortAutomatisch);
  server.on("/api/update/suchen", HTTP_POST, []() {
    updateSuchenLassen();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/update/installieren", HTTP_POST, []() {
    updateInstallierenLassen();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/wlan", HTTP_POST, []() {
    server.send(200, "text/plain", "ok");
    delay(300);
    wlanEinrichtungAnfordern();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "nicht gefunden"); });
  server.begin();
  Serial.printf("Einstellungen: http://%s.local oder http://%s\n", WLAN_HOSTNAME, WiFi.localIP().toString().c_str());
}

void webLoop() { server.handleClient(); }
