#include "bilder.h"

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
    case 0:  return tag ? "Sonnig" : "Klar";
    case 1:  return "Heiter";
    case 2:  return "Teils bewölkt";
    case 3:  return "Bedeckt";
    case 45: return "Nebel";
    case 48: return "Reifnebel";
    case 51: case 53: case 55: return "Nieselregen";
    case 56: case 57: return "Eisniesel";
    case 61: return "Leichter Regen";
    case 63: return "Regen";
    case 65: return "Starkregen";
    case 66: case 67: return "Eisregen";
    case 71: return "Leichter Schnee";
    case 73: return "Schnee";
    case 75: return "Starker Schnee";
    case 77: return "Schneegriesel";
    case 80: case 81: return "Regenschauer";
    case 82: return "Starke Schauer";
    case 85: case 86: return "Schneeschauer";
    case 95: return "Gewitter";
    case 96: case 99: return "Gewitter, Hagel";
  }
  return "Unbekannt";
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

void zeichneHimmel(TFT_eSprite &s, Bild b, bool tag, uint32_t t, bool blitz) {
  RGB oben, unten;
  verlauf(b, tag, oben, unten);
  int w = s.width(), h = s.height();
  for (int y = 0; y < h; y++) {
    uint16_t c = mische(oben, unten, y / float(h - 1));
    if (blitz) c = s.alphaBlend(110, TFT_WHITE, c);
    s.drawFastHLine(0, y, w, c);
  }

  // Sterne, die funkeln (nur nachts und wenn nicht zu bewoelkt)
  if (tag || !(b == KLAR || b == HEITER)) return;
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
}

// ------------------------------------------------------------
// Bausteine
// ------------------------------------------------------------

static void sonne(TFT_eSprite &s, float cx, float cy, float g, uint32_t t) {
  float r = g * 0.2f;
  uint16_t gelb = farbe(255, 198, 44);
  uint16_t hg = s.readPixel(cx, cy);
  float puls = 0.5f + 0.5f * sinf(t * 0.0025f);

  // Schein
  s.fillSmoothCircle(cx, cy, r * (1.55f + 0.12f * puls), s.alphaBlend(45 + 25 * puls, gelb, hg));

  // Strahlen drehen sich langsam und werden mal laenger, mal kuerzer
  float dreh = t * 0.0004f;
  float r1 = r * 1.4f;
  for (int i = 0; i < 8; i++) {
    float a = dreh + i * PI / 4;
    float l = g * (0.09f + 0.03f * sinf(t * 0.004f + i * 1.3f));
    s.drawWideLine(cx + cosf(a) * r1, cy + sinf(a) * r1, cx + cosf(a) * (r1 + l), cy + sinf(a) * (r1 + l),
                   max(2.0f, g * 0.045f), gelb);
  }
  s.fillSmoothCircle(cx, cy, r, gelb);
  s.fillSmoothCircle(cx - r * 0.25f, cy - r * 0.25f, r * 0.55f, farbe(255, 224, 110));
}

static void mond(TFT_eSprite &s, float cx, float cy, float g, uint32_t t) {
  float r = g * 0.21f;
  uint16_t hell = farbe(238, 234, 210);
  uint16_t hg = s.readPixel(cx, cy);
  float puls = 0.5f + 0.5f * sinf(t * 0.0018f);
  s.fillSmoothCircle(cx, cy, r * (1.5f + 0.1f * puls), s.alphaBlend(35 + 20 * puls, hell, hg));
  s.fillSmoothCircle(cx, cy, r, hell);
  uint16_t krater = farbe(212, 207, 182);
  s.fillSmoothCircle(cx - r * 0.35f, cy - r * 0.2f, r * 0.22f, krater);
  s.fillSmoothCircle(cx + r * 0.32f, cy + r * 0.3f, r * 0.16f, krater);
  s.fillSmoothCircle(cx + r * 0.12f, cy - r * 0.48f, r * 0.1f, krater);
}

// Wolke: cy ist die Oberkante des flachen Unterteils
static void wolkeForm(TFT_eSprite &s, float cx, float cy, float w, uint16_t c) {
  s.fillSmoothRoundRect(cx - w * 0.45f, cy, w * 0.9f, w * 0.2f, w * 0.1f, c);
  s.fillSmoothCircle(cx - w * 0.24f, cy + w * 0.02f, w * 0.18f, c);
  s.fillSmoothCircle(cx - w * 0.01f, cy - w * 0.08f, w * 0.26f, c);
  s.fillSmoothCircle(cx + w * 0.25f, cy + w * 0.03f, w * 0.17f, c);
}

