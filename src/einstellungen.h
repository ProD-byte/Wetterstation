#pragma once
// Einstellungen (Einstellungsseite wetterstation.local und Tasten), ueberstehen Updates

#include <Arduino.h>

const int SEITEN_ANZAHL = 9;  // Jetzt, 24 Stunden, Tage, Sonne & Mond, Luft & Pollen, Radar, Verlauf, Orte, ISS

struct Einstellungen {
  uint8_t licht = 0;          // Helligkeitsstufe am Tag (0 = hellste)
  uint16_t seiten = 0x1FF;    // welche Seiten gezeigt werden (Bit 0 = Jetzt ... Bit 8 = ISS)
  uint8_t wechsel = 12;       // Sekunden bis zur naechsten Seite, 0 = nur per Taste
  bool nacht = true;          // Nachtmodus: Display abends dunkler
  uint8_t nachtVon = 22;      // Uhrzeit (volle Stunde)
  uint8_t nachtBis = 6;
  bool autoUpdate = true;     // neue Builds von GitHub selbst installieren
  uint8_t sprache = 0;        // 0 = Deutsch, 1 = Englisch
  uint8_t lage = 0;           // Ausrichtung: 0 = quer (USB rechts), 1 = quer gedreht (USB links),
                              //              2 = hochkant (USB unten), 3 = hochkant (USB oben)
  uint8_t radarZoom = 0;      // Ausschnitt des Regenradars: 0 = nah, 1 = mittel, 2 = weit
  uint8_t warnAb = 1;         // DWD-Warnungen zeigen ab Stufe: 1 = alle (gelb), 2 = orange, 3 = rot, 5 = keine
  char name[25] = "wetterstation";  // Name im Netz (name.local) - verschieden, wenn mehrere Stationen im WLAN sind
};

// Name im Netz pruefen: 1-24 Zeichen a-z, 0-9 und "-" (nicht am Anfang/Ende)
bool nameGueltig(const String &n);

extern Einstellungen einst;

// Text in der eingestellten Sprache: TX("Heute", "Today")
#define TX(de, en) (einst.sprache ? (en) : (de))

void einstellungenLaden();
void einstellungenSpeichern();
