"""Erzeugt die echten Seiten (Flasher, Einrichtung am Handy, Einstellungen) mit Beispieldaten
und fotografiert sie mit Chrome (headless) fuer das Tutorial-Video.

Voraussetzung: server.py laeuft (http://127.0.0.1:8795). Aufruf: python seiten.py
Ergebnis: seiten/*.png
"""
import json
import math
import pathlib
import re
import subprocess

HIER = pathlib.Path(__file__).parent
REPO = HIER.parent
AUS = HIER / "seiten"
AUS.mkdir(exist_ok=True)
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
BASIS = "http://127.0.0.1:8795/seiten/"

# dunkles Farbschema erzwingen (die Seiten folgen sonst dem System)
DUNKEL = "<style>html:root{--bg:#0b0d12;--card:#151922;--card2:#1d222d;--line:#262c39;--text:#e8ebf1;--muted:#8b93a5}</style>"


def roh(datei):
    """HTML aus einer C-Header-Datei (R\"rawliteral(...)rawliteral\")."""
    text = (REPO / "src" / datei).read_text(encoding="utf-8")
    return re.search(r'R"rawliteral\((.*)\)rawliteral"', text, re.S).group(1)


def einfuegen(html, vorher="", nachher=""):
    """vorher: vor das Seitenskript, nachher: ans Ende des body."""
    html = html.replace("</head>", DUNKEL + "</head>", 1)
    i = html.rfind("<script>")
    html = html[:i] + vorher + html[i:]
    return html.replace("</body>", nachher + "</body>", 1)


# ---------- Einrichtung am Handy (portal_html.h) ----------
NETZE = [{"ssid": "Home-WiFi", "rssi": -48, "open": False}, {"ssid": "Garden-AP", "rssi": -61, "open": False},
         {"ssid": "FRITZ!Box 7530", "rssi": -70, "open": False}, {"ssid": "Guest", "rssi": -79, "open": True}]
ORT = {"name": "Cottbus", "lat": 51.76, "lon": 14.33, "gewaehlt": False}
TREFFER = [{"name": "Cottbus", "info": "Brandenburg, Germany", "lat": 51.76, "lon": 14.33},
           {"name": "Cottbus-Sachsendorf", "info": "Brandenburg, Germany", "lat": 51.73, "lon": 14.33}]
PORTAL_MOCK = f"""<script>
window.fetch = async (u, o) => {{
  u = String(u); const j = x => new Response(JSON.stringify(x));
  if (u.startsWith('/scan')) return j({json.dumps(NETZE)});
  if (u.startsWith('/status')) return j({{state: 'ok'}});
  if (u.startsWith('/orte')) return j({json.dumps(TREFFER)});
  if (u.startsWith('/ort') && !(o && o.method === 'POST')) return j({json.dumps(ORT)});
  return new Response('ok');
}};
</script>"""
ZUSTAENDE = {
    "liste": "",
    "passwort": "setTimeout(() => { pick(nets[0]); $('#pass').value = 'mysecret'; $('#pass').focus(); }, 200);",
    "ort": "setTimeout(async () => { await ortFragen(); $('#suche').value = 'Cottbus'; $('#sucheForm').requestSubmit(); }, 200);",
    "fertig": "setTimeout(() => waehle(" + json.dumps(TREFFER[0]) + "), 200);",
}
portal = roh("portal_html.h")
for name, js in ZUSTAENDE.items():
    (AUS / f"portal_{name}.html").write_text(einfuegen(portal, PORTAL_MOCK, f"<script>{js}</script>"), encoding="utf-8")

# ---------- Einstellungsseite (web_html.h) ----------
temp = [round(7 + 4 * math.sin((i - 9) / 24 * 2 * math.pi) + 2 * math.sin(i / 50), 1) for i in range(168)]
druck = [round(1012 + 9 * math.sin(i / 40), 1) for i in range(168)]
regen = [round(max(0, 1.4 * math.sin(i / 7) - 0.6), 1) if 60 < i < 110 else 0 for i in range(168)]
STATUS = {"ort": "Cottbus", "gewaehlt": True, "build": 14, "neu": 0, "ip": "192.168.178.65", "wlan": "Home-WiFi",
          "rssi": -52, "einst": {"licht": 0, "seiten": 255, "wechsel": 12, "nacht": True, "von": 22, "bis": 6,
                                 "auto": True, "sprache": 1, "lage": 0, "radarzoom": 0, "warnab": 2,
                                 "name": "wetterstation"}}
ORTE = [{"name": "Berlin", "temp": 10.2}, {"name": "Dresden", "temp": 9.6}]
VERLAUF = {"stunde": 15, "wochentag": 0, "temp": temp, "druck": druck, "regen": regen}
EINST_MOCK = f"""<script>
window.fetch = async (u, o) => {{
  u = String(u); const j = x => new Response(JSON.stringify(x));
  if (u.startsWith('/api/status')) return j({json.dumps(STATUS)});
  if (u.startsWith('/api/orte')) return j({json.dumps(ORTE)});
  if (u.startsWith('/api/verlauf')) return j({json.dumps(VERLAUF)});
  return new Response('ok');
}};
</script>"""
(AUS / "einstellungen.html").write_text(einfuegen(roh("web_html.h"), EINST_MOCK), encoding="utf-8")

# ---------- Flasher (docs/index.html) ----------
flasher = (REPO / "docs" / "index.html").read_text(encoding="utf-8")
flasher = flasher.replace("__VERSION__", "Build 14").replace('src="simulation.html?einbetten"', 'src="/sim.html?einbetten"')
(AUS / "flasher.html").write_text(flasher, encoding="utf-8")


# ---------- fotografieren ----------
def foto(seite, datei, breite, hoehe, faktor=1, extra=""):
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--lang=en-US",
                    "--force-color-profile=srgb", f"--force-device-scale-factor={faktor}",
                    f"--window-size={breite},{hoehe}", "--virtual-time-budget=6000",
                    f"--screenshot={AUS / datei}", BASIS + seite + extra],
                   check=True, capture_output=True)
    print(datei)


for name in ZUSTAENDE:
    foto(f"portal_{name}.html", f"portal_{name}.png", 500, 1040, 2)  # Chrome headless: mind. ~500 px breit
foto("einstellungen.html", "einstellungen.png", 1100, 2900)
foto("flasher.html", "flasher.png", 1100, 2000, 1, "?lang=en")
