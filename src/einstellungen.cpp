#include "einstellungen.h"
#include <Preferences.h>

Einstellungen einst;

void einstellungenLaden() {
  Preferences p;
  p.begin("einst", true);
  Einstellungen e;
  e.licht = p.getUChar("licht", e.licht) % 4;
  e.seiten = p.getUChar("seiten8", e.seiten);  // neuer Schluessel: frueher gab es nur 5 Seiten
  if (!e.seiten) e.seiten = 1;  // mindestens "Jetzt"
  e.wechsel = p.getUChar("wechsel", e.wechsel);
  e.nacht = p.getBool("nacht", e.nacht);
  e.nachtVon = p.getUChar("von", e.nachtVon) % 24;
  e.nachtBis = p.getUChar("bis", e.nachtBis) % 24;
  e.autoUpdate = p.getBool("auto", e.autoUpdate);
  e.sprache = p.getUChar("sprache", e.sprache) % 2;
  e.gedreht = p.getBool("gedreht", e.gedreht);
  e.radarZoom = p.getUChar("radarzoom", e.radarZoom) % 3;
  e.warnAb = constrain(p.getUChar("warnab", e.warnAb), 1, 5);
  String n = p.getString("name", e.name);
  if (nameGueltig(n)) strlcpy(e.name, n.c_str(), sizeof(e.name));
  p.end();
  einst = e;
}

void einstellungenSpeichern() {
  Preferences p;
  p.begin("einst", false);
  p.putUChar("licht", einst.licht);
  p.putUChar("seiten8", einst.seiten);
  p.putUChar("wechsel", einst.wechsel);
  p.putBool("nacht", einst.nacht);
  p.putUChar("von", einst.nachtVon);
  p.putUChar("bis", einst.nachtBis);
  p.putBool("auto", einst.autoUpdate);
  p.putUChar("sprache", einst.sprache);
  p.putBool("gedreht", einst.gedreht);
  p.putUChar("radarzoom", einst.radarZoom);
  p.putUChar("warnab", einst.warnAb);
  p.putString("name", einst.name);
  p.end();
}

bool nameGueltig(const String &n) {
  if (n.length() < 1 || n.length() > 24 || n[0] == '-' || n[n.length() - 1] == '-') return false;
  for (char c : n)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  return true;
}
