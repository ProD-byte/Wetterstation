#include "diagnose.h"
#include "netzupdate.h"  // FIRMWARE_BUILD
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_core_dump.h>
#include <time.h>

static const uint32_t WATCHDOG_S = 30;               // loop() darf hoechstens so lange haengen
static const uint32_t PROBEZEIT = 60000;             // neue Firmware muss so lange laufen (mit WLAN)
static const uint32_t WETTER_STILL = 10 * 60000UL;  // Wetter-Aufgabe so lange ohne Lebenszeichen -> Neustart

// Arduino bestaetigt neue Firmware sonst schon beim Start (esp32-hal-misc.c) - wir wollen erst sehen, ob sie laeuft
extern "C" bool verifyRollbackLater() { return true; }

static bool probe = false;       // laufende Firmware ist neu und noch nicht bestaetigt
static bool zeitNachtragen = false;  // Absturz/Ruecksprung beim Start erkannt, Uhrzeit noch unbekannt
static bool wdtAn = false;
static const char *neustartGrund = "unbekannt";
static volatile uint32_t wetterLeben = 0;  // millis() des letzten Lebenszeichens (0 = Aufgabe laeuft noch nicht)
static int kaputt = 0;

static const char *grundName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "strom";      // eingesteckt / Akku
    case ESP_RST_EXT: return "extern";
    case ESP_RST_SW: return "neustart";        // von der Firmware gewollt (Update, Name, WLAN ...)
    case ESP_RST_PANIC: return "absturz";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "spannung";  // Spannung eingebrochen (Netzteil / Kabel / Akku leer)
    case ESP_RST_DEEPSLEEP: return "tiefschlaf";
    default: return "unbekannt";
  }
}

static bool istAbsturz(esp_reset_reason_t r) {
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
         r == ESP_RST_BROWNOUT;
}

// Schluessel loeschen, ohne dass Preferences fuer jeden fehlenden einen Fehler ausgibt
static void weg(Preferences &p, const char *k) {
  if (p.isKey(k)) p.remove(k);
}

static bool zeitBekannt() { return time(nullptr) > 1700000000; }

// Core-Dump im Flash auswerten (schreibt ESP-IDF bei jedem Absturz) und danach loeschen
static void coreDumpLesen(Preferences &p) {
  if (esp_core_dump_image_check() != ESP_OK) return;
  esp_core_dump_summary_t *s = (esp_core_dump_summary_t *)malloc(sizeof(esp_core_dump_summary_t));
  if (s && esp_core_dump_get_summary(s) == ESP_OK) {
    char pc[12], kette[16 * 11 + 1] = "";
    snprintf(pc, sizeof(pc), "0x%08x", (unsigned)s->exc_pc);
    for (uint32_t i = 0; i < s->exc_bt_info.depth && i < 16; i++) {
      char a[12];
      snprintf(a, sizeof(a), "%s0x%08x", i ? " " : "", (unsigned)s->exc_bt_info.bt[i]);
      strlcat(kette, a, sizeof(kette));
    }
    s->exc_task[sizeof(s->exc_task) - 1] = 0;
    p.putString("lzT", s->exc_task);
    p.putString("lzPC", pc);
    p.putString("lzBT", kette);
    p.putUInt("lzU", s->ex_info.exc_cause);
    Serial.printf("Letzter Absturz in '%s' bei %s, Aufrufkette: %s\n", s->exc_task, pc, kette);
  }
  free(s);
  esp_core_dump_image_erase();
}

