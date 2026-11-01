# LILYGO TTGO T-Display (ESP32) – Spickzettel

Gekauft bei Eckstein (Art.-Nr. TT01001, 16,95 €).
Hersteller-Repo: https://github.com/Xinyuan-LilyGO/TTGO-T-Display

## Eckdaten

| | |
|---|---|
| Chip | ESP32 (klassisch, 2 Kerne, 240 MHz), 520 kB SRAM, **kein PSRAM** |
| Flash | 4 MB laut Shop (es gibt auch 16-MB-Versionen – siehe „Flash prüfen“) |
| Display | 1,14" IPS, ST7789V, **135 × 240 Pixel**, SPI |
| USB | USB-C, USB-Seriell-Chip **CH9102** (Windows: Treiber „CH343/CH9102“ von wch.cn) |
| Spannungsregler | AP2112K, 3,3 V, > 600 mA |
| Akku | 3,7-V-LiPo an JST 1,25 mm 2-polig, Ladechip TP4054, **500 mA** Ladestrom |
| Funk | WLAN 2,4 GHz b/g/n, Bluetooth 4.2 (Classic + BLE) |
| Größe | 51,5 × 25 × 8,5 mm, 7,8 g |

## Pins

### Belegt auf dem Board

| Funktion | GPIO |
|---|---|
| Display MOSI | 19 |
| Display SCLK | 18 |
| Display CS | 5 |
| Display DC | 16 |
| Display RST | 23 |
| Display-Hintergrundlicht (dimmbar per PWM) | 4 |
| Taster links (auch BOOT) | 0 |
| Taster rechts | 35 |
| Akkuspannung (über 1:2-Teiler) | 34 |
| ADC-Teiler einschalten (ADC_EN) | 14 |
| I2C SDA / SCL (auch an 4-pol. Stecker) | 21 / 22 |

### Frei für eigene Sachen

| Art | GPIO | Hinweis |
|---|---|---|
| Ein- und Ausgang, ohne Haken | **17, 25, 26, 27, 32, 33** | 25 + 26 haben DAC (echter Analogausgang) |
| Nur Eingang (kein Pull-up/-down) | 36, 37, 38, 39 | gut für Analogwerte (ADC1) |
| Mit Vorsicht | 2, 12, 13, 15 | 2 und 12 sind Boot-Pins: beim Einschalten nicht auf HIGH ziehen |
| I2C | 21, 22 | frei, wenn man kein I2C braucht |

Wichtig:
- Mit WLAN an funktionieren die ADC2-Pins (u. a. 2, 12–15, 25–27) **nicht** als Analogeingang. Analog also an 32, 33, 36–39.
- Ein zweiter SPI-Bus ist praktisch nicht möglich (GPIO 14 ist nicht herausgeführt). Sensoren am besten per I2C anschließen.
- Es gibt **keine** frei programmierbare LED. Die blaue LED gehört zum Ladechip.
- Der I2C-Stecker hat die Reihenfolge GND, 3,3 V, SDA, SCL. Das passt nicht zu jedem OLED-/Sensor-Kabel.

## Stromversorgung

| Quelle | Bereich | Hinweis |
|---|---|---|
| USB-C oder 5V-Pin | 3,8–6 V | läuft über den Regler |
| 3V3-Pin | 2,3–3,6 V | **umgeht den Regler** – über 3,6 V geht der ESP32 kaputt |
| LiPo | 3,7–4,2 V | wird bei USB geladen, Abschaltung unter ca. 3 V |

Verbrauch (gemessen von anderen):
- ca. 40 mA ohne WLAN, ca. 65 mA mit Display an, 80–140 mA mit WLAN (Spitzen bis 370 mA)
- Light Sleep ca. 9 mA, Deep Sleep ca. 260–350 µA. Dafür vorher Display ausschalten und **GPIO 14 auf LOW**, sonst bleiben es ca. 9 mA.

Akku-Hinweise:
- **Polung am JST-Stecker prüfen!** Er ist nicht beschriftet und manche Akkus sind falsch herum belegt.
- 500 mA Ladestrom ist für kleine Akkus (< 1000 mAh) recht viel.
- Unter ca. 3,2 V fängt das Hintergrundlicht an zu flackern.

## Akkuspannung messen

