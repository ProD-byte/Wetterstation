# 🌦️ Wetterstation

Animierte Wetterstation für das **LILYGO TTGO T-Display** (ESP32, 1,14″, 240 × 135 Pixel).
Zeigt das Wetter für deinen Ort mit Daten von [Open-Meteo](https://open-meteo.com/) (kostenlos, ohne Anmeldung).
Den Ort wählst du bei der Einrichtung am Handy (Name oder Postleitzahl). Ohne Auswahl ermittelt die Station ihn
ungefähr über die Internet-Adresse ([ipapi.co](https://ipapi.co/), ersatzweise [ipwho.is](https://ipwho.is/)); klappt auch das nicht, nimmt sie Cottbus.
Geschrieben mit **VS Code + PlatformIO**.

## ✨ Funktionen

| | |
|---|---|
| ☀️ **Jetzt** | animiertes Wetterbild (Sonne, Wolken, Regen, Schnee, Gewitter mit Blitz, Nebel), Temperatur, gefühlte Temperatur, Wind, Luftfeuchte |
| 📈 **24 Stunden** | Temperaturkurve, die sich beim Umblättern aufbaut, Regenwahrscheinlichkeit als Balken |
| 📅 **Nächste Tage** | heute, morgen, übermorgen mit Höchst- und Tiefstwert und Regenrisiko |
| 🌙 **Himmel** | Farbverlauf passend zum Wetter, nachts mit funkelnden Sternen |
| 📶 **WLAN** | Einrichtung per Handy (WLAN „Wetterstation-Setup“), merkt sich bis zu 5 Netze |
| 🔄 **Updates** | über WLAN (OTA) oder im Browser mit dem Online-Flasher |

Die Seiten blättern alle 12 s von allein weiter.

| Taste | kurz | lang |
|---|---|---|
| **IO0** | nächste Seite | |
| **IO35** | Helligkeit (4 Stufen) | 3 s halten = WLAN neu einrichten |

## 👀 Simulation

Wie das Display aussieht, zeigt die Simulation im Browser – mit echten Tasten und allen Wetterlagen:
**https://prod-byte.github.io/Wetterstation/simulation.html**

## ⚡ Firmware aufspielen

### Ohne Programmieren – Online-Flasher
**https://prod-byte.github.io/Wetterstation/** (auch auf [daveslab.de/basteln](https://daveslab.de/basteln/#wetter))
T-Display per USB-C anschließen, Chrome oder Edge am Computer.

### Mit VS Code + PlatformIO
1. Repository klonen und den Ordner in VS Code öffnen (PlatformIO-Erweiterung installiert).
2. `geheim.beispiel.ini` → **`geheim.ini`** kopieren und ein Passwort für WLAN-Updates eintragen (oder leer lassen).
3. Umgebung **`env:tdisplay`** wählen → Upload (→).
4. Spätere Updates ohne Kabel: Umgebung **`env:tdisplay_wlan`** → Upload.

> 🔒 `geheim.ini` wird **nicht** hochgeladen (siehe `.gitignore`).

## 📱 Erste Schritte

1. Board einschalten → das Display zeigt „WLAN einrichten“.
2. Mit dem Handy ins WLAN **„Wetterstation-Setup“** → die Einrichtungsseite öffnet sich von selbst.
3. Eigenes WLAN wählen, Passwort eingeben.
4. Die Seite fragt: **„Wo steht deine Wetterstation?“** – Ort oder Postleitzahl suchen, oder den ungefähr ermittelten Vorschlag antippen. Fertig.

WLAN oder Ort ändern: Taste **IO35** drei Sekunden halten.

## 🤖 Automatischer Build (GitHub Actions)

Bei jedem Hochladen baut GitHub die Firmware automatisch (`.github/workflows/build.yml`):

- **Actions** → Lauf anklicken → **Artifacts**: `wetterstation-komplett.bin` (Neuinstallation) und die Einzelteile (Update)
- Ein **Tag** wie `v1.0.0` erstellt ein **Release**.
- Der **Online-Flasher** und die **Simulation** (GitHub Pages) bekommen automatisch die neueste Version.

## 📁 Aufbau

| Datei | Inhalt |
|---|---|
| `src/main.cpp` | Seiten, Tasten, Startbild, Update über WLAN |
| `src/bilder.cpp` | Himmel, animierte Wetterbilder, Text mit Umlauten |
| `src/wetter.cpp` | holt das Wetter alle 15 min im Hintergrund |
| `src/wlan.cpp` | WLAN mit Einrichtung per Handy (inkl. Ortswahl) |
| `src/ort.cpp` | Standort: speichern, über die Internet-Adresse ermitteln, Orte suchen |
| `simulation/wetterstation.html` | Simulation (zeichnet wie die Firmware) |
| `docs/` | Online-Flasher (GitHub Pages) |
| `website/basteln-wetter.js` | baut den Abschnitt in daveslab.de/basteln ein |
| `INFO.md` | Spickzettel zum Board: Pins, Strom, Akku, Macken |

## 📚 Verwendet

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – Display
- [ArduinoJson](https://arduinojson.org/) – Wetterdaten lesen
- [Open-Meteo](https://open-meteo.com/) – Wetterdaten (CC BY 4.0)
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/) – Online-Flasher
