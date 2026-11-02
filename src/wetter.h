#pragma once
// Wetterdaten von open-meteo.com (kostenlos, ohne Anmeldung), dazu Luft & Pollen (auch Open-Meteo)
// und Unwetterwarnungen des Deutschen Wetterdienstes (ueber brightsky.dev).
// Laeuft als eigene Aufgabe im Hintergrund, damit die Animationen nicht stocken.

#include <Arduino.h>

struct Wetter {
  bool gueltig = false;
  char ort[40] = "";  // Ortsname fuer die Anzeige

  // jetzt
  float temp = 0, gefuehlt = 0, wind = 0;
  int feuchte = 0, code = 0;
  bool tag = true;
  // Regen in den naechsten 2 Stunden: 0 = nichts, 1 = beginnt um regenZeit, 2 = hoert um regenZeit auf
  int regenArt = 0;
  char regenZeit[6] = "";

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

  // Sonne (heute)
  char sonneAuf[6] = "--:--", sonneUnter[6] = "--:--";
  float uv = 0;

  // Luft & Pollen
  bool luftGueltig = false;
  int aqi = 0;          // Europaeischer Luftqualitaetsindex (0 = sehr gut ... 100+ = extrem schlecht)
  float pm25 = 0;
  float pollen[5] = {};  // Koerner/m3: Erle, Birke, Graeser, Beifuss, Ambrosia

  // DWD-Warnung (die schwerste, nur in Deutschland)
  char warnung[48] = "";  // leer = keine
  char warnungEn[48] = "";
  int warnStufe = 0;      // 1 gering ... 4 extrem

  int32_t utcAbweichung = 3600;  // Zeitzone des Orts in Sekunden (inkl. Sommerzeit)

  uint32_t abruf = 0;  // millis() beim letzten Abruf
  time_t stand = 0;    // Uhrzeit (Unix) des letzten erfolgreichen Abrufs - fuer "Stand 14:30"
  uint8_t quelle = 0;  // woher die Daten kommen (Quelle in ersatz.h): 0 = Open-Meteo, sonst Ersatzdienst
};

// Aelter als das werden Daten nicht mehr angezeigt (Wetterdienst laenger nicht erreichbar)
const time_t DATEN_MAX_ALTER = 2 * 3600;

const char *pollenName(int i);  // in der eingestellten Sprache

enum WetterStatus { VERBINDE, ORTUNG, LADE, FERTIG, FEHLER };
// Warum der letzte Abruf gescheitert ist (fuer den Hinweis auf dem Display)
enum WetterFehler { KEIN_FEHLER, UEBERLASTET, KEIN_INTERNET, NICHT_ERREICHBAR };

void wetterStart();
bool wetterHolen(Wetter &w);  // Kopie der neuesten Daten, false solange noch keine da sind
WetterStatus wetterStatus();
WetterFehler wetterFehler();
int wetterNaechsterVersuch();  // Sekunden bis zum naechsten Versuch nach einem Fehler (0 = laeuft gerade)
void wetterNeuLaden();        // z. B. nach Ortswechsel auf der Einstellungsseite
void wetterOrteNeu();         // weitere Orte geaendert -> gleich deren Wetter holen
void wetterOrtAutomatisch();  // Ort wieder ueber die Internet-Adresse bestimmen (im Hintergrund)
