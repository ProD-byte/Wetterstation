#include "bilder.h"
#include "einstellungen.h"
#include "schriften.h"

struct RGB { uint8_t r, g, b; };

uint16_t farbe(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
static uint16_t farbe(RGB c) { return farbe(c.r, c.g, c.b); }
static uint16_t mische(RGB a, RGB b, float f) {
  return farbe(a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f);
}
static RGB dunkler(RGB c, float f) { return {uint8_t(c.r * f), uint8_t(c.g * f), uint8_t(c.b * f)}; }

// ------------------------------------------------------------
// Wettercodes
// ------------------------------------------------------------

Bild bildFuerCode(int code) {
  if (code == 0) return KLAR;
  if (code <= 2) return HEITER;
  if (code == 3) return BEDECKT;
  if (code == 45 || code == 48) return NEBEL;
  if (code >= 51 && code <= 57) return NIESEL;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return REGEN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return SCHNEE;
  if (code >= 95) return GEWITTER;
  return BEDECKT;
}

const char *textFuerCode(int code, bool tag) {
  switch (code) {
    case 0:  return tag ? TX("Sonnig", "Sunny") : TX("Klar", "Clear");
    case 1:  return TX("Heiter", "Mostly clear");
    case 2:  return TX("Teils bewölkt", "Partly cloudy");
    case 3:  return TX("Bedeckt", "Overcast");
    case 45: return TX("Nebel", "Fog");
    case 48: return TX("Reifnebel", "Freezing fog");
    case 51: case 53: case 55: return TX("Nieselregen", "Drizzle");
    case 56: case 57: return TX("Eisniesel", "Freezing drizzle");
    case 61: return TX("Leichter Regen", "Light rain");
    case 63: return TX("Regen", "Rain");
    case 65: return TX("Starkregen", "Heavy rain");
    case 66: case 67: return TX("Eisregen", "Freezing rain");
    case 71: return TX("Leichter Schnee", "Light snow");
    case 73: return TX("Schnee", "Snow");
    case 75: return TX("Starker Schnee", "Heavy snow");
    case 77: return TX("Schneegriesel", "Snow grains");
    case 80: case 81: return TX("Regenschauer", "Showers");
    case 82: return TX("Starke Schauer", "Heavy showers");
    case 85: case 86: return TX("Schneeschauer", "Snow showers");
    case 95: return TX("Gewitter", "Thunderstorm");
    case 96: case 99: return TX("Gewitter, Hagel", "Storm, hail");
  }
  return TX("Unbekannt", "Unknown");
}

// ------------------------------------------------------------
// Himmel
// ------------------------------------------------------------

static void verlauf(Bild b, bool tag, RGB &oben, RGB &unten) {
  if (!tag) {
    switch (b) {
      case KLAR: case HEITER: oben = {6, 10, 38};  unten = {28, 40, 92}; return;
      case GEWITTER:          oben = {18, 14, 34}; unten = {52, 44, 74}; return;
      default:                oben = {16, 20, 34}; unten = {48, 54, 72}; return;
    }
  }
  switch (b) {
    case KLAR:     oben = {24, 92, 206};  unten = {110, 178, 240}; break;
    case HEITER:   oben = {40, 104, 190}; unten = {126, 174, 222}; break;
    case BEDECKT:  oben = {72, 88, 110};  unten = {132, 144, 160}; break;
    case NEBEL:    oben = {108, 114, 122}; unten = {170, 174, 180}; break;
    case NIESEL:   oben = {62, 80, 104};  unten = {112, 128, 148}; break;
    case REGEN:    oben = {40, 56, 80};   unten = {84, 100, 124}; break;
    case SCHNEE:   oben = {96, 118, 150}; unten = {182, 196, 214}; break;
    case GEWITTER: oben = {30, 26, 52};   unten = {74, 64, 96}; break;
  }
}

// Blitz: zweimal kurz hintereinander, dann Pause
bool blitzAktiv(uint32_t t) {
  uint32_t p = t % 4200;
  return p < 90 || (p > 180 && p < 260);
}

static RGB misch(RGB a, RGB b, float f) {
  return {uint8_t(a.r + (b.r - a.r) * f), uint8_t(a.g + (b.g - a.g) * f), uint8_t(a.b + (b.b - a.b) * f)};
}

static void wolkeForm(TFT_eSprite &s, float cx, float cy, float w, uint16_t c, uint32_t t = 0, bool atmen = false);

// Zufall, der fuer dieselbe Zahl immer gleich ist - so muss fuer Voegel, Sternschnuppen und Tropfen nichts gespeichert werden
static uint32_t zufall(uint32_t n) {
  uint32_t x = (n ^ 61) ^ (n >> 16);
  x *= 9;
  x ^= x >> 4;
  x *= 0x27d4eb2d;
  x ^= x >> 15;
  return x;
}

// Lage (Wind, Temperatur, Monat, Hagel, Regenbogen) - Wind macht die Wolken schneller (bei Sturm jagen sie)
// und den Regen schraeger
static HimmelLage lage;
void himmelLage(const HimmelLage &l) { lage = l; }
static float windFaktor() { return constrain(0.4f + lage.wind / 12, 0.4f, 5.0f); }
static float windSchraeg() { return min(0.9f, 0.15f + lage.wind / 50); }

// Pixel unter (x, y), auch am Rand
static uint16_t unter(TFT_eSprite &s, float x, float y) {
  return s.readPixel(constrain((int)x, 0, s.width() - 1), constrain((int)y, 0, s.height() - 1));
}
// Kreis und Linie, halb durchsichtig (a = 0..1)
static void kreisA(TFT_eSprite &s, float x, float y, float r, uint16_t f, float a) {
  if (a <= 0.01f || r <= 0) return;
  if (x + r < 0 || y + r < 0 || x - r >= s.width() || y - r >= s.height()) return;
  uint16_t c = a >= 0.99f ? f : s.alphaBlend(a * 255, f, unter(s, x, y));
  if (r < 3) s.drawSpot(x, y, r, c);
  else s.fillSmoothCircle(x, y, r, c);
}
static void linieA(TFT_eSprite &s, float ax, float ay, float bx, float by, float d, uint16_t f, float a) {
  if (a <= 0.01f) return;
  s.drawWideLine(ax, ay, bx, by, d, a >= 0.99f ? f : s.alphaBlend(a * 255, f, unter(s, (ax + bx) / 2, (ay + by) / 2)));
}
// kleiner Stern mit vier Zacken, der aufblitzt
static void funkeln(TFT_eSprite &s, float x, float y, float l, uint16_t f, float a) {
  if (l < 0.5f || a <= 0.01f) return;
  linieA(s, x - l, y, x + l, y, 1.5f, f, a);
  linieA(s, x, y - l, x, y + l, 1.5f, f, a);
  linieA(s, x - l * 0.4f, y - l * 0.4f, x + l * 0.4f, y + l * 0.4f, 1, f, a * 0.6f);
  linieA(s, x - l * 0.4f, y + l * 0.4f, x + l * 0.4f, y - l * 0.4f, 1, f, a * 0.6f);
  kreisA(s, x, y, max(1.0f, l * 0.18f), f, a);
}

// Blatt: zwei Keile (spitz - breit - spitz) mit Stiel, wk = Drehung
static void blatt(TFT_eSprite &s, float x, float y, float l, float wk, uint16_t f, float a = 1) {
  float dx = cosf(wk) * l / 2, dy = sinf(wk) * l / 2;
  uint16_t c = a >= 0.99f ? f : s.alphaBlend(a * 255, f, unter(s, x, y));
  s.drawWedgeLine(x - dx, y - dy, x, y, 0.4f, l * 0.28f, c);
  s.drawWedgeLine(x, y, x + dx, y + dy, l * 0.28f, 0.4f, c);
  s.drawWideLine(x - dx, y - dy, x - dx * 1.35f, y - dy * 1.35f, 1, c);
}

// Bluete: fuenf Blaetter um eine gelbe Mitte
static void bluete(TFT_eSprite &s, float x, float y, float r, float dreh) {
  for (int i = 0; i < 5; i++) {
    float wk = dreh + i * 2 * PI / 5;
    kreisA(s, x + cosf(wk) * r * 0.55f, y + sinf(wk) * r * 0.55f, r * 0.5f, farbe(255, 196, 214), 1);
  }
  kreisA(s, x, y, r * 0.3f, farbe(255, 222, 90), 1);
}

// Hagelkoerner: fallen schnell, springen am Boden zweimal auf
static void hagel(TFT_eSprite &s, float x0, float br, float y0, float hoehe, int n, uint32_t periode, float g, uint32_t t) {
  for (int i = 0; i < n; i++) {
    uint32_t z = zufall(i + 31);
    uint32_t per = periode * (0.8f + (z % 100) / 250.0f);
    float p = ((t + uint32_t(i * per * 0.41f)) % per) / float(per);
    float r = max(1.2f, g * 0.028f) * (0.8f + ((z >> 8) % 50) / 100.0f);
    float x = x0 + br * (i + 0.5f) / n, y;
    if (p < 0.6f) {
      y = y0 + hoehe * (p / 0.6f) * (p / 0.6f);
    } else {
      float q = (p - 0.6f) / 0.4f, s1 = q < 0.6f ? q / 0.6f : (q - 0.6f) / 0.4f, hub = q < 0.6f ? g * 0.12f : g * 0.045f;
      y = y0 + hoehe - sinf(s1 * PI) * hub;
      x += (q < 0.6f ? q / 0.6f : 1 + (q - 0.6f) / 0.4f * 0.5f) * g * 0.05f * (i % 2 ? 1 : -1);
    }
    float a = p > 0.9f ? (1 - p) / 0.1f : 1;
    kreisA(s, x, y, r, farbe(236, 244, 255), a);
    kreisA(s, x - r * 0.35f, y - r * 0.35f, r * 0.35f, TFT_WHITE, a);
  }
}

// Hinter dem Inhalt: grosse Wolken ziehen vorbei (schneller bei Wind), Regen/Schnee/Nebel ueber den ganzen Himmel,
// Gewitterblitz, nachts Sternschnuppen, am Tag ab und zu Voegel
static void himmelEffekte(TFT_eSprite &s, Bild b, bool tag, uint32_t t) {
  int w = s.width(), h = s.height();
  auto bei = [&](float x, float y) { return unter(s, x, y); };
  const float wind = windFaktor();

  int wolken = b == HEITER ? 2 : b == KLAR ? 0 : b == NEBEL ? 0 : 4;
  bool dunkel = b == REGEN || b == GEWITTER;
  RGB wf = !tag ? RGB{140, 150, 180} : dunkel ? RGB{150, 160, 185} : RGB{255, 255, 255};
  uint8_t deck = b == HEITER ? 30 : dunkel ? 46 : 40;
  for (int i = 0; i < wolken; i++) {
    float bw = 150 + 40 * (i % 3);
    float tempo = (0.0035f + 0.0012f * i) * wind;  // Pixel pro Millisekunde
    float x = fmodf(t * tempo + i * 157.0f, w + bw * 1.3f) - bw * 0.65f;
    float y = 38 + (i * 47) % (h - 80);
    wolkeForm(s, x, y, bw, s.alphaBlend(deck, farbe(wf.r, wf.g, wf.b), bei(x, y)));
  }

  if (b == REGEN || b == GEWITTER || b == NIESEL) {
    int n = b == NIESEL ? 24 : 48;
    uint32_t periode = b == NIESEL ? 1700 : 850;
    float l = b == NIESEL ? 6 : 13, sch = windSchraeg();
    for (int i = 0; i < n; i++) {
      float p = ((t + i * 7919u) % periode) / float(periode);
      float x = (i * 53) % (w + 60) - p * (h + 30) * sch * 0.6f;
      float y = p * (h + 30) - 20;
      s.drawWideLine(x, y, x - l * sch * 0.6f, y + l, 1, s.alphaBlend(80, farbe(180, 205, 240), bei(x, y)));
    }
  } else if (b == SCHNEE) {
    for (int i = 0; i < 44; i++) {
      float p = ((t + i * 6151u) % 5200u) / 5200.0f;
      float x = (i * 67) % w + sinf(t * 0.0012f + i) * 12 * wind;
      float y = p * (h + 12) - 6;
      s.fillSmoothCircle(x, y, 1 + i % 2, s.alphaBlend(170, TFT_WHITE, bei(x, y)));
    }
  } else if (b == NEBEL) {
    // Nebelbaender treiben quer ueber den Bildschirm (zeilenweise, der Himmel ist pro Zeile einfarbig)
    for (int k = 0; k < 4; k++) {
      int y0 = 26 + k * 38 + sinf(t * 0.0006f + k) * 6;
      int x0 = sinf(t * 0.0003f + k * 2) * 60 - 60;
      for (int y = y0; y < y0 + 24 && y < h; y++) {
        uint8_t a = 50 * sinf(PI * (y - y0) / 24.0f);
        s.drawFastHLine(x0, y, w + 120, s.alphaBlend(a, farbe(235, 238, 242), bei(10, y)));
      }
    }
  }

  // Gewitter: verzweigter Blitz quer ueber den Himmel, solange es blitzt
  if (b == GEWITTER && blitzAktiv(t)) {
    uint32_t z = zufall(t / 4200 + 99);
    float schritt = h * 0.68f / 7;
    float px[8], py[8], ax[4], ay[4];
    px[0] = 40 + z % (w - 80), py[0] = -4;
    for (int i = 1; i <= 7; i++) px[i] = px[i - 1] + (int)(zufall(z + i) % 29) - 14, py[i] = py[i - 1] + schritt;
    ax[0] = px[3], ay[0] = py[3];
    for (int i = 1; i <= 3; i++) ax[i] = ax[i - 1] + 10 + zufall(z + 20 + i) % 9, ay[i] = ay[i - 1] + schritt * 0.7f;
    uint16_t schein = farbe(190, 200, 255);
    for (int i = 1; i <= 7; i++) linieA(s, px[i - 1], py[i - 1], px[i], py[i], 6, schein, 0.25f);
    for (int i = 1; i <= 3; i++) linieA(s, ax[i - 1], ay[i - 1], ax[i], ay[i], 4, schein, 0.25f);
    for (int i = 1; i <= 7; i++) s.drawWideLine(px[i - 1], py[i - 1], px[i], py[i], 1.8f, TFT_WHITE);
    for (int i = 1; i <= 3; i++) s.drawWideLine(ax[i - 1], ay[i - 1], ax[i], ay[i], 1.2f, TFT_WHITE);
  }

  // klare Nacht: ab und zu eine Sternschnuppe
  if (!tag && (b == KLAR || b == HEITER)) {
    uint32_t lokal = t % 7000, z = zufall(t / 7000 + 500);
    if (z % 2 == 0 && lokal < 900) {
      float u = lokal / 900.0f, sx = 80 + z % (w - 80), sy = 10 + (z >> 8) % 50, dist = 150 * u;
      float hx = sx - dist * 0.9f, hy = sy + dist * 0.42f, lang = 45 * sinf(u * PI) + 4;
      float a = min(1.0f, sinf(u * PI) * 1.6f);
      for (int i = 0; i < 6; i++) {
        float f1 = i / 6.0f, f2 = (i + 1) / 6.0f;
        linieA(s, hx + lang * 0.9f * f1, hy - lang * 0.42f * f1, hx + lang * 0.9f * f2, hy - lang * 0.42f * f2,
               2 - f1 * 1.2f, TFT_WHITE, a * (1 - f1));
      }
      kreisA(s, hx, hy, 1.5f, TFT_WHITE, a);
    }
  }

  // Hagel: Koerner prasseln ueber den ganzen Himmel und springen unten auf
  if (b == GEWITTER && lage.hagel) hagel(s, 0, w, -10, h - 4, 22, 900, 60, t);

  // Sturm: Blaetter wirbeln quer durchs Bild (Herbst/Winter bunt, sonst gruen)
  int m = lage.monat;
  bool herbst = m >= 8 && m <= 10;
  static const RGB BUNT[4] = {{230, 120, 40}, {200, 70, 40}, {230, 180, 50}, {150, 90, 40}};
  static const RGB GRUEN[4] = {{90, 170, 70}, {120, 190, 80}, {70, 140, 60}, {150, 200, 90}};
  const RGB *blattF = herbst || m == 11 || (m >= 0 && m <= 1) ? BUNT : GRUEN;
  if (lage.wind >= 40) {
    for (int i = 0; i < 8; i++) {
      uint32_t z = zufall(i + 300), per = 2200 + z % 1600;
      float p = ((t + i * 977u) % per) / float(per);
      float x = -20 + (w + 40) * p, y = 20 + (z >> 8) % (h - 40) + sinf(p * 9 + i) * 14;
      blatt(s, x, y, 9 + z % 4, t * 0.012f * (i % 2 ? 1 : -1) + i, farbe(blattF[i % 4]), 0.95f);
    }
  }
  bool ruhig = b == KLAR || b == HEITER || b == BEDECKT;
  // Herbst: ab und zu segelt ein Blatt herunter
  if (herbst && ruhig && lage.wind < 40) {
    uint32_t lokal = t % 12000, z = zufall(t / 12000 + 1200);
    if (z % 3 != 0 && lokal < 8000) {
      float u = lokal / 8000.0f, x = 30 + z % (w - 60) + sinf(u * 7) * 26, y = -10 + (h + 20) * u;
      blatt(s, x, y, 15, sinf(u * 7) * 0.9f + 0.6f, farbe(blattF[z % 4]));
    }
  }
  // Fruehling am Tag: Blueten treiben schraeg durchs Bild
  if (m >= 2 && m <= 4 && tag && ruhig) {
    for (int j = 0; j < 2; j++) {
      uint32_t tt = t + j * 4500, lokal = tt % 9000, z = zufall((tt / 9000) * 2 + j + 1500);
      if (z % 4 == 0 || lokal > 7500) continue;
      float u = lokal / 7500.0f, x = w + 10 - (w + 40) * u;
      float y = 10 + (z >> 8) % (h / 2) + u * h * 0.5f + sinf(u * 8) * 8;
      bluete(s, x, y, 6, u * 6);
    }
  }
  // warme Sommernacht: Gluehwuermchen schweben und leuchten auf
  if (m >= 5 && m <= 7 && !tag && ruhig && lage.temp >= 15) {
    for (int i = 0; i < 9; i++) {
      uint32_t z = zufall(i + 700);
      float x = w * (0.1f + 0.8f * (z % 100) / 100.0f) + sinf(t * 0.0005f + i * 2) * 24;
      float y = h * (0.45f + 0.45f * ((z >> 8) % 100) / 100.0f) + sinf(t * 0.0007f + i) * 12;
      float g2 = max(0.0f, sinf(t * 0.0021f + i * 1.7f));
      g2 = g2 * g2 * g2;
      uint16_t gelbgruen = farbe(220, 255, 120);
      kreisA(s, x, y, 8, gelbgruen, 0.2f * g2);
      kreisA(s, x, y, 4, gelbgruen, 0.3f * g2);
      kreisA(s, x, y, 1.8f, farbe(240, 255, 170), 0.3f + 0.7f * g2);
    }
  }

  // schoenes Wetter am Tag: ab und zu zieht ein kleiner Vogelschwarm vorbei
  if (tag && (b == KLAR || b == HEITER)) {
    uint32_t lokal = t % 22000, z = zufall(t / 22000 + 900);
    if (z % 3 != 0 && lokal < 9000) {
      float u = lokal / 9000.0f;
      bool rechts = z & 1;
      float x0 = rechts ? -30 + (w + 60) * u : w + 30 - (w + 60) * u;
      float y0 = 34 + (z >> 8) % 50 + sinf(u * 6) * 4, r = rechts ? -1 : 1;
      static const int8_t SCHWARM[3][2] = {{0, 0}, {14, 6}, {26, -3}};
      uint16_t f = farbe(40, 50, 72);
      for (int j = 0; j < 3; j++) {
        float x = x0 + SCHWARM[j][0] * r, y = y0 + SCHWARM[j][1];
        float schlag = sinf(t * 0.014f + j * 1.9f), sp = 5.5f - j * 0.5f;
        linieA(s, x, y, x - sp, y - 2.5f * schlag - 1, 1.5f, f, 0.8f);
        linieA(s, x, y, x + sp, y - 2.5f * schlag - 1, 1.5f, f, 0.8f);
      }
    }
  }
}

void zeichneHimmel(TFT_eSprite &s, Bild b, bool tag, uint32_t t, bool blitz, float daemmerung) {
  RGB oben, unten;
  verlauf(b, tag, oben, unten);
  // Morgen- und Abendrot: oben violett, unten orange-rosa
  if (daemmerung > 0) {
    float d = min(daemmerung, 1.0f) * (b == KLAR || b == HEITER ? 1.0f : 0.45f);
    oben = misch(oben, {72, 52, 118}, d * 0.6f);
    unten = misch(unten, {255, 146, 92}, d * 0.8f);
  }
  int w = s.width(), h = s.height();

  // sanfter Wetterwechsel: aendert sich Wetter oder Tag/Nacht, blendet der Himmel in 4 s ueber
  static bool hwStart = false, hwTag;
  static Bild hwB;
  static uint32_t hwAb;
  static RGB vonO, vonU, jetztO, jetztU;
  if (!hwStart) {
    hwStart = true, hwB = b, hwTag = tag, hwAb = t - 10000, vonO = oben, vonU = unten;
  } else if (hwB != b || hwTag != tag) {
    hwB = b, hwTag = tag, hwAb = t, vonO = jetztO, vonU = jetztU;
  }
  float fw = min(1.0f, (t - hwAb) / 4000.0f);
  if (fw < 1) {
    float fs = fw * fw * (3 - 2 * fw);
    oben = misch(vonO, oben, fs);
    unten = misch(vonU, unten, fs);
  }
  jetztO = oben, jetztU = unten;

  // Regenbogen nach dem Schauer: sieben Baender, Mittelpunkt unter dem Bild rechts (zeilenweise wie die Strahlen)
  const float bogen = tag && (b == KLAR || b == HEITER) ? lage.regenbogen : 0;
  static const RGB BOGEN[7] = {{255, 80, 80}, {255, 160, 60}, {255, 230, 90}, {110, 220, 110}, {90, 160, 255}, {110, 110, 230}, {170, 100, 220}};
  const float bcx = w * 0.7f, bcy = h * 1.25f, br0 = bcy - h * 0.2f;

  // Sonnenstrahlen bei "teils bewoelkt": breite, zarte Lichtbahnen von oben rechts, zeilenweise in den Verlauf gemischt
  const bool strahlen = tag && b == HEITER;
  const int STRAHLEN = 5;
  float strCos[STRAHLEN][2], strA[STRAHLEN];
  const float sx = w * 0.82f, sy = -30;
  if (strahlen) {
    for (int k = 0; k < STRAHLEN; k++) {
      float wk = 1.95f + k * 0.2f + sinf(t * 0.00025f + k) * 0.03f, br = 0.045f + 0.02f * (k % 2);
      strCos[k][0] = cosf(wk - br) / sinf(wk - br);  // x-Versatz je Zeile fuer beide Kanten
      strCos[k][1] = cosf(wk + br) / sinf(wk + br);
      strA[k] = 0.05f + 0.035f * (0.5f + 0.5f * sinf(t * 0.0007f + k * 1.4f));
    }
  }
  const uint16_t STRAHL = farbe(255, 246, 214);

  for (int y = 0; y < h; y++) {
    uint16_t c = mische(oben, unten, y / float(h - 1));
    if (blitz) c = s.alphaBlend(110, TFT_WHITE, c);
    s.drawFastHLine(0, y, w, c);
    if (bogen > 0) {
      float dy = y - bcy;
      for (int i = 0; i < 7; i++) {
        float ra = br0 - i * 3 + 1.6f, ri = br0 - i * 3 - 1.6f;
        if (fabsf(dy) >= ra) continue;
        float xo = sqrtf(ra * ra - dy * dy), xi = fabsf(dy) < ri ? sqrtf(ri * ri - dy * dy) : 0;
        uint16_t f = s.alphaBlend(0.3f * bogen * 255, farbe(BOGEN[i]), c);
        int l1 = bcx - xo, l2 = bcx - xi, r1 = bcx + xi, r2 = bcx + xo;
        if (l2 > l1) s.drawFastHLine(l1, y, l2 - l1, f);
        if (r2 > r1) s.drawFastHLine(r1, y, r2 - r1, f);
      }
    }
    if (!strahlen) continue;
    for (int k = 0; k < STRAHLEN; k++) {
      float xa = sx + strCos[k][0] * (y - sy), xb = sx + strCos[k][1] * (y - sy);
      if (xa > xb) std::swap(xa, xb);
      int a = max(0, (int)xa), e = min(w - 1, (int)xb);
      if (e >= a) s.drawFastHLine(a, y, e - a + 1, s.alphaBlend(strA[k] * 255, STRAHL, c));
    }
  }

  // Sterne, die funkeln (nur nachts und wenn nicht zu bewoelkt)
  if (tag || !(b == KLAR || b == HEITER)) {
    himmelEffekte(s, b, tag, t);
    return;
  }
  uint32_t z = 12345;
  int anzahl = b == KLAR ? 34 : 18;
  for (int i = 0; i < anzahl; i++) {
    z = z * 1103515245u + 12345u;
    int x = (z >> 8) % w;
    z = z * 1103515245u + 12345u;
    int y = (z >> 8) % (h - 20);
    float hell = 0.5f + 0.5f * sinf(t * 0.0015f + i * 1.7f);
    s.drawPixel(x, y, s.alphaBlend(60 + hell * 195, TFT_WHITE, s.readPixel(x, y)));
    if (i % 6 == 0) {  // ein paar grosse Sterne mit Kreuz
      uint8_t a = 30 + hell * 110;
      s.drawPixel(x - 1, y, s.alphaBlend(a, TFT_WHITE, s.readPixel(x - 1, y)));
      s.drawPixel(x + 1, y, s.alphaBlend(a, TFT_WHITE, s.readPixel(x + 1, y)));
      s.drawPixel(x, y - 1, s.alphaBlend(a, TFT_WHITE, s.readPixel(x, y - 1)));
      s.drawPixel(x, y + 1, s.alphaBlend(a, TFT_WHITE, s.readPixel(x, y + 1)));
    }
  }
  himmelEffekte(s, b, tag, t);
}

uint16_t grundFarbe(TFT_eSprite &s, int x, int y) {
  static const int8_t PUNKTE[5][2] = {{0, 0}, {-7, -7}, {7, -7}, {-7, 7}, {7, 7}};
  uint16_t c[5];
  int hell[5];
  for (int i = 0; i < 5; i++) {
    c[i] = s.readPixel(constrain(x + PUNKTE[i][0], 0, s.width() - 1), constrain(y + PUNKTE[i][1], 0, s.height() - 1));
    hell[i] = ((c[i] >> 11) & 31) * 2 + ((c[i] >> 5) & 63) + (c[i] & 31) * 2;  // grob die Helligkeit
  }
  // den Punkt mit der mittleren Helligkeit nehmen
  for (int i = 0; i < 5; i++) {
    int kleiner = 0, gleich = 0;
    for (int j = 0; j < 5; j++) kleiner += hell[j] < hell[i], gleich += hell[j] == hell[i];
    if (kleiner <= 2 && kleiner + gleich > 2) return c[i];
  }
  return c[0];
}

// ------------------------------------------------------------
// Bausteine
// ------------------------------------------------------------

static void sonne(TFT_eSprite &s, float cx, float cy, float g, uint32_t t) {
  float r = g * 0.2f;
  uint16_t gelb = farbe(255, 198, 44), gelbHell = farbe(255, 214, 96);
  uint16_t hg = grundFarbe(s, cx, cy);
  float puls = 0.5f + 0.5f * sinf(t * 0.0025f);

  // Schein in zwei Stufen (aussen ganz zart)
  uint16_t schein = s.alphaBlend(16 + 12 * puls, gelb, hg);
  s.fillSmoothCircle(cx, cy, r * (2.2f + 0.18f * puls), schein);
  s.fillSmoothCircle(cx, cy, r * (1.6f + 0.14f * puls), s.alphaBlend(42 + 26 * puls, gelb, schein));

  // 12 Strahlen als spitze Keile, abwechselnd lang und kurz, drehen sich langsam und atmen
  float dreh = t * 0.0003f, r1 = r * 1.35f;
  for (int i = 0; i < 12; i++) {
    float a = dreh + i * PI / 6;
    bool lang = i % 2 == 0;
    float l = g * (lang ? 0.13f : 0.08f) * (1 + 0.2f * sinf(t * 0.004f + i * 1.3f));
    s.drawWedgeLine(cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * (r1 + l), cy + sinf(a) * (r1 + l),
                    max(1.2f, r * (lang ? 0.2f : 0.14f)), 0.6f, lang ? gelb : gelbHell);
  }
  // Kern mit weichem Verlauf zur hellen Mitte
  s.fillSmoothCircle(cx, cy, r, farbe(255, 176, 30));
  s.fillSmoothCircle(cx - r * 0.08f, cy - r * 0.08f, r * 0.82f, gelb);
  s.fillSmoothCircle(cx - r * 0.2f, cy - r * 0.2f, r * 0.55f, farbe(255, 220, 100));
  s.fillSmoothCircle(cx - r * 0.3f, cy - r * 0.3f, r * 0.25f, farbe(255, 244, 190));
  // alle 3,5 s blitzt oben links ein Glanzstern auf
  float gp = (t % 3500) / 3500.0f;
  if (gp < 0.25f) {
    float a = sinf(gp / 0.25f * PI);
    funkeln(s, cx - r * 0.42f, cy - r * 0.42f, r * 0.75f * a, farbe(255, 255, 235), a);
  }
}

static void mond(TFT_eSprite &s, float cx, float cy, float g, uint32_t t) {
  float r = g * 0.21f;
  uint16_t hell = farbe(238, 234, 210);
  uint16_t hg = grundFarbe(s, cx, cy);
  float puls = 0.5f + 0.5f * sinf(t * 0.0018f);
  uint16_t schein = s.alphaBlend(16 + 10 * puls, hell, hg);
  s.fillSmoothCircle(cx, cy, r * (1.75f + 0.12f * puls), schein);
  s.fillSmoothCircle(cx, cy, r * (1.38f + 0.08f * puls), s.alphaBlend(38 + 22 * puls, hell, schein));
  s.fillSmoothCircle(cx, cy, r, hell);
  s.fillSmoothCircle(cx + r * 0.12f, cy + r * 0.12f, r * 0.86f, farbe(226, 221, 194));  // unten rechts dunkler: wirkt rund
  s.fillSmoothCircle(cx - r * 0.1f, cy - r * 0.1f, r * 0.7f, hell);
  // Krater mit dunklem Rand und hellerem Boden
  static const float KRATER[5][3] = {{-0.38f, -0.2f, 0.22f}, {0.32f, 0.3f, 0.17f}, {0.12f, -0.5f, 0.11f},
                                     {-0.1f, 0.42f, 0.12f}, {0.5f, -0.12f, 0.08f}};
  for (auto &k : KRATER) {
    float kx = cx + k[0] * r, ky = cy + k[1] * r, kr = k[2] * r;
    kreisA(s, kx, ky, kr, farbe(196, 190, 164), 1);
    kreisA(s, kx - kr * 0.2f, ky - kr * 0.2f, kr * 0.7f, farbe(218, 213, 188), 1);
  }
  // drei Sterne daneben funkeln nacheinander
  static const float STERNE[3][2] = {{1.9f, -1.2f}, {-1.75f, -1.45f}, {1.55f, 1.35f}};
  for (int k = 0; k < 3; k++) {
    float a = max(0.0f, sinf(t * 0.0026f + k * 2.1f));
    a *= a;
    funkeln(s, cx + STERNE[k][0] * r, cy + STERNE[k][1] * r, r * 0.32f * a + 0.5f, farbe(255, 252, 230), a);
  }
}

// Wolke: cy ist die Oberkante des flachen Unterteils. Mit atmen werden die Buckel leicht groesser und kleiner.
static void wolkeForm(TFT_eSprite &s, float cx, float cy, float w, uint16_t c, uint32_t t, bool atmen) {
  auto at = [&](int k) { return atmen ? 1 + 0.035f * sinf(t * 0.0013f + k * 1.7f) : 1.0f; };
  s.fillSmoothRoundRect(cx - w * 0.45f, cy, w * 0.9f, w * 0.2f, w * 0.1f, c);
  s.fillSmoothCircle(cx - w * 0.24f, cy + w * 0.02f, w * 0.18f * at(0), c);
  s.fillSmoothCircle(cx - w * 0.01f, cy - w * 0.08f, w * 0.26f * at(1), c);
  s.fillSmoothCircle(cx + w * 0.25f, cy + w * 0.03f, w * 0.17f * at(2), c);
}

static void wolke(TFT_eSprite &s, float cx, float cy, float w, RGB c, uint32_t t) {
  wolkeForm(s, cx + w * 0.03f, cy + w * 0.05f, w, farbe(dunkler(c, 0.72f)), t, true);  // Schatten unten
  wolkeForm(s, cx, cy, w, farbe(c), t, true);
  // Unterseite etwas dunkler, oben Lichtkanten - so wirkt die Wolke plastisch
  s.fillSmoothRoundRect(cx - w * 0.4f, cy + w * 0.09f, w * 0.8f, w * 0.1f, w * 0.05f, farbe(dunkler(c, 0.9f)));
  auto at = [&](int k) { return 1 + 0.05f * sinf(t * 0.0013f + k * 1.7f); };
  uint16_t licht = farbe(misch(c, {255, 255, 255}, 0.45f));
  s.fillSmoothCircle(cx - w * 0.05f, cy - w * 0.15f, w * 0.15f * at(1), licht);
  s.fillSmoothCircle(cx - w * 0.27f, cy - w * 0.04f, w * 0.09f * at(0), licht);
  s.fillSmoothCircle(cx + w * 0.21f, cy - w * 0.03f, w * 0.08f * at(2), farbe(misch(c, {255, 255, 255}, 0.25f)));
  kreisA(s, cx - w * 0.09f, cy - w * 0.2f, w * 0.05f, farbe(misch(c, {255, 255, 255}, 0.7f)), 1);
}

static void blitzZeichen(TFT_eSprite &s, float cx, float cy, float g, uint16_t c, bool leuchtet) {
  if (leuchtet) {  // Leuchten um den Blitz
    uint16_t hg = grundFarbe(s, cx, cy + g * 0.2f);
    uint16_t schein = s.alphaBlend(40, farbe(255, 240, 160), hg);
    s.fillSmoothCircle(cx, cy + g * 0.2f, g * 0.3f, schein);
    s.fillSmoothCircle(cx, cy + g * 0.2f, g * 0.18f, s.alphaBlend(70, farbe(255, 240, 160), schein));
  }
  s.fillTriangle(cx + g * 0.04f, cy, cx - g * 0.09f, cy + g * 0.2f, cx + g * 0.01f, cy + g * 0.2f, c);
  s.fillTriangle(cx - g * 0.03f, cy + g * 0.17f, cx + g * 0.08f, cy + g * 0.17f, cx - g * 0.07f, cy + g * 0.4f, c);
}

// Schneeflocke: sechs Arme, bei groesseren Flocken mit kleinen Zweigen
static void flocke(TFT_eSprite &s, float x, float y, float r, float dreh, uint16_t f, float a, float dicke) {
  for (int k = 0; k < 6; k++) {
    float wk = dreh + k * PI / 3, cw = cosf(wk), sw = sinf(wk);
    linieA(s, x, y, x + cw * r, y + sw * r, dicke, f, a);
    if (r >= 3.5f) {
      float mx = x + cw * r * 0.6f, my = y + sw * r * 0.6f, z = r * 0.32f;
      linieA(s, mx, my, mx + cosf(wk + 0.8f) * z, my + sinf(wk + 0.8f) * z, 1, f, a);
      linieA(s, mx, my, mx + cosf(wk - 0.8f) * z, my + sinf(wk - 0.8f) * z, 1, f, a);
    }
  }
}

// Fallende Teilchen (Regen oder Schnee): jedes mit eigenem Tempo, Regen schraeg nach Wind, unten Spritzer
static void teilchen(TFT_eSprite &s, float x0, float breite, float y0, float hoehe, int n, uint32_t periode,
                     float g, bool schnee, uint16_t c, uint32_t t) {
  float schraeg = schnee ? 0 : windSchraeg();
  for (int i = 0; i < n; i++) {
    uint32_t z = zufall(i + 7);
    uint32_t per = periode * (0.75f + (z % 100) / 200.0f);
    uint32_t ph = (t + uint32_t(i * per * 0.37f)) % per;
    float p = ph / float(per);
    float x = x0 + breite * (i + 0.5f) / n + sinf(i * 12.9f) * breite * 0.04f;
    float y = y0 + p * hoehe;
    float a = p > 0.75f ? (1 - p) / 0.25f : 1;
    if (schnee) {
      x += sinf(t * 0.002f + i * 2.1f) * g * 0.05f;
      float r = max(2.0f, g * 0.045f) * (0.8f + ((z >> 8) % 40) / 100.0f);
      flocke(s, x, y, r, t * 0.0015f + i, c, a, max(1.0f, g * 0.012f));
    } else {
      x -= schraeg * p * hoehe * 0.5f;
      float l = g * (n > 6 ? 0.11f : 0.07f), dicke = max(1.5f, g * 0.03f);
      linieA(s, x, y, x - l * schraeg, y + l, dicke * 0.7f, c, a);
      kreisA(s, x - l * schraeg, y + l, dicke * 0.9f, c, a);
      if (p > 0.8f) {  // Aufprall: flacher Ring und zwei hochspringende Troepfchen
        float q = (p - 0.8f) / 0.2f, yb = y0 + hoehe + l, xb = x - l * schraeg, rx = g * 0.07f * q;
        if (rx >= 1) s.drawEllipse(xb, yb, rx, max(1.0f, rx * 0.32f), s.alphaBlend(180 * (1 - q), c, unter(s, xb, yb)));
        float hub = sinf(q * PI) * g * 0.05f;
        kreisA(s, xb - rx * 0.8f, yb - hub, 1, c, 1 - q);
        kreisA(s, xb + rx * 0.8f, yb - hub * 0.8f, 1, c, 1 - q);
      }
    }
  }
}

// ------------------------------------------------------------
// Wetterbilder
// ------------------------------------------------------------

void zeichneBild(TFT_eSprite &s, Bild b, bool tag, int cx, int cy, int groesse, uint32_t t) {
  float g = groesse;
  float d = sinf(t * 0.0007f) * g * 0.04f;  // Wolken treiben hin und her
  const RGB WEISS = {238, 242, 247};

  switch (b) {
    case KLAR:
      if (tag) sonne(s, cx, cy, g, t);
      else mond(s, cx, cy, g, t);
      break;

    case HEITER:
      if (tag) sonne(s, cx + g * 0.16f, cy - g * 0.14f, g * 0.78f, t);
      else mond(s, cx + g * 0.16f, cy - g * 0.14f, g * 0.8f, t);
      wolke(s, cx - g * 0.08f + d, cy + g * 0.12f, g * 0.74f, WEISS, t);
      break;

    case BEDECKT:
      wolke(s, cx + g * 0.18f - d, cy - g * 0.04f, g * 0.62f, {176, 186, 198}, t + 900);
      wolke(s, cx - g * 0.06f + d, cy + g * 0.08f, g * 0.8f, WEISS, t);
      break;

    case NEBEL:
      wolke(s, cx + d, cy - g * 0.1f, g * 0.66f, {205, 210, 216}, t);
      // weiche Schwaden ziehen gegeneinander und werden mal dichter, mal duenner
      for (int k = 0; k < 4; k++) {
        float bw = g * (0.78f - k * 0.07f);
        float x = cx + sinf(t * 0.0008f * (k % 2 ? -1 : 1) + k * 1.3f) * g * 0.12f;
        float y = cy + g * (0.12f + k * 0.085f);
        float h = max(3.0f, g * 0.06f);
        uint8_t a = 150 + 50 * sinf(t * 0.0011f + k);
        s.fillSmoothRoundRect(x - bw / 2, y, bw, h, h / 2, s.alphaBlend(a, farbe(226, 230, 234), unter(s, x, y + h / 2)));
      }
      break;

    case NIESEL:
      teilchen(s, cx - g * 0.3f, g * 0.6f, cy + g * 0.04f, g * 0.34f, 5, 1100, g, false, farbe(150, 200, 255), t);
      wolke(s, cx + d, cy - g * 0.1f, g * 0.8f, {214, 220, 228}, t);
      break;

    case REGEN:
      teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.38f, 8, 600, g, false, farbe(120, 180, 255), t);
      wolke(s, cx + d, cy - g * 0.12f, g * 0.84f, {188, 196, 210}, t);
      break;

    case SCHNEE:
      teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.4f, 7, 2400, g, true, TFT_WHITE, t);
      wolke(s, cx + d, cy - g * 0.12f, g * 0.8f, {228, 234, 242}, t);
      break;

    case GEWITTER: {
      bool hell = blitzAktiv(t);
      // beim Blitz leuchtet die Wolke von innen
      auto auf = [&](RGB c) { return hell ? misch(c, {236, 236, 255}, 0.5f) : c; };
      if (lage.hagel) hagel(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.06f, g * 0.36f, 6, 800, g, t);
      else teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.38f, 5, 520, g, false, farbe(110, 160, 230), t);
      wolke(s, cx + g * 0.16f - d, cy - g * 0.2f, g * 0.6f, auf({92, 96, 116}), t + 700);
      wolke(s, cx - g * 0.04f + d, cy - g * 0.1f, g * 0.84f, auf({140, 146, 166}), t);
      blitzZeichen(s, cx + g * 0.02f, cy + g * 0.06f, g, hell ? farbe(255, 255, 210) : farbe(255, 206, 60), hell);
      break;
    }
  }
}

