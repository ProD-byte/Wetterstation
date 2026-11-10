#pragma once
// Ausfallsicherheit und Diagnose
//
// Ruecksprung: Eine neue Firmware (aus dem Internet oder per WLAN-Upload) gilt erst als gut, wenn sie
//   60 s lang laeuft und das WLAN verbunden ist. Startet das Geraet vorher neu (Absturz, Watchdog,
//   Strom weg), nimmt der Bootloader automatisch wieder die alte Firmware. Diesen Build installiert
//   die Station dann nicht noch einmal selbst (von Hand auf der Einstellungsseite geht es weiter).
// Watchdog: Haengt loop() laenger als 30 s, startet die Station neu. Meldet sich die Wetter-Aufgabe
//   10 min lang nicht, ebenso.
// Diagnose: Neustart-Grund, Laufzeit, Speicher, Zahl der Abstuerze und der letzte Absturz (aus dem
//   Core-Dump im Flash: Aufgabe, Adresse, Aufrufkette) - in /api/status und auf der Einstellungsseite.

#include <Arduino.h>

void diagnoseStart();             // ganz am Anfang von setup()
void diagnoseBereit();            // am Ende von setup(): Watchdog fuer loop() einschalten
void diagnoseLoop(uint32_t t);    // jede Runde in loop()
void diagnoseFuettern();          // waehrend langer Arbeit in loop() (Update-Fortschritt)
void diagnoseLebenszeichen();     // Wetter-Aufgabe: jede Runde

void diagnoseUpdateVersuch(int build);  // vor dem Installieren aus dem Internet (0 = Versuch vorbei, fehlgeschlagen)
int diagnoseKaputterBuild();            // Build, von dem zuletzt zurueckgesprungen wurde (0 = keiner)

String diagnoseJson();   // fuer /api/status
void diagnoseLoeschen(); // Einstellungsseite: Absturz-Zaehler und -Bericht zuruecksetzen
