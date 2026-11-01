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
  p.end();
}