// ------------------------------------------------------------
// Hitze und Feuerwerk
// ------------------------------------------------------------

// Hitze ab 30 Grad: die unteren 34 Zeilen verschieben sich um ein Pixel hin und her (direkt im Bildspeicher)
void hitzeFlimmern(TFT_eSprite &s, uint32_t t) {
  if (lage.temp < 30) return;
  uint8_t *pix = (uint8_t *)s.getPointer();
  if (!pix) return;
  int w = s.width(), h = s.height(), bpp = s.getColorDepth() / 8, band = 34;
  if (bpp < 1) return;
  for (int y = h - band; y < h; y++) {
    float st = (y - (h - band)) / float(band);
    int d = lroundf(sinf(t * 0.006f + y * 0.55f) * 1.4f * st);
    if (!d) continue;
    uint8_t *z = pix + (size_t)y * w * bpp;
    if (d > 0) memmove(z + d * bpp, z, (size_t)(w - d) * bpp);
    else memmove(z, z - d * bpp, (size_t)(w + d) * bpp);
  }
}

// Feuerwerk: Raketen steigen auf und zerplatzen in bunte Funken, die fallen, knistern und verglimmen
void feuerwerk(TFT_eSprite &s, float sek, uint32_t jahr) {
  static const RGB FARBEN[6] = {{255, 90, 90}, {255, 200, 70}, {120, 220, 255}, {160, 255, 140}, {230, 130, 255}, {255, 255, 255}};
  int W = s.width(), H = s.height();
  float ms = sek * 1000, groesse = min(W, H) / 170.0f;
  for (int k = 0; k * 650 < 58000; k++) {
    uint32_t z = zufall(k * 7 + 3 + jahr);
    float tau = ms - (k * 650 + z % 400);
    if (tau < 0 || tau > 3200) continue;
    float x0 = W * (0.15f + 0.7f * (z % 1000) / 1000.0f), gipfel = H * (0.16f + 0.26f * ((z >> 10) % 1000) / 1000.0f);
    RGB f = FARBEN[(z >> 20) % 6];
    if (tau < 900) {  // Aufstieg mit Funkenschweif
      float u = tau / 900, y = H + 5 - (H + 5 - gipfel) * (1 - (1 - u) * (1 - u));
      for (int i = 1; i <= 4; i++) kreisA(s, x0 + sinf(i * 2 + tau * 0.03f) * 0.8f, y + i * 4, 1.1f, farbe(255, 220, 160), 0.6f * (1 - i / 5.0f));
      kreisA(s, x0, y, 1.6f, farbe(255, 240, 200), 1);
      continue;
    }
    float tb = (tau - 900) / 1000;
    auto weg = [](float tt) { return (1 - expf(-tt * 2.2f)) / 2.2f; };
    if (tb < 0.12f) kreisA(s, x0, gipfel, 14 * groesse * (1 - tb / 0.12f), farbe(misch(f, {255, 255, 255}, 0.6f)), 0.5f * (1 - tb / 0.12f));
    const int n = 30;
    uint16_t col = farbe(f), kern = farbe(misch(f, {255, 255, 255}, max(0.0f, 0.5f - tb)));
    for (int i = 0; i < n; i++) {
      uint32_t zi = zufall(z + i);
      float wk = i * 2 * PI / n + ((zi % 100) / 100.0f - 0.5f) * 0.25f, v = (40 + zi % 18) * groesse;
      float a = powf(max(0.0f, 1 - tb / 2.3f), 1.5f);
      if (i % 3 == 0 && tb > 1.2f) a *= 0.5f + 0.5f * sinf(tau * 0.05f + i);  // Knistern
      float ta = max(0.0f, tb - 0.08f);
      float x = x0 + cosf(wk) * v * weg(tb), y = gipfel + sinf(wk) * v * weg(tb) + 14 * groesse * tb * tb;
      float xa = x0 + cosf(wk) * v * weg(ta), ya = gipfel + sinf(wk) * v * weg(ta) + 14 * groesse * ta * ta;
      linieA(s, xa, ya, x, y, 1, col, a * 0.5f);
      kreisA(s, x, y, 1.4f, kern, a);
    }
  }
}

