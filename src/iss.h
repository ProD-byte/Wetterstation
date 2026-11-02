#pragma once
// ISS-Ueberfluege: die Bahndaten (TLE) holt die Station einmal am Tag kostenlos von CelesTrak und rechnet
// daraus selbst (SGP4) aus, wann die ISS ueber dem eingestellten Ort zu sehen ist - weltweit, ohne Anmeldung.
// Zu sehen ist sie nur, wenn es bei uns dunkel ist (Sonne mind. 6 Grad unter dem Horizont),
// die ISS oben aber noch von der Sonne angestrahlt wird.

#include <Arduino.h>

struct IssPunkt {
  int16_t az10, el10;  // Richtung (0 = Nord, 90 = Ost) und Hoehe ueber dem Horizont, in Zehntelgrad
  bool sichtbar;       // in diesem Moment von der Sonne angestrahlt und bei uns dunkel
};

const int ISS_SCHRITT = 10;  // Sekunden zwischen zwei Bahnpunkten

struct IssUeberflug {
  bool gueltig = false;
  time_t start = 0, ende = 0;        // ueber dem Horizont
  time_t sichtAb = 0, sichtBis = 0;  // davon zu sehen
  float maxHoehe = 0;                // hoechster Punkt, solange sie zu sehen ist (Grad)
  float azMitte = 0;                 // Blickrichtung fuer die Ansicht (Mitte des sichtbaren Teils)
  float azAb = 0, azBis = 0;         // Richtung, in der sie auftaucht / verschwindet
  int punkte = 0;
  IssPunkt bahn[120];                // alle ISS_SCHRITT Sekunden ab start (reicht fuer 20 min)
};

// Aus der Wetter-Aufgabe aufrufen: holt die Bahndaten taeglich und rechnet bei Bedarf neu
void issAktualisieren(float lat, float lon);
// Naechster sichtbarer Ueberflug in den naechsten 2 Tagen (laeuft er gerade, dieser); false = keiner
bool issHolen(IssUeberflug &u);
// Himmelsrichtung als kurzer Text ("SW", "NO" ...) in der eingestellten Sprache
const char *issRichtung(float az);
