# 🌦️ Wetterstation

Animierte Wetterstation für das **LILYGO T-Display-S3** (ESP32-S3, 1,9″, 320 × 170 Pixel, 16 MB Flash, 8 MB PSRAM).
Zeigt das Wetter für deinen Ort mit Daten von [Open-Meteo](https://open-meteo.com/) (kostenlos, ohne Anmeldung).
Den Ort wählst du bei der Einrichtung am Handy (Name oder Postleitzahl). Ohne Auswahl ermittelt die Station ihn
ungefähr über die Internet-Adresse ([ipapi.co](https://ipapi.co/), ersatzweise [ipwho.is](https://ipwho.is/)); klappt auch das nicht, nimmt sie Cottbus.
Geschrieben mit **VS Code + PlatformIO** (Arduino-Kern 2.0.14 – TFT_eSPI läuft auf dem T-Display-S3 nicht mit Kern 3.x, siehe INFO.md).

**Open Source** (MIT-Lizenz) · **[Online-Flasher](https://prod-byte.github.io/Wetterstation/)** · **[Simulation](https://prod-byte.github.io/Wetterstation/simulation.html)** · *English below ↓*

## ✨ Funktionen

| | |
|---|---|
| ☀️ **Jetzt** | animiertes Wetterbild (Sonne, Wolken, Regen, Schnee, Gewitter mit Blitz, Nebel), Temperatur, gefühlte Temperatur, Wind, Luftfeuchte – und „Regen ab 14:15“ / „Regen bis 14:30“, wenn sich in den nächsten 2 Stunden etwas tut |
| 📈 **24 Stunden** | Temperaturkurve, die sich beim Umblättern aufbaut, Regenwahrscheinlichkeit als Balken |
| 📅 **Nächste Tage** | heute, morgen, übermorgen mit Höchst- und Tiefstwert und Regenrisiko |
| 🌅 **Sonne & Mond** | Sonne auf ihrem Tagesbogen, Auf- und Untergang, Tageslänge, UV-Index, Mondphase |
| 🌿 **Luft & Pollen** | Europäischer Luftqualitätsindex, Feinstaub, Erle, Birke, Gräser, Beifuß, Ambrosia |
| 📡 **Regenradar** | Radarbilder der letzten Stunde ([RainViewer](https://www.rainviewer.com/)) als Animation auf einer dunklen Karte ([OpenStreetMap](https://www.openstreetmap.org/copyright)); Ausschnitt nah, mittel oder weit. Ist nirgends Regen, steht „Kein Regen in der Nähe“ da |
| 📉 **Letzte 7 Tage** | Temperatur, Luftdruck und Regen der vergangenen Woche (auch als Diagramm auf der Einstellungsseite) |
| 🗺️ **Orte** | bis zu 3 weitere Orte im Vergleich mit deinem (Seite erscheint nur, wenn welche eingetragen sind) |
| ⚠️ **Warnungen** | Unwetterwarnungen des Deutschen Wetterdienstes ([brightsky.dev](https://brightsky.dev/)) blinken oben – alle, erst ab Orange, ab Rot oder gar nicht |
| 🌙 **Himmel** | Farbverlauf passend zum Wetter, Morgen- und Abendrot, ziehende Wolken, Regen/Schnee/Nebel über den ganzen Himmel, nachts funkelnde Sterne; Nachtmodus dimmt das Display |
| 📶 **WLAN** | Einrichtung per Handy (WLAN „Wetterstation-Setup“), merkt sich bis zu 5 Netze |
| 🌐 **Weltweit** | Uhrzeit, Nachtmodus, Sonnenauf- und -untergang richten sich nach der Zeitzone des gewählten Orts (inkl. Sommerzeit) |
| 🏷️ **Mehrere Stationen** | Name im Netz änderbar (z. B. `wetterstation-kueche.local`), damit mehrere Stationen im selben WLAN erreichbar sind |
| ⚙️ **Einstellungen** | im Browser unter **http://wetterstation.local**: Sprache, Ausrichtung, Ort, weitere Orte, Ausschnitt des Regenradars (nah/mittel/weit), Warnungen, Name im Netz, Helligkeit, Seiten, Blättern, Nachtmodus, Updates |
| 🔃 **Ausrichtung** | quer oder hochkant, jeweils in beide Richtungen (USB rechts/links bzw. unten/oben) – alle Seiten haben eine eigene Hochkant-Anordnung. Umschalten auf der Einstellungsseite; das T-Display-S3 hat keinen Lagesensor, deshalb dreht es sich nicht von selbst |
| 🌍 **Sprache** | Deutsch oder Englisch – Display, Einstellungsseite und Einrichtung. Bei der Einrichtung übernimmt die Station die Sprache des Handys |
| 🔄 **Updates** | von selbst aus dem Internet (neue Builds von GitHub), über WLAN (OTA) oder mit dem Online-Flasher |

Die Seiten blättern alle 12 s von allein weiter (einstellbar).

| Taste | kurz | lang |
|---|---|---|
| **IO0** | nächste Seite, nachts erst mal aufwecken | 2 s halten = Sprache Deutsch/Englisch wechseln (zeigt auch Adresse der Einstellungsseite und Build) |
| **IO14** | Helligkeit (4 Stufen), nachts erst mal aufwecken | 3 s halten = WLAN und Ort neu einrichten |

### Update aus dem Internet
Die Station schaut alle 6 Stunden auf GitHub Pages nach (`version.json`). Gibt es einen neueren Build, installiert
sie ihn selbst, sobald eine Minute lang keine Taste gedrückt wurde – abschaltbar auf der Einstellungsseite.
Im Nachtmodus bleibt das Display dabei dunkel.
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
4. Spätere Updates ohne Kabel: Umgebung **`env:tdisplay_s3_wlan`** → Upload. Hat die Station einen eigenen Namen, `upload_port` in `platformio.ini` anpassen.

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
| `src/radar.cpp` | Regenradar: Karte + Radarbilder laden, entpacken (PNGdec), im PSRAM halten |
| `src/verlauf.cpp`, `src/orte.cpp` | letzte 7 Tage, weitere Orte |
| `src/schriften.h` | geglättete Schriften (erzeugt, nicht von Hand ändern) |
| `src/netzupdate.cpp` | Update aus dem Internet (heißt nicht `update.cpp`, sonst verdeckt es unter Windows `Update.h` des ESP32-Kerns) |
| `simulation/wetterstation.html` | Simulation (zeichnet wie die Firmware) |
| `docs/` | Online-Flasher (GitHub Pages) |
| `website/basteln-wetter.js` | baut den Abschnitt in daveslab.de/basteln ein |
| `INFO.md` | Spickzettel zum Board: Pins, Strom, Akku, Macken |

## 🔒 Gut zu wissen

- Die Einstellungsseite hat **kein Passwort** – jeder im selben WLAN kann die Station umstellen. Für ein Heimnetz ist das gedacht, nicht für offene Netze.
- Die Station prüft die HTTPS-Zertifikate der Wetterdienste nicht (spart Speicher und Pflege). Sie schickt dabei nur ihren Standort.
- Die Daten kommen von kostenlosen Diensten für private Nutzung (siehe unten). Für kommerzielle Geräte bitte deren Bedingungen beachten.

## 📜 Lizenz

Der Code steht unter der **MIT-Lizenz** ([LICENSE](LICENSE)) – benutzen, ändern, weitergeben, auch in eigenen Projekten; der Hinweis auf die Lizenz muss bleiben.
Die eingebaute Schrift Lato hat ihre eigene Lizenz: [SIL Open Font License 1.1](lizenzen/OFL-Lato.txt).

## 📚 Verwendet

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) – Display
- [ArduinoJson](https://arduinojson.org/) – Wetterdaten lesen
- [PNGdec](https://github.com/bitbank2/PNGdec) – Radarbilder und Karte entpacken
- [Lato](https://www.latofonts.com/) von Łukasz Dziedzic – Schrift (SIL Open Font License 1.1)
- [Open-Meteo](https://open-meteo.com/) – Wetter, Luft & Pollen, Ortssuche (CC BY 4.0)
- [RainViewer](https://www.rainviewer.com/api.html) – Radarbilder
- [OpenStreetMap](https://www.openstreetmap.org/copyright) – Karte (© OpenStreetMap-Mitwirkende, ODbL)
- [Bright Sky](https://brightsky.dev/) – Warnungen des Deutschen Wetterdienstes
- [ipapi.co](https://ipapi.co/), [ipwho.is](https://ipwho.is/), [Zippopotam.us](https://www.zippopotam.us/) – ungefährer Standort, Postleitzahlen
- [ESP Web Tools](https://esphome.github.io/esp-web-tools/) – Online-Flasher

---

## 🇬🇧 English

Animated weather station for the **LILYGO T-Display-S3** (ESP32-S3, 1.9″, 320 × 170, 16 MB flash, 8 MB PSRAM) – open source, MIT licensed, works worldwide.

### Install – no programming needed
1. Open the **[online flasher](https://prod-byte.github.io/Wetterstation/)** in Chrome or Edge on a computer and connect the board via USB-C (no driver needed).
2. Click **“Löschen & neu installieren”** (erase & install) for a new board, or **“Update installieren”** to keep your Wi-Fi settings.
3. On your phone, join the Wi-Fi **“Wetterstation-Setup”** – the setup page opens by itself. Pick your Wi-Fi, enter the password, then search your town (or tap the suggestion; without a choice the station locates itself roughly via your internet connection).

The station follows your phone’s language (German or English) and the time zone of the chosen place.

### Pages
Now (animated weather, feels like, wind, humidity, “rain from 14:15”) · next 24 hours · next days · sun & moon · air quality & pollen (Europe) · **rain radar** (last hour, animated, near/medium/far) · last 7 days · up to 3 more places. Severe-weather warnings from the German Weather Service (Germany only).

### Buttons
| Button | short | long |
|---|---|---|
| **IO0** | next page (at night: wake up first) | hold 2 s = German/English, shows the settings address and build |
| **IO14** | brightness (4 levels) | hold 3 s = set up Wi-Fi and location again |

### Settings
Open **http://wetterstation.local** (or the station’s IP address) on the same Wi-Fi: language, location, more places, radar area, warnings, brightness, pages, night mode, orientation (landscape or portrait, either way round), automatic updates.
**Several stations on one Wi-Fi?** Give each one its own name there (e.g. `wetterstation-kitchen`) – it is then reachable as `wetterstation-kitchen.local`.

### Updates
The station checks GitHub every 6 hours and installs new builds by itself (can be switched off). Updates also work via the online flasher or over Wi-Fi from PlatformIO (`env:tdisplay_s3_wlan`).

### Build it yourself
VS Code + PlatformIO, environment `env:tdisplay_s3` (Arduino core 2.0.14 / `espressif32 @ 6.5.0` – TFT_eSPI does not run on this board with core 3.x). Copy `geheim.beispiel.ini` to `geheim.ini` first (password for Wi-Fi uploads, may stay empty).

### Good to know
The settings page has no password (meant for home networks). HTTPS certificates of the weather services are not verified. Data comes from free services for personal use: Open-Meteo, RainViewer, OpenStreetMap, Bright Sky – see the list above.

**License:** MIT ([LICENSE](LICENSE)); the embedded Lato font is under the [SIL Open Font License 1.1](lizenzen/OFL-Lato.txt).