// ------------------------------------------------------------
// Scheibe: ab und zu trifft ein Regentropfen oder eine Schneeflocke das Glas
// ------------------------------------------------------------

// Wassertropfen auf Glas: Schatten, Wasser, etwas helleres Inneres (Lichtbrechung), Glanzpunkt
static void glasTropfen(TFT_eSprite &s, float x, float y, float r, float a) {
  kreisA(s, x + 1, y + 1.5f, r, TFT_BLACK, 0.22f * a);
  kreisA(s, x, y, r, farbe(200, 220, 245), 0.38f * a);
  kreisA(s, x - r * 0.12f, y + r * 0.22f, r * 0.62f, TFT_WHITE, 0.14f * a);
  kreisA(s, x - r * 0.38f, y - r * 0.38f, max(1.0f, r * 0.28f), TFT_WHITE, 0.85f * a);
}

// Abrutschen: mal kurz haengen bleiben, dann weiter (steigt nie rueckwaerts), dabei schneller werdend
static float rutschen(float u) {
  const int n = 3;
  float f = u - sinf(2 * PI * n * u) / (2 * PI * n);
  return powf(max(0.0f, f), 1.35f);
}

// Wasserspur hinter dem Tropfen, ein paar kleine Reste bleiben haengen
static void rinnsal(TFT_eSprite &s, float x0, float y0, float y, float r, uint32_t seed) {
  uint16_t spur = farbe(210, 228, 250);
  for (float yy = y0; yy < y - r; yy += 2)
    kreisA(s, x0 + 1.6f * sinf((yy - y0) * 0.09f + seed), yy, r * 0.35f, spur, 0.22f * (0.4f + 0.6f * (yy - y0) / (y - y0 + 1)));
  for (int k = 0; k < 2; k++) {
    float yr = y0 + (zufall(seed + k) % 1000) / 1000.0f * (y - y0 - r);
    if (yr < y - r * 2) glasTropfen(s, x0 + 1.6f * sinf((yr - y0) * 0.09f + seed), yr, 2, 0.8f);
  }
}

