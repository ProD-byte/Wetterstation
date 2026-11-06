"""Musik fuer Trailer + Tutorial: ruhiger, moderner Elektronik-Track (100 BPM, 61 Takte = 146,4 s).

Komplett synthetisch erzeugt (keine Samples) - darf frei im Video verwendet werden.
Akkorde Am - F - C - G, Schluss als Kadenz F - G - C. Die Taktgrenzen sind die Szenenwechsel im Video:
Takt 0-1 Intro, 2-13 Teaser (kraeftig), 14 Uebergang, 15-56 Tutorial (ruhiger), 57-58 Ausklang, 59-60 Schlussakkord.

Kurzfassung fuer das Update-Video (26 Takte = 62,4 s): Takt 0-1 Intro, 2-5 Strophe, 6-13 Refrain, 14 Pause,
15-21 Refrain, 22-23 Ausklang, 24-25 Schlussakkord.

Aufruf: python musik.py ausgabe.wav            (Trailer + Tutorial)
        python musik.py ausgabe.wav update     (Update-Video)
"""
import sys
import wave
import numpy as np

SR = 44100
BPM = 100
BEAT = 60 / BPM          # 0,6 s
BAR = 4 * BEAT           # 2,4 s
KURZ = len(sys.argv) > 2 and sys.argv[2] == "update"  # Kurzfassung fuers Update-Video
TAKTE = 26 if KURZ else 61
AUSKLANG = 22 if KURZ else 57  # ab hier Abspann: ruhige Kadenz F - G
SCHLUSS = 24 if KURZ else 59  # Takt mit dem Schlussakkord (klingt natuerlich aus)
LAENGE = TAKTE * BAR
N = int(LAENGE * SR) + SR
rng = np.random.default_rng(7)

L = np.zeros(N)
R = np.zeros(N)
HALL = np.zeros((2, N))  # Hall-Bus (Send)


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def t_arr(dauer):
    return np.arange(int(dauer * SR)) / SR


def adsr(n, a, d, s, r, sustain_len=None):
    """Huellkurve fuer n Samples (Zeiten in Sekunden)."""
    a, d, r = int(a * SR), int(d * SR), int(r * SR)
    env = np.ones(n) * s
    if a: env[:min(a, n)] = np.linspace(0, 1, a)[:min(a, n)]
    if d and a < n: env[a:min(a + d, n)] = np.linspace(1, s, d)[:max(0, min(a + d, n) - a)]
    if r: env[max(0, n - r):] *= np.linspace(1, 0, min(r, n))
    return env


def mix(sig_l, sig_r, start, hall=0.0):
    i = int(start * SR)
    n = min(len(sig_l), N - i)
    if n <= 0: return
    L[i:i + n] += sig_l[:n]
    R[i:i + n] += sig_r[:n]
    if hall:
        HALL[0, i:i + n] += sig_l[:n] * hall
        HALL[1, i:i + n] += sig_r[:n] * hall


def takt(b, beat=0.0):
    return b * BAR + beat * BEAT


# ---------- Akkorde ----------
AM, F, C, G = [57, 60, 64], [53, 57, 60], [48, 52, 55, 59], [55, 59, 62]
PROG = [AM, F, C, G]
WURZEL = {id(AM): 45, id(F): 41, id(C): 48, id(G): 43}


def akkord(b):
    if b >= SCHLUSS: return [48, 55, 64, 67, 72]  # Schluss: offenes C-Dur
    if b >= AUSKLANG: return [F, G][b - AUSKLANG]
    return PROG[b % 4]


def wurzel(b):
    if b >= SCHLUSS: return 36
    return WURZEL[id(akkord(b))]


# Abschnitte (Takt 0-basiert): Intro 0-1, Strophe 2-5, Refrain 6-13, Pause 14, Tutorial 15-56,
# Ausklang 57-58, Schluss 59-60
def abschnitt(b):
    if b < 2: return "intro"
    if b < 6: return "strophe"
    if b < 14: return "refrain"
    if b < 15: return "pause"
    if b < AUSKLANG: return "refrain" if KURZ else "tutorial"
    if b < SCHLUSS: return "ausklang"
    return "schluss"


