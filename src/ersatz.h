#pragma once
// Ersatz-Wetterdienste, solange Open-Meteo nicht antwortet (beide kostenlos, ohne Anmeldung):
//  - Bright Sky (Daten des Deutschen Wetterdienstes, brightsky.dev) - bei deutscher Sprache zuerst
//  - MET Norway (Norwegischer Wetterdienst, api.met.no) - weltweit
// Beide liefern Stundenwerte; Tage, Sonnenauf-/-untergang und gefuehlte Temperatur rechnen wir selbst aus.
// Was sie nicht haben (UV, Viertelstunden-Regen), bleibt vom letzten Open-Meteo-Abruf stehen.

#include "wetter.h"
#include "ort.h"

enum Quelle : uint8_t { OPEN_METEO, BRIGHT_SKY, MET_NORWAY };

// 200 = ok, sonst HTTP-Code (negativ = keine Verbindung, 0 = Antwort nicht lesbar)
int ersatzAbrufen(Wetter &w, const Ort &ort, Quelle q);
const char *quellenName(uint8_t q);