static void wolke(TFT_eSprite &s, float cx, float cy, float w, RGB c) {
  wolkeForm(s, cx + w * 0.03f, cy + w * 0.05f, w, farbe(dunkler(c, 0.72f)));  // Schatten unten
  wolkeForm(s, cx, cy, w, farbe(c));
}

static void blitzZeichen(TFT_eSprite &s, float cx, float cy, float g, uint16_t c) {
  s.fillTriangle(cx + g * 0.04f, cy, cx - g * 0.09f, cy + g * 0.2f, cx + g * 0.01f, cy + g * 0.2f, c);
  s.fillTriangle(cx - g * 0.03f, cy + g * 0.17f, cx + g * 0.08f, cy + g * 0.17f, cx - g * 0.07f, cy + g * 0.4f, c);
}

// Fallende Teilchen (Regen oder Schnee), blenden am Ende aus
static void teilchen(TFT_eSprite &s, float x0, float breite, float y0, float hoehe, int n, uint32_t periode,
                     float g, bool schnee, uint16_t c, uint32_t t) {
  for (int i = 0; i < n; i++) {
    uint32_t ph = (t + uint32_t(i * periode * 0.37f)) % periode;
    float p = ph / float(periode);
    float x = x0 + breite * (i + 0.5f) / n + sinf(i * 12.9f) * breite * 0.04f;
    float y = y0 + p * hoehe;
    uint8_t a = p > 0.75f ? (1 - p) / 0.25f * 255 : 255;
    if (schnee) {
      x += sinf(t * 0.002f + i * 2.1f) * g * 0.05f;
      s.fillSmoothCircle(x, y, max(1.0f, g * 0.03f), s.alphaBlend(a, c, s.readPixel(x, y)));
    } else {
      float l = g * (n > 6 ? 0.11f : 0.07f);
      s.drawWideLine(x, y, x - l * 0.3f, y + l, max(1.5f, g * 0.03f), s.alphaBlend(a, c, s.readPixel(x, y)));
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
      wolke(s, cx - g * 0.08f + d, cy + g * 0.12f, g * 0.74f, WEISS);
      break;

    case BEDECKT:
      wolke(s, cx + g * 0.18f - d, cy - g * 0.04f, g * 0.62f, {176, 186, 198});
      wolke(s, cx - g * 0.06f + d, cy + g * 0.08f, g * 0.8f, WEISS);
      break;

    case NEBEL:
      wolke(s, cx + d, cy - g * 0.1f, g * 0.66f, {205, 210, 216});
      for (int k = 0; k < 3; k++) {
        float bw = g * (0.74f - k * 0.08f);
        float x = cx + sinf(t * 0.0009f + k * 1.3f) * g * 0.07f;
        float y = cy + g * (0.14f + k * 0.1f);
        float h = max(3.0f, g * 0.05f);
        s.fillSmoothRoundRect(x - bw / 2, y, bw, h, h / 2, farbe(220, 224, 228));
      }
      break;

    case NIESEL:
      teilchen(s, cx - g * 0.3f, g * 0.6f, cy + g * 0.04f, g * 0.34f, 5, 1100, g, false, farbe(150, 200, 255), t);
      wolke(s, cx + d, cy - g * 0.1f, g * 0.8f, {214, 220, 228});
      break;

    case REGEN:
      teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.38f, 8, 600, g, false, farbe(120, 180, 255), t);
      wolke(s, cx + d, cy - g * 0.12f, g * 0.84f, {188, 196, 210});
      break;

    case SCHNEE:
      teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.4f, 7, 2400, g, true, TFT_WHITE, t);
      wolke(s, cx + d, cy - g * 0.12f, g * 0.8f, {228, 234, 242});
      break;

    case GEWITTER: {
      bool hell = blitzAktiv(t);
      teilchen(s, cx - g * 0.32f, g * 0.64f, cy + g * 0.02f, g * 0.38f, 5, 520, g, false, farbe(110, 160, 230), t);
      wolke(s, cx + g * 0.16f - d, cy - g * 0.2f, g * 0.6f, {92, 96, 116});
      wolke(s, cx - g * 0.04f + d, cy - g * 0.1f, g * 0.84f, {140, 146, 166});
      blitzZeichen(s, cx + g * 0.02f, cy + g * 0.06f, g, hell ? farbe(255, 255, 210) : farbe(255, 206, 60));
      break;
    }
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
  if (p < 0.03f || p > 0.97f) return "Neumond";
  if (p < 0.22f) return "Junge Sichel";
  if (p < 0.28f) return "Erstes Viertel";
  if (p < 0.47f) return "Zunehmend";
  if (p < 0.53f) return "Vollmond";
  if (p < 0.72f) return "Abnehmend";
  if (p < 0.78f) return "Letztes Viertel";
  return "Alte Sichel";
}

