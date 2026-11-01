#pragma once
// Wetterverlauf der letzten 7 Tage (stuendlich) von Open-Meteo (past_days=7) -
// gibt es sofort, die Station muss nicht erst eine Woche lang sammeln.

#include <Arduino.h>

const int VERLAUF_MAX = 168;  // 7 Tage x 24 Stunden

struct Verlauf {
  int n = 0;                    // Anzahl Stunden, aelteste zuerst, letzte = aktuelle Stunde
  float temp[VERLAUF_MAX];      // Grad
  float druck[VERLAUF_MAX];     // hPa (Meereshoehe)
  float regen[VERLAUF_MAX];     // mm in der Stunde
  int ersteStunde = 0;          // Uhrzeit des ersten Werts
  int ersterWochentag = 0;      // 0 = Sonntag
};

void verlaufAktualisieren(float lat, float lon);  // aus der Wetter-Aufgabe, stuendlich
bool verlaufHolen(Verlauf &v);
String verlaufJson();  // fuer die Einstellungsseite
