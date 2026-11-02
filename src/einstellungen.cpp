#include "einstellungen.h"
#include <Preferences.h>

Einstellungen einst;

void einstellungenLaden() {
  Preferences p;
  p.begin("einst", true);
  Einstellungen e;
  e.licht = p.getUChar("licht", e.licht) % 4;
  // neuer Schluessel je Seitenzahl: frueher 5, dann 8 Seiten - die ISS ist bei alten Einstellungen erst mal an
  if (p.isKey("seiten9")) e.seiten = p.getUShort("seiten9", e.seiten);
  else e.seiten = p.getUChar("seiten8", 0xFF) | 0x100;
  if (!e.seiten) e.seiten = 1;  // mindestens "Jetzt"
  e.wechsel = p.getUChar("wechsel", e.wechsel);
  e.nacht = p.getBool("nacht", e.nacht);
  e.nachtVon = p.getUChar("von", e.nachtVon) % 24;
  e.nachtBis = p.getUChar("bis", e.nachtBis) % 24;
  e.autoUpdate = p.getBool("auto", e.autoUpdate);
  e.sprache = p.getUChar("sprache", e.sprache) % 2;
  // frueher gab es nur "gedreht" (quer, 180 Grad) - wird beim ersten Start uebernommen
  e.lage = p.isKey("lage") ? p.getUChar("lage", 0) % 4 : (p.getBool("gedreht", false) ? 1 : 0);
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
  p.putUShort("seiten9", einst.seiten);
  p.putUChar("wechsel", einst.wechsel);
  p.putBool("nacht", einst.nacht);
  p.putUChar("von", einst.nachtVon);
  p.putUChar("bis", einst.nachtBis);
  p.putBool("auto", einst.autoUpdate);
  p.putUChar("sprache", einst.sprache);
  p.putUChar("lage", einst.lage);
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
