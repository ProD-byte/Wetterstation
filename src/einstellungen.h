#pragma once
// Einstellungen (Einstellungsseite wetterstation.local und Tasten), ueberstehen Updates

#include <Arduino.h>

const int SEITEN_ANZAHL = 5;  // Jetzt, 24 Stunden, Tage, Sonne & Mond, Luft & Pollen

struct Einstellungen {
  uint8_t licht = 0;          // Helligkeitsstufe am Tag (0 = hellste)
  uint8_t seiten = 0x1F;      // welche Seiten gezeigt werden (Bit 0 = Jetzt ...)
  uint8_t wechsel = 12;       // Sekunden bis zur naechsten Seite, 0 = nur per Taste
  bool nacht = true;          // Nachtmodus: Display abends dunkler
  uint8_t nachtVon = 22;      // Uhrzeit (volle Stunde)
  uint8_t nachtBis = 6;
  bool autoUpdate = true;     // neue Builds von GitHub selbst installieren
  uint8_t sprache = 0;        // 0 = Deutsch, 1 = Englisch
  bool gedreht = false;       // Bild um 180 Grad drehen (Board andersherum eingebaut)
};

extern Einstellungen einst;

// Text in der eingestellten Sprache: TX("Heute", "Today")
#define TX(de, en) (einst.sprache ? (en) : (de))

void einstellungenLaden();
void einstellungenSpeichern();