void zeichneMondphase(TFT_eSprite &s, int cx, int cy, int r, float phase) {
  uint16_t hell = farbe(238, 234, 210);
  uint16_t hg = s.readPixel(cx, cy);
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
// Text
// ------------------------------------------------------------

// Wandelt UTF-8-Umlaute in Grundbuchstaben um und merkt sich, wo Punkte hin muessen.
// gross[i] = 1 fuer Ä Ö Ü (Punkte hoeher)
static int ohneUmlaute(const char *text, char *aus, int maxLen, int *stellen, uint8_t *gross, int maxStellen) {
  int n = 0, k = 0;
  for (const uint8_t *p = (const uint8_t *)text; *p && n < maxLen - 2; p++) {
    if (*p == 0xC3 && p[1]) {
      uint8_t c = *++p;
      char basis = 0;
      bool g = false;
      switch (c) {
        case 0xA4: basis = 'a'; break;
        case 0xB6: basis = 'o'; break;
        case 0xBC: basis = 'u'; break;
        case 0x84: basis = 'A'; g = true; break;
        case 0x96: basis = 'O'; g = true; break;
        case 0x9C: basis = 'U'; g = true; break;
        case 0x9F: aus[n++] = 's'; aus[n++] = 's'; continue;
      }
      if (!basis) continue;
      if (k < maxStellen) { stellen[k] = n; gross[k] = g; k++; }
      aus[n++] = basis;
    } else {
      aus[n++] = *p;
    }
  }
  aus[n] = 0;
  return k;
}

int breite(TFT_eSprite &s, const char *text) {
  char klar[64];
  int st[8];
  uint8_t gr[8];
  ohneUmlaute(text, klar, sizeof(klar), st, gr, 8);
  return s.textWidth(klar);
}

static void textMitSchatten(TFT_eSprite &s, const char *text, int x, int y, uint16_t f) {
  s.setTextDatum(L_BASELINE);
  s.setTextColor(TFT_BLACK);
  s.drawString(text, x + 1, y + 1);
  s.setTextColor(f);
  s.drawString(text, x, y);
}

void schreibe(TFT_eSprite &s, const char *text, int x, int y, uint16_t f, Ausrichtung a) {
  char klar[64];
  int st[8];
  uint8_t gr[8];
  int k = ohneUmlaute(text, klar, sizeof(klar), st, gr, 8);
  int w = s.textWidth(klar);
  if (a == MITTE) x -= w / 2;
  else if (a == RECHTS) x -= w;
  textMitSchatten(s, klar, x, y, f);

  // Punkte ueber die Umlaute setzen
  int fh = s.fontHeight();
  int dp = max(1, fh / 11);
  for (int i = 0; i < k; i++) {
    char vorne[64];
    memcpy(vorne, klar, st[i]);
    vorne[st[i]] = 0;
    char eins[2] = {klar[st[i]], 0};
    int x0 = x + s.textWidth(vorne);
    int bw = s.textWidth(eins);
    int py = y - int(fh * (gr[i] ? 0.8f : 0.58f));
    for (int j = 0; j < 2; j++) {
      int px = x0 + int(bw * (j ? 0.68f : 0.3f)) - dp / 2;
      s.fillRect(px + 1, py + 1, dp, dp, TFT_BLACK);
      s.fillRect(px, py, dp, dp, f);
    }
  }
}

void schreibeTemp(TFT_eSprite &s, float wert, int x, int y, uint16_t f, Ausrichtung a) {
  char zahl[8];
  snprintf(zahl, sizeof(zahl), "%d", (int)lroundf(wert));
  int fh = s.fontHeight();
  int r = max(2, fh / 14);
  int wz = s.textWidth(zahl);
  int w = wz + 2 * r + 3;
  if (a == MITTE) x -= w / 2;
  else if (a == RECHTS) x -= w;
  textMitSchatten(s, zahl, x, y, f);

  // Gradzeichen als kleiner Ring
  int gx = x + wz + r + 2;
  int gy = y - int(fh * 0.62f) + r;
  for (int i = 0; i < max(1, r / 2); i++) {
    s.drawCircle(gx + 1, gy + 1, r - i, TFT_BLACK);
  }
  for (int i = 0; i < max(1, r / 2); i++) {
    s.drawCircle(gx, gy, r - i, f);
  }
}
