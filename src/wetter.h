#pragma once
// Wetterdaten von open-meteo.com (kostenlos, ohne Anmeldung)
// Laeuft als eigene Aufgabe im Hintergrund, damit die Animationen nicht stocken.

#include <Arduino.h>

struct Wetter {
  bool gueltig = false;

  // jetzt
  float temp = 0, gefuehlt = 0, wind = 0;
  int feuchte = 0, code = 0;
  bool tag = true;

  // naechste 24 Stunden
  int stunden = 0;
  float stdTemp[24];
  int stdRegen[24];   // Regenwahrscheinlichkeit in %
  int stdStunde[24];  // Uhrzeit 0..23

  // heute + 3 Tage
  int tagCode[4];
  float tagMax[4], tagMin[4];
  int tagRegen[4];
  int tagWochentag[4];  // 0 = Sonntag

  uint32_t abruf = 0;  // millis() beim letzten Abruf
};

enum WetterStatus { VERBINDE, LADE, FERTIG, FEHLER };

void wetterStart();
bool wetterHolen(Wetter &w);  // Kopie der neuesten Daten, false solange noch keine da sind
WetterStatus wetterStatus();