```cpp
pinMode(14, OUTPUT);
digitalWrite(14, HIGH);            // Teiler einschalten (nötig im Akkubetrieb)
analogSetPinAttenuation(34, ADC_11db);
float volt = analogReadMilliVolts(34) * 2.0f / 1000.0f;  // Teiler 1:2
```

- Faktor 2,0 ist der Sollwert. Mit Multimeter nachmessen und anpassen (einer kam auf 2,04).
- Mehrere Messungen mitteln, es gibt öfter Ausreißer nach oben.
- **Während USB lädt, stimmt der Wert nicht** (zeigt fest ca. 4,2 V oder über 4,6 V). Nur im Akkubetrieb aussagekräftig.

## Software

### PlatformIO (so ist das Projekt eingerichtet)

- `board = lilygo-t-display` gibt es fertig in PlatformIO.
- Display-Bibliothek: **TFT_eSPI** von Bodmer. Die Einstellungen kommen über `build_flags`
  (`USER_SETUP_LOADED` + `Setup25_TTGO_T_Display.h`), man muss in der Bibliothek nichts ändern.
- `monitor_rts = 0` und `monitor_dtr = 0` setzen. Sonst kann der serielle Monitor den ESP32 resetten
  oder in den Download-Modus schicken (das Board hat dafür keine Schutzschaltung).

### Arduino IDE (falls gewünscht)

- Board „ESP32 Dev Module“ (oder „LilyGo T-Display“), PSRAM aus, Flash 4 MB.
- In TFT_eSPI die Datei `User_Setup_Select.h` öffnen: `Setup1` auskommentieren, `Setup25_TTGO_T_Display.h` einkommentieren.
- LILYGO schreibt, dass ihre Beispiele mit TFT_eSPI bis 2.0.14 und ESP32-Kern 2.0.14 getestet sind.

### Alternativen

- **MicroPython**: das offizielle Image hat keinen ST7789-Treiber, man braucht z. B. `st7789_mpy` von russhughes.
- **CircuitPython**: offiziell unterstützt (Board „LILYGO TTGO T-Display ESP32“).
- **ESPHome**: läuft mit `board: esp32dev`, Display über die `st7789v`-Komponente.

## Bekannte Macken

- **Startet nach dem Einstecken manchmal nicht:** Bei manchen Boards entlädt sich ein Kondensator neben der USB-Buchse zu langsam. Dann einfach Reset drücken (Taster an der Seite).
- **Upload klappt nicht:** linken Taster (GPIO 0) halten, kurz Reset drücken, loslassen. Dann ist das Board im Download-Modus.
- Schutzfolie auf dem Display lassen, bis die Stiftleisten gelötet sind (Flussmittel spritzt).

## Flash-Größe prüfen

Im Startprojekt wird die echte Flash-Größe beim Start auf dem seriellen Monitor angezeigt.
Hat dein Board 16 MB, kannst du in `platformio.ini` auf `board_upload.flash_size = 16MB` umstellen.

## Ideen zum Experimentieren

- Uhr mit Uhrzeit aus dem Internet (NTP) und Akkuanzeige
- WLAN-Scanner: Netze mit Signalstärke als Balken
- BLE-Scanner: Geräte in der Nähe anzeigen
- Sensor am I2C-Stecker (z. B. BME280 für Temperatur/Feuchte/Luftdruck) mit Verlaufskurve
- Kleine Fernbedienung per ESP-NOW (funkt direkt zwischen ESP32s, ohne Router)
- Mini-Spiel mit den zwei Tastern (z. B. Reaktionstest)
- Deep-Sleep-Experiment: Wie lange hält der Akku, wenn das Board nur alle 10 min aufwacht?

## Quellen

- Hersteller: https://github.com/Xinyuan-LilyGO/TTGO-T-Display
- Ausführliche Pin- und Stromanalyse: https://done.land/components/microcontroller/families/esp/esp32/developmentboards/esp32s/t-display/
- Projekte und Messwerte: https://github.com/kreier/t-display
- TFT_eSPI-Setup: https://github.com/Bodmer/TFT_eSPI/blob/master/User_Setups/Setup25_TTGO_T_Display.h
- TFT_eSPI mit PlatformIO: https://github.com/Bodmer/TFT_eSPI/wiki/Installing-on-PlatformIO