# ---------- Instrumente ----------
def pad(midi, dauer, laut):
    t = t_arr(dauer)
    out = []
    for verstimmung in (-0.08, 0.08):  # links/rechts leicht verstimmt (Schwebung)
        f = hz(midi + verstimmung)
        s = np.zeros_like(t)
        for k in range(1, 9):
            s += np.sin(2 * np.pi * f * k * t + k) / k ** 1.4
        out.append(s)
    env = adsr(len(t), 0.6, 0.4, 0.8, 0.9) * laut
    return out[0] * env, out[1] * env


def pad_ende(midi, dauer, laut):
    """Schlussakkord: weicher Anschlag, Obertoene klingen schneller ab als der Grundton (wird dunkler)."""
    t = t_arr(dauer)
    out = []
    for verstimmung in (-0.06, 0.06):
        f = hz(midi + verstimmung)
        s = np.zeros_like(t)
        for k in range(1, 7):
            s += np.sin(2 * np.pi * f * k * t + k) / k ** 1.8 * np.exp(-t * 0.5 * k)
        out.append(s)
    env = np.minimum(1, t / 0.08) * np.exp(-t / 1.9) * laut
    return out[0] * env, out[1] * env


def pluck(midi, laut, hell=1.0):
    t = t_arr(1.2)
    f = hz(midi)
    s = np.zeros_like(t)
    for k in range(1, 7):
        s += np.sin(2 * np.pi * f * k * t) * np.exp(-t * (4 + k * 3 / hell)) / k
    s *= np.minimum(1, t * 400) * laut
    return s


def glocke(midi, laut):
    t = t_arr(2.4)
    f = hz(midi)
    s = (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * f * 2.0 * t) * np.exp(-t * 2)
         + 0.15 * np.sin(2 * np.pi * f * 3.01 * t) * np.exp(-t * 4))
    return s * np.exp(-t * 1.6) * np.minimum(1, t * 300) * laut


def bass(midi, dauer, laut):
    t = t_arr(dauer)
    f = hz(midi)
    s = np.sin(2 * np.pi * f * t) + 0.25 * np.sin(2 * np.pi * 2 * f * t)
    return s * adsr(len(t), 0.01, 0.1, 0.8, 0.08) * laut


def kick(laut):
    t = t_arr(0.45)
    f = 45 + 85 * np.exp(-t * 28)
    phase = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(phase) * np.exp(-t * 7) * laut


def hihat(laut, lang=0.05):
    t = t_arr(lang * 4)
    n = rng.standard_normal(len(t))
    n = np.diff(n, prepend=0)  # Hochpass
    return n * np.exp(-t / lang) * laut


def clap(laut):
    t = t_arr(0.35)
    n = rng.standard_normal(len(t))
    n = n - np.convolve(n, np.ones(8) / 8, "same")  # grob Bandpass
    env = np.exp(-t * 18) + 0.6 * np.exp(-((t - 0.012) ** 2) / 2e-5) + 0.5 * np.exp(-((t - 0.024) ** 2) / 2e-5)
    return n * env * laut


def riser(dauer, laut):
    t = t_arr(dauer)
    n = rng.standard_normal(len(t))
    n = np.diff(n, prepend=0)
    sweep = np.sin(2 * np.pi * np.cumsum(300 + 1500 * (t / dauer) ** 2) / SR) * 0.3
    return (n * 0.5 + sweep) * (t / dauer) ** 2 * laut


def crash(laut):
    t = t_arr(2.5)
    n = np.diff(rng.standard_normal(len(t)), prepend=0)
    return n * np.exp(-t * 1.8) * laut


# ---------- Arrangement ----------
kick_zeiten = []