void diagnoseStart() {
  esp_reset_reason_t r = esp_reset_reason();
  neustartGrund = grundName(r);

  Preferences p;
  p.begin("diag", false);
  String eigen = p.isKey("grund") ? p.getString("grund", "") : "";  // eigener Grund vor einem gewollten Neustart (z. B. Wetter-Aufgabe hing)
  if (eigen.length()) {
    weg(p, "grund");
    static char g[16];
    strlcpy(g, eigen.c_str(), sizeof(g));
    neustartGrund = g;
  }
  bool absturzJetzt = istAbsturz(r) || eigen.length();
  if (absturzJetzt) {
    p.putUInt("abst", p.getUInt("abst", 0) + 1);
    p.putString("lzG", neustartGrund);
    p.putInt("lzB", FIRMWARE_BUILD);
    p.putUInt("lzZ", 0);
    weg(p, "lzT"), weg(p, "lzPC"), weg(p, "lzBT"), weg(p, "lzU"), weg(p, "lzR");
    zeitNachtragen = true;
  }
  coreDumpLesen(p);

  // Laeuft gerade eine neue, noch nicht bestaetigte Firmware?
  const esp_partition_t *laufend = esp_ota_get_running_partition();
  esp_ota_img_states_t zustand;
  probe = esp_ota_get_state_partition(laufend, &zustand) == ESP_OK && zustand == ESP_OTA_IMG_PENDING_VERIFY;

  // Ist der Bootloader gerade von einer neuen Firmware zurueckgesprungen?
  const esp_partition_t *ungueltig = esp_ota_get_last_invalid_partition();
  esp_app_desc_t d;
  if (ungueltig && esp_ota_get_partition_description(ungueltig, &d) == ESP_OK) {
    char kennung[17];
    for (int i = 0; i < 8; i++) snprintf(kennung + 2 * i, 3, "%02x", d.app_elf_sha256[i]);
    if (!p.isKey("rsK") || p.getString("rsK", "") != kennung) {  // dieser Ruecksprung ist neu
      p.putString("rsK", kennung);
      int versuch = p.getInt("vers", 0);
      p.putInt("rsB", versuch);  // 0 = per WLAN hochgeladene eigene Firmware
      p.putUInt("rsZ", 0);
      if (versuch > FIRMWARE_BUILD) p.putInt("kaputt", versuch);
      if (absturzJetzt) p.putInt("lzB", versuch), p.putBool("lzR", true);  // abgestuerzt ist die neue, nicht diese
      weg(p, "vers");
      zeitNachtragen = true;
      if (versuch) Serial.printf("Neue Firmware (Build %d) lief nicht - zurueck auf Build %d\n", versuch, FIRMWARE_BUILD);
      else Serial.printf("Per WLAN hochgeladene Firmware lief nicht - zurueck auf die vorige (Build %d)\n", FIRMWARE_BUILD);
    }
  }
  kaputt = p.getInt("kaputt", 0);
  p.end();

  Serial.printf("Neustart-Grund: %s%s\n", neustartGrund, probe ? " - neue Firmware, wird geprueft" : "");
}

void diagnoseBereit() {
  // Watchdog: laeuft schon fuer die Leerlauf-Aufgabe (5 s); laenger stellen und loop() dazunehmen
  esp_task_wdt_init(WATCHDOG_S, true);
  wdtAn = esp_task_wdt_add(nullptr) == ESP_OK;
}

void diagnoseFuettern() {
  if (wdtAn) esp_task_wdt_reset();
}

void diagnoseLebenszeichen() { wetterLeben = millis() | 1; }

static void neustartMitGrund(const char *grund) {
  Preferences p;
  p.begin("diag", false);
  p.putString("grund", grund);
  p.end();
  Serial.printf("Neustart: %s\n", grund);
  delay(200);
  ESP.restart();
}

