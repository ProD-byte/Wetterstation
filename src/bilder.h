#pragma once
// Animierte Wetterbilder, Himmel und Text mit Umlauten

#include <TFT_eSPI.h>

enum Bild { KLAR, HEITER, BEDECKT, NEBEL, NIESEL, REGEN, SCHNEE, GEWITTER };
enum Ausrichtung { LINKS, MITTE, RECHTS };

uint16_t farbe(uint8_t r, uint8_t g, uint8_t b);

// Wettercode (WMO, wie open-meteo ihn liefert) -> Bild und Text
Bild bildFuerCode(int code);
const char *textFuerCode(int code, bool tag);

// Himmel mit Farbverlauf (nachts mit Sternen), blitz = kurz aufhellen
void zeichneHimmel(TFT_eSprite &s, Bild b, bool tag, uint32_t t, bool blitz);
bool blitzAktiv(uint32_t t);

// Wetterbild um (cx, cy), groesse = ungefaehre Breite in Pixeln, t = millis() fuer die Animation
void zeichneBild(TFT_eSprite &s, Bild b, bool tag, int cx, int cy, int groesse, uint32_t t);

// Text mit Schatten, y = Grundlinie. Kann ä ö ü Ä Ö Ü ß (die Schriften selbst nicht).
// Schrift vorher mit setFreeFont() oder setTextFont() waehlen.
void schreibe(TFT_eSprite &s, const char *text, int x, int y, uint16_t f, Ausrichtung a);
int breite(TFT_eSprite &s, const char *text);
// Temperatur mit Gradzeichen
void schreibeTemp(TFT_eSprite &s, float wert, int x, int y, uint16_t f, Ausrichtung a);
