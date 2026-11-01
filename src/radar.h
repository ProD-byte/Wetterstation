#pragma once
// Regenradar: dunkle Karte (OpenStreetMap) mit den Radarbildern der letzten Stunde (RainViewer).
// Laedt im Hintergrund (Wetter-Aufgabe) in zwei abwechselnde Speichersaetze im PSRAM,
// damit die Animation beim Nachladen nie stockt.

#include <Arduino.h>

// Ausschnitt in Pixeln (unter der Titelzeile): quer 320 x 150, hochkant 170 x 268
const int RADAR_QUER_B = 320, RADAR_QUER_H = 150;
const int RADAR_HOCH_B = 170, RADAR_HOCH_H = 268;
const int RADAR_PIXEL_MAX = 50000;      // reicht fuer beide Formate
const int RADAR_BILDER = 7;             // letzte Stunde in 10-Minuten-Schritten

struct RadarStand {
  bool karte = false;                      // Karte geladen
  int bilder = 0;                          // Anzahl Radarbilder (0 = noch keine)
  int regen = 0;                           // Pixel mit Regen im Ausschnitt (0 = trocken)
  int b = 0, h = 0;                        // Groesse des Ausschnitts
  const uint16_t *kartePixel = nullptr;    // b * h Farben (RGB565)
  const uint8_t *bild[RADAR_BILDER] = {};  // je Pixel 3 Byte: Farbe (RGB565, low/high) + Deckkraft
  time_t zeit[RADAR_BILDER] = {};          // Aufnahmezeit je Bild
};

// Aus der Wetter-Aufgabe: Karte (nur bei neuem Ort/Zoom/Format) und Radarbilder neu laden. Dauert einige Sekunden.
// stufe: 0 = nah, 1 = mittel, 2 = weit; hochkant: Ausschnitt 170 x 268 statt 320 x 150
void radarAktualisieren(float lat, float lon, int stufe, bool hochkant);
bool radarHolen(RadarStand &r);  // false solange noch nichts da ist