void diagnoseLoop(uint32_t t) {
  diagnoseFuettern();

  // Neue Firmware laeuft stabil und hat WLAN -> behalten
  if (probe && t > PROBEZEIT && WiFi.status() == WL_CONNECTED) {
    probe = false;
    esp_ota_mark_app_valid_cancel_rollback();
    Preferences p;
    p.begin("diag", false);
    weg(p, "vers");
    if (kaputt && kaputt <= FIRMWARE_BUILD) weg(p, "kaputt"), kaputt = 0;  // ein neuerer Build hat es geschafft
    p.end();
    Serial.println("Neue Firmware laeuft - bestaetigt");
  }

  // Uhrzeit fuer Absturz / Ruecksprung nachtragen, sobald sie bekannt ist (ungefaehr: Zeitpunkt des Starts)
  if (zeitNachtragen && zeitBekannt()) {
    zeitNachtragen = false;
    uint32_t start = time(nullptr) - t / 1000;
    Preferences p;
    p.begin("diag", false);
    if (p.isKey("lzZ") && !p.getUInt("lzZ", 1)) p.putUInt("lzZ", start);
    if (p.isKey("rsZ") && !p.getUInt("rsZ", 1)) p.putUInt("rsZ", start);
    p.end();
  }

  // mit Vorzeichen rechnen: das Lebenszeichen (millis() | 1, andere Aufgabe) kann ein, zwei ms "vor" der
  // hier gelesenen Uhr liegen - ohne Vorzeichen liefe die Differenz ueber und loeste faelschlich einen Neustart aus
  uint32_t leben = wetterLeben;
  if (leben && (int32_t)(millis() - leben) > (int32_t)WETTER_STILL) neustartMitGrund("wetter");
}

void diagnoseUpdateVersuch(int build) {
  Preferences p;
  p.begin("diag", false);
  if (build) p.putInt("vers", build);
  else weg(p, "vers");
  p.end();
}

int diagnoseKaputterBuild() { return kaputt; }

void diagnoseLoeschen() {
  Preferences p;
  p.begin("diag", false);
  const char *schluessel[] = {"abst", "lzG", "lzB", "lzZ", "lzT", "lzPC", "lzBT", "lzU", "lzR", "rsB", "rsZ", "kaputt"};
  for (const char *k : schluessel) weg(p, k);
  p.end();
  kaputt = 0;
}

static String text(const String &s) {
  String aus = "\"";
  for (char c : s)
    if (c != '"' && c != '\\' && (uint8_t)c >= 0x20) aus += c;
  return aus + "\"";
}

// Text lesen, ohne dass Preferences fuer einen fehlenden Schluessel einen Fehler ausgibt
static String lies(Preferences &p, const char *k) { return p.isKey(k) ? p.getString(k, "") : String(); }

String diagnoseJson() {
  Preferences p;
  p.begin("diag", true);
  uint32_t leben = wetterLeben, t = millis();
  int32_t still = leben ? max((int32_t)(t - leben), (int32_t)0) : 0;  // mit Vorzeichen, siehe diagnoseLoop
  String j = "{\"laufzeit\":" + String(t / 1000) + ",\"neustart\":" + text(neustartGrund) +
             ",\"pruefen\":" + (probe ? "true" : "false") + ",\"heap\":" + ESP.getFreeHeap() +
             ",\"heapMin\":" + ESP.getMinFreeHeap() + ",\"psram\":" + ESP.getFreePsram() +
             ",\"wetterAlter\":" + (leben ? String(still / 1000) : String("null")) +
             ",\"abstuerze\":" + p.getUInt("abst", 0) + ",\"kaputt\":" + kaputt;
  if (p.isKey("lzG"))
    j += ",\"absturz\":{\"grund\":" + text(lies(p, "lzG")) + ",\"build\":" + p.getInt("lzB", 0) +
         ",\"zeit\":" + p.getUInt("lzZ", 0) +
         ",\"imUpdate\":" + (p.getBool("lzR", false) ? "true" : "false") + ",\"aufgabe\":" + text(lies(p, "lzT")) +
         ",\"pc\":" + text(lies(p, "lzPC")) + ",\"kette\":" + text(lies(p, "lzBT")) + "}";
  if (p.isKey("rsZ"))
    j += ",\"ruecksprung\":{\"build\":" + String(p.getInt("rsB", 0)) + ",\"zeit\":" + p.getUInt("rsZ", 0) + "}";
  p.end();
  return j + "}";
}
