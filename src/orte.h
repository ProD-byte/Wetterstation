#pragma once
// Weitere Orte (z. B. Arbeit, Familie, Urlaub): eigene Seite mit dem Wetter dort im Vergleich.
// Der Hauptort bleibt in ort.cpp. Die Liste liegt im Speicher und uebersteht Updates.

#include <Arduino.h>

const int ORTE_MAX = 3;  // dazu kommt der Hauptort - zusammen 4 Zeilen auf dem Display

struct OrtWetter {
  char name[40] = "";
  float lat = 0, lon = 0;
  bool gueltig = false;
  float temp = 0, max = 0, min = 0;
  int code = 0;
  bool tag = true;
};

void orteLaden();
int orteAnzahl();
int orteHolen(OrtWetter *liste);  // Kopie der Liste mit Wetter, Rueckgabe: Anzahl
bool orteHinzu(const char *name, float lat, float lon);
void orteEntfernen(int i);
void orteAktualisieren();         // aus der Wetter-Aufgabe: Wetter fuer alle Orte (eine Abfrage)
String orteJson();
