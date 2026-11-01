# 🌦️ Wetterstation

Animierte Wetterstation für das **LILYGO T-Display-S3** (ESP32-S3, 1,9″, 320 × 170 Pixel, 16 MB Flash, 8 MB PSRAM).
Zeigt das Wetter für deinen Ort mit Daten von [Open-Meteo](https://open-meteo.com/) (kostenlos, ohne Anmeldung).
Den Ort wählst du bei der Einrichtung am Handy (Name oder Postleitzahl). Ohne Auswahl ermittelt die Station ihn
ungefähr über die Internet-Adresse ([ipapi.co](https://ipapi.co/), ersatzweise [ipwho.is](https://ipwho.is/)); klappt auch das nicht, nimmt sie Cottbus.
Geschrieben mit **VS Code + PlatformIO** (Arduino-Kern 2.0.14 – TFT_eSPI läuft auf dem T-Display-S3 nicht mit Kern 3.x, siehe INFO.md).

## ✨ Funktionen

| | |
|---|---|
| ☀️ **Jetzt** | animiertes Wetterbild (Sonne, Wolken, Regen, Schnee, Gewitter mit Blitz, Nebel), Temperatur, gefühlte Temperatur, Wind, Luftfeuchte – und „Regen ab 14:15“ / „Regen bis 14:30“, wenn sich in den nächsten 2 Stunden etwas tut |
| 📈 **24 Stunden** | Temperaturkurve, die sich beim Umblättern aufbaut, Regenwahrscheinlichkeit als Balken |
| 📅 **Nächste Tage** | heute, morgen, übermorgen mit Höchst- und Tiefstwert und Regenrisiko |
| 🌅 **Sonne & Mond** | Sonne auf ihrem Tagesbogen, Auf- und Untergang, Tageslänge, UV-Index, Mondphase |
| 🌿 **Luft & Pollen** | Europäischer Luftqualitätsindex, Feinstaub, Erle, Birke, Gräser, Beifuß, Ambrosia |
| ⚠️ **Warnungen** | Unwetterwarnungen des Deutschen Wetterdienstes ([brightsky.dev](https://brightsky.dev/)) blinken oben |
| 🌙 **Himmel** | Farbverlauf passend zum Wetter, nachts mit funkelnden Sternen; Nachtmodus dimmt das Display |
| 📶 **WLAN** | Einrichtung per Handy (WLAN „Wetterstation-Setup“), merkt sich bis zu 5 Netze |
| ⚙️ **Einstellungen** | im Browser unter **http://wetterstation.local**: Sprache, Bild drehen, Ort, Helligkeit, Seiten, Blättern, Nachtmodus, Updates |
| 🔃 **Einbaulage** | Bild um 180° drehen (Einstellungsseite), wenn das Board andersherum im Gehäuse sitzt. Das T-Display-S3 hat keinen Lagesensor, deshalb dreht es sich nicht von selbst |
| 🌍 **Sprache** | Deutsch oder Englisch – Display, Einstellungsseite und Einrichtung. Bei der Einrichtung übernimmt die Station die Sprache des Handys |
| 🔄 **Updates** | von selbst aus dem Internet (neue Builds von GitHub), über WLAN (OTA) oder mit dem Online-Flasher |

Die Seiten blättern alle 12 s von allein weiter (einstellbar).

| Taste | kurz | lang |
|---|---|---|
| **IO0** | nächste Seite | 2 s halten = Sprache Deutsch/Englisch wechseln (zeigt auch Adresse der Einstellungsseite und Build) |
| **IO14** | Helligkeit (4 Stufen), nachts erst mal aufwecken | 3 s halten = WLAN und Ort neu einrichten |

### Update aus dem Internet
Die Station schaut alle 6 Stunden auf GitHub Pages nach (`version.json`). Gibt es einen neueren Build, installiert
sie ihn selbst, sobald eine Minute lang keine Taste gedrückt wurde – abschaltbar auf der Einstellungsseite.
Firmware, die du selbst in VS Code baust (Build 0), sucht nicht nach Updates.

## 👀 Simulation

Wie das Display aussieht, zeigt die Simulation im Browser – mit echten Tasten und allen Wetterlagen:
**https://prod-byte.github.io/Wetterstation/simulation.html**

## ⚡ Firmware aufspielen

### Ohne Programmieren – Online-Flasher
**https://prod-byte.github.io/Wetterstation/** (auch auf [daveslab.de/basteln](https://daveslab.de/basteln/#wetter))
T-Display-S3 per USB-C anschließen, Chrome oder Edge am Computer (kein Treiber nötig).

### Mit VS Code + PlatformIO
1. Repository klonen und den Ordner in VS Code öffnen (PlatformIO-Erweiterung installiert).
2. `geheim.beispiel.ini` → **`geheim.ini`** kopieren und ein Passwort für WLAN-Updates eintragen (oder leer lassen).
3. Umgebung **`env:tdisplay_s3`** wählen → Upload (→).
4. Spätere Updates ohne Kabel: Umgebung **`env:tdisplay_s3_wlan`** → Upload.

> 🔒 `geheim.ini` wird **nicht** hochgeladen (siehe `.gitignore`).

## 📱 Erste Schritte

1. Board einschalten → das Display zeigt „WLAN einrichten“.
2. Mit dem Handy ins WLAN **„Wetterstation-Setup“** → die Einrichtungsseite öffnet sich von selbst.
3. Eigenes WLAN wählen, Passwort eingeben.
4. Die Seite fragt: **„Wo steht deine Wetterstation?“** – Ort oder Postleitzahl suchen, oder den ungefähr ermittelten Vorschlag antippen. Fertig.

WLAN oder Ort ändern: Taste **IO14** drei Sekunden halten.

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
| `src/web.cpp` | Einstellungsseite `wetterstation.local` |
| `src/einstellungen.cpp` | gespeicherte Einstellungen |
| `src/netzupdate.cpp` | Update aus dem Internet (heißt nicht `update.cpp`, sonst verdeckt es unter Windows `Update.h` des ESP32-Kerns) |
| `simulation/wetterstation.html` | Simulation (zeichnet wie die Firmware) |
| `docs/` | Online-Flasher (GitHub Pages) |
| `website/basteln-wetter.js` | baut den Abschnitt in daveslab.de/basteln ein |
| `INFO.md` | Spickzettel zum Board: Pins, Strom, Akku, Macken |

## 📚 Verwendet

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – Display
- [ArduinoJson](https://arduinojson.org/) – Wetterdaten lesen
- [Open-Meteo](https://open-meteo.com/) – Wetterdaten (CC BY 4.0)
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/) – Online-Flasher
