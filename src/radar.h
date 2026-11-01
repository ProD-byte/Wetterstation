#pragma once
// Regenradar: dunkle Karte (OpenStreetMap) mit den Radarbildern der letzten Stunde (RainViewer).
// Laedt im Hintergrund (Wetter-Aufgabe) in zwei abwechselnde Speichersaetze im PSRAM,
// damit die Animation beim Nachladen nie stockt.

#include <Arduino.h>

const int RADAR_B = 320, RADAR_H = 150;  // Ausschnitt in Pixeln (unter der Titelzeile)
const int RADAR_BILDER = 7;              // letzte Stunde in 10-Minuten-Schritten

struct RadarStand {
  bool karte = false;                      // Karte geladen
  int bilder = 0;                          // Anzahl Radarbilder (0 = noch keine)
  int regen = 0;                           // Pixel mit Regen im Ausschnitt (0 = trocken)
  const uint16_t *kartePixel = nullptr;    // RADAR_B * RADAR_H Farben (RGB565)
  const uint8_t *bild[RADAR_BILDER] = {};  // je Pixel 3 Byte: Farbe (RGB565, low/high) + Deckkraft
  time_t zeit[RADAR_BILDER] = {};          // Aufnahmezeit je Bild
};

// Aus der Wetter-Aufgabe: Karte (nur bei neuem Ort) und Radarbilder neu laden. Dauert einige Sekunden.
// stufe: 0 = nah (ca. 120 x 55 km), 1 = mittel (240 x 110 km), 2 = weit (480 x 220 km)
void radarAktualisieren(float lat, float lon, int stufe);
bool radarHolen(RadarStand &r);  // false solange noch nichts da ist