for b in range(TAKTE):
    a = abschnitt(b)
    ak = akkord(b)
    start = takt(b)

    if b > SCHLUSS: continue  # Schlussakkord klingt bis zum Ende durch

    # Flaechen-Pad (Intro schwillt an); der Schlussakkord klingt von selbst aus
    if a == "schluss":
        for m in ak:
            pl, pr = pad_ende(m, LAENGE - start, 0.11 / len(ak) * 3)
            mix(pl, pr, start, hall=0.6)
    else:
        laut = {"intro": 0.05 + 0.03 * b, "strophe": 0.07, "refrain": 0.075, "pause": 0.09, "tutorial": 0.065,
                "ausklang": 0.06}[a]
        for m in ak:
            pl, pr = pad(m, BAR + (0.3 if b == SCHLUSS - 1 else 0.9), laut / len(ak) * 3)
            mix(pl, pr, start, hall=0.5)

    # Bass (Tutorial/Ausklang: halbe Noten, ruhiger)
    if a in ("strophe", "refrain"):
        for beat in range(4):
            mix(*(bass(wurzel(b), BEAT * 0.9, 0.22),) * 2, takt(b, beat))
    if a in ("tutorial", "ausklang"):
        for beat in (0, 2):
            mix(*(bass(wurzel(b), BEAT * 1.8, 0.18),) * 2, takt(b, beat))
    if a == "schluss":
        bs = bass(wurzel(b), 4.5, 0.22) * np.exp(-t_arr(4.5) * 0.7)
        mix(bs, bs, start)

    # Arpeggio (Strophe Achtel, Refrain Sechzehntel, Pause/Tutorial leiser; ab Schritt 4 Sechzehntel,
    # im Ausklang wieder Achtel und leiser werdend; der letzte Schlag vor dem Schluss bleibt frei)
    if a in ("intro", "strophe", "refrain", "pause", "tutorial", "ausklang"):
        toene = [ak[0] + 12, ak[1] + 12, ak[2] + 12, ak[1] + 24, ak[2] + 12, ak[1] + 12, ak[0] + 24, ak[2] + 12]
        schritt = 0.25 if a == "refrain" or (a == "tutorial" and b >= 41) else 0.5
        laut = {"intro": 0.05 if b == 1 else 0.0, "strophe": 0.07, "refrain": 0.06, "pause": 0.045,
                "tutorial": 0.042 if b >= 41 else 0.055, "ausklang": 0.05 - 0.012 * (b - AUSKLANG)}[a]
        if laut:
            for i in range(int(4 / schritt) - (2 if b == SCHLUSS - 1 else 0)):
                m = toene[i % len(toene)]
                s = pluck(m, laut, hell=1.6 if a == "refrain" else 1.0)
                pan = 0.5 + 0.35 * np.sin(i * 1.3)
                mix(s * (1 - pan) * 2, s * pan * 2, takt(b, i * schritt), hall=0.35)
                # Echo (punktierte Achtel), abwechselnd links/rechts
                for e in range(1, 4):
                    mix(s * 0.35 ** e * (e % 2), s * 0.35 ** e * ((e + 1) % 2), takt(b, i * schritt + e * 0.75), hall=0.2)

    # Glocken-Melodie im Refrain
    # (im Tutorial leise waehrend der Handy-Einrichtung und am Ende vor dem Schluss)
    if a == "refrain" or (a == "tutorial" and (27 <= b < 41 or b >= 53)):
        MELODIE = {0: [76, 72], 1: [69, 72], 2: [67, 72], 3: [74, 71]}
        for i, m in enumerate(MELODIE[b % 4]):
            s = glocke(m, 0.09 if a == "refrain" else 0.055)
            mix(s * 0.9, s * 1.1, takt(b, i * 2), hall=0.6)
    if a == "ausklang":  # Melodie fuehrt zum Grundton hin: A-C ueber F, H-D ueber G
        for i, m in enumerate([[69, 72], [71, 74]][b - AUSKLANG]):
            s = glocke(m, 0.06)
            mix(s * 0.9, s * 1.1, takt(b, i * 2), hall=0.6)
    if a == "schluss":  # langsam aufgefaechertes C-Dur, landet auf dem hohen C
        for beat, m in ((0, 72), (0.5, 76), (1, 79), (2, 84)):
            s = glocke(m, 0.065)
            mix(s * 0.95, s * 1.05, takt(b, beat), hall=0.8)

    # Schlagzeug (im Ausklang nur noch leise, der letzte Schlag vor dem Schluss bleibt frei)
    if a in ("strophe", "refrain", "tutorial", "ausklang"):
        beats = [0, 1, 2, 3] if a == "refrain" else [0, 2]
        for beat in beats:
            k = kick({"refrain": 0.55, "strophe": 0.42, "tutorial": 0.34, "ausklang": 0.28}[a])
            mix(k, k, takt(b, beat))
            kick_zeiten.append(takt(b, beat))
        if a in ("refrain", "tutorial", "ausklang") or b >= 4:
            for i in range(6 if b == SCHLUSS - 1 else 8):
                laut = 0.035 if i % 2 else 0.018
                hh = hihat(laut * {"refrain": 1.0, "strophe": 0.7, "tutorial": 0.55, "ausklang": 0.4}[a])
                mix(hh * 0.8, hh * 1.2, takt(b, i * 0.5))
    if a == "schluss":
        k = kick(0.4)
        mix(k, k, start)
        if a == "refrain":
            for beat in (1, 3):
                cl = clap(0.12)
                mix(cl * 1.1, cl * 0.9, takt(b, beat), hall=0.4)