void scheibe(TFT_eSprite &s, Bild b, uint32_t t) {
  bool regen = b == REGEN || b == GEWITTER || b == NIESEL;
  if (!regen && b != SCHNEE) return;
  int W = s.width(), H = s.height();
  uint32_t P = regen ? 8000 : 13000;
  int bahnen = regen ? 2 : 1;
  for (int j = 0; j < bahnen; j++) {
    uint32_t tt = t + j * P / 2, lokal = tt % P;
    uint32_t z = zufall((tt / P) * 2 + j + 1);
    if (z % 4 == 0 || (b == NIESEL && z % 2)) continue;  // nicht jedes Mal - "ab und an"
    float x0 = 20 + z % (W - 40), y0 = 18 + (z >> 8) % (int)(H * 0.5f), r = 5 + (z >> 16) % 4;
    uint32_t start = 1500;
    if (!regen) {
      // Schneeflocke: bleibt kleben, schmilzt langsam zu einem Tropfen, der dann herunterlaeuft
      start = 6500;
      if (lokal < start) {
        float rein = min(1.0f, lokal / 300.0f), m = constrain((lokal - 3000.0f) / 3500.0f, 0.0f, 1.0f);
        float L = 11 * (1.3f - 0.3f * rein) * (1 - m), dreh = (z % 100) / 30.0f;
        if (m > 0) glasTropfen(s, x0, y0, 1 + (r - 1) * m, m);
        if (L > 0.6f) flocke(s, x0, y0, L, dreh, TFT_WHITE, 0.95f * (1 - m * 0.6f), 1.3f);
        kreisA(s, x0, y0, 2.2f + m, TFT_WHITE, 0.9f * (1 - m));
        continue;
      }
    } else if (lokal < start) {
      // Aufprall: ein Ring breitet sich aus, der Tropfen waechst hinein und zittert kurz
      float q = min(1.0f, lokal / 200.0f);
      if (q < 1) s.drawSmoothCircle(x0, y0, r * (1 + 2 * q), s.alphaBlend(128 * (1 - q), TFT_WHITE, unter(s, x0, y0 - r * (1 + 2 * q))), unter(s, x0, y0 - r * (1 + 2 * q)));
      glasTropfen(s, x0, y0 + sinf(lokal * 0.02f) * 0.4f * (1 - q), r * (0.6f + 0.4f * q), 1);
      continue;
    }
    float u = min(1.0f, (lokal - start) / float(P - start - 200));
    float y = y0 + (H + 15 - y0) * rutschen(u), x = x0 + 1.6f * sinf((y - y0) * 0.09f + z % 7);
    rinnsal(s, x0, y0, y, r, z % 7);
    glasTropfen(s, x, y, r * (1 + 0.15f * min(1.0f, u * 3)), 1);
  }
}