# Uebergaenge: Riser vor Strophe, Refrain und nach der Pause, Crash auf dem Einsatz
# (der Schluss kommt ohne Rauschen aus, sonst klingt er hart)
for b_ziel, dauer in ((2, BAR), (6, BAR / 2), (15, BAR)):
    rs = riser(dauer, 0.12)
    mix(rs * 0.9, rs * 1.1, takt(b_ziel) - dauer, hall=0.3)
for b_ziel in (2, 6, 15):
    cr = crash(0.08)
    mix(cr * 1.1, cr * 0.9, takt(b_ziel), hall=0.5)
    k = kick(0.6)
    mix(k, k, takt(b_ziel))

# Ducking: Flaeche/Hall atmen mit der Bassdrum (wie Sidechain)
duck = np.ones(N)
for kz in kick_zeiten:
    i = int(kz * SR)
    n = min(int(0.35 * SR), N - i)
    duck[i:i + n] = np.minimum(duck[i:i + n], 1 - 0.45 * np.exp(-np.arange(n) / SR / 0.12))

# Hall: Faltung mit abklingendem Rauschen (2,6 s)
ir_t = t_arr(2.6)
hall_l = np.zeros(N)
hall_r = np.zeros(N)
for kanal, ziel in ((0, hall_l), (1, hall_r)):
    ir = rng.standard_normal(len(ir_t)) * np.exp(-ir_t / 0.55)
    ir = np.convolve(ir, np.ones(6) / 6, "same")  # weicher
    ir /= np.sqrt(np.sum(ir ** 2))
    groesse = 1 << int(np.ceil(np.log2(N + len(ir))))
    erg = np.fft.irfft(np.fft.rfft(HALL[kanal], groesse) * np.fft.rfft(ir, groesse), groesse)[:N]
    ziel += erg

L = L * (0.55 + 0.45 * duck) + hall_l * 0.35 * duck
R = R * (0.55 + 0.45 * duck) + hall_r * 0.35 * duck

# Master: sanft begrenzen, Ein-/Ausblenden, normalisieren
ende = int(LAENGE * SR)
L, R = L[:ende], R[:ende]
for kanal in (L, R):
    kanal[:] = np.tanh(kanal * 1.4) / np.tanh(1.4)
fade_in = int(0.4 * SR)
fade_out = int(1.5 * SR)  # nur der letzte Hallrest, der Akkord klingt selbst aus
for kanal in (L, R):
    kanal[:fade_in] *= np.linspace(0, 1, fade_in)
    kanal[-fade_out:] *= np.linspace(1, 0, fade_out) ** 2
spitze = max(np.abs(L).max(), np.abs(R).max())
L, R = L / spitze * 0.89, R / spitze * 0.89

daten = (np.stack([L, R], axis=1) * 32767).astype(np.int16)
with wave.open(sys.argv[1], "wb") as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(daten.tobytes())
print(f"{sys.argv[1]}: {LAENGE:.1f} s, Spitze vor Normalisierung {spitze:.2f}")