// ------------------------------------------------------------
// Mond und Warnung
// ------------------------------------------------------------

float mondphase(time_t jetzt) {
  // Bekannter Neumond: 6.1.2000 18:14 UTC, ein Mondmonat dauert 29,53 Tage
  double tage = (jetzt - 947182440.0) / 86400.0;
  double p = fmod(tage / 29.530588853, 1.0);
  return p < 0 ? p + 1 : p;
}

const char *mondName(float p) {
  // kurz genug fuer die rechte Haelfte des Displays
  if (p < 0.03f || p > 0.97f) return TX("Neumond", "New moon");
  if (p < 0.22f) return TX("Junge Sichel", "Waxing crescent");
  if (p < 0.28f) return TX("Erstes Viertel", "First quarter");
  if (p < 0.47f) return TX("Zunehmend", "Waxing gibbous");
  if (p < 0.53f) return TX("Vollmond", "Full moon");
  if (p < 0.72f) return TX("Abnehmend", "Waning gibbous");
  if (p < 0.78f) return TX("Letztes Viertel", "Last quarter");
  return TX("Alte Sichel", "Waning crescent");
}

void zeichneMondphase(TFT_eSprite &s, int cx, int cy, int r, float phase) {
  uint16_t hell = farbe(238, 234, 210);
  uint16_t hg = grundFarbe(s, cx, cy);
  s.fillSmoothCircle(cx, cy, r + 4, s.alphaBlend(30, hell, hg));   // Schein
  s.fillSmoothCircle(cx, cy, r, s.alphaBlend(60, hell, TFT_BLACK)); // dunkle Seite
  // Beleuchteter Teil zeilenweise: zunehmend rechts hell, abnehmend links (Nordhalbkugel)
  float k = cosf(phase * 2 * PI);
  for (int y = -r + 1; y < r; y++) {
    float w = sqrtf(r * r - y * y);
    float a = phase < 0.5f ? w * k : -w;
    float b = phase < 0.5f ? w : -w * k;
    if (b > a) s.drawFastHLine(cx + lroundf(a), cy + y, lroundf(b - a), hell);
  }
  uint16_t krater = farbe(212, 207, 182);
  for (int i = 0; i < 3; i++) {
    static const float kx[] = {-0.35f, 0.32f, 0.12f}, ky[] = {-0.2f, 0.3f, -0.48f}, kr[] = {0.22f, 0.16f, 0.1f};
    int x = cx + kx[i] * r, y = cy + ky[i] * r;
    if (s.readPixel(x, y) == hell) s.fillCircle(x, y, max(1, (int)(kr[i] * r)), krater);
  }
}

void zeichneWarnzeichen(TFT_eSprite &s, int cx, int cy, int g, uint16_t f) {
  int h = g, b = g * 1.15f;
  s.fillTriangle(cx, cy - h / 2, cx - b / 2, cy + h / 2, cx + b / 2, cy + h / 2, f);
  int t = max(1, g / 7);
  s.fillRect(cx - t / 2, cy - h / 6, max(2, t), h / 3, TFT_BLACK);
  s.fillRect(cx - t / 2, cy + h / 4, max(2, t), max(2, t), TFT_BLACK);
}


// ------------------------------------------------------------
// Text: geglaettete Schriften (Lato, src/schriften.h) mit echten Umlauten, ß und °
// ------------------------------------------------------------

static const uint8_t *const SCHRIFT_DATEN[] = {SCHRIFT_K, SCHRIFT_M, SCHRIFT_MF, SCHRIFT_G, SCHRIFT_GF, SCHRIFT_XG, SCHRIFT_RIESIG};
static const uint8_t *geladen = nullptr;

void schrift(TFT_eSprite &s, Schrift welche) {
  const uint8_t *daten = SCHRIFT_DATEN[welche];
  if (daten == geladen) return;  // Laden kostet etwas - nur wenn sich die Schrift aendert
  if (geladen) s.unloadFont();
  s.loadFont(daten);
  geladen = daten;
}

int breite(TFT_eSprite &s, const char *text) { return s.textWidth(text); }

static void textMitSchatten(TFT_eSprite &s, const char *text, int x, int y, uint16_t f) {
  s.setTextDatum(L_BASELINE);
  s.setTextColor(TFT_BLACK);  // nur Vordergrund: Kanten werden mit dem Hintergrund verrechnet
  s.drawString(text, x + 1, y + 1);
  s.setTextColor(f);
  s.drawString(text, x, y);
}

void schreibe(TFT_eSprite &s, const char *text, int x, int y, uint16_t f, Ausrichtung a) {
  int w = s.textWidth(text);
  if (a == MITTE) x -= w / 2;
  else if (a == RECHTS) x -= w;
  textMitSchatten(s, text, x, y, f);
}

void schreibeTemp(TFT_eSprite &s, float wert, int x, int y, uint16_t f, Ausrichtung a) {
  char z[12];
  snprintf(z, sizeof(z), "%d°", (int)lroundf(wert));
  schreibe(s, z, x, y, f, a);
}