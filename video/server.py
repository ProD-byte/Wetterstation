"""Kleiner Server zum Rendern des Trailers.

Liefert trailer.html, die Board-Fotos und sim.html (= ../simulation/wetterstation.html mit HTML-Kopf)
und speichert die Bilder, die video.html?render (oder trailer.html?render) per POST schickt,
nach %TEMP%/wetterstation-video/bilder/00000.jpg ...

Aufruf: python server.py   (dann http://127.0.0.1:8795/trailer.html?render oeffnen)
"""
import base64
import http.server
import pathlib
import tempfile
import urllib.parse

HIER = pathlib.Path(__file__).parent
# Einzelbilder ausserhalb des Projekts (der Ordner wird von Bitrix24 synchronisiert - tausende Dateien)
BILDER = pathlib.Path(tempfile.gettempdir()) / "wetterstation-video" / "bilder"
BILDER.mkdir(parents=True, exist_ok=True)
SIM = (HIER.parent / "simulation" / "wetterstation.html").read_text(encoding="utf-8")


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=str(HIER), **k)

    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path.startswith("/sim.html"):
            daten = ("<!doctype html><meta charset=utf-8>" + SIM).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(daten)))
            self.end_headers()
            self.wfile.write(daten)
            return
        super().do_GET()

    def do_POST(self):
        url = urllib.parse.urlparse(self.path)
        laenge = int(self.headers.get("Content-Length", 0))
        daten = self.rfile.read(laenge)
        if url.path == "/bild":
            n = int(urllib.parse.parse_qs(url.query)["n"][0])
            if daten.startswith(b"data:"):  # "data:image/jpeg;base64,..."
                daten = base64.b64decode(daten.split(b",", 1)[1])
            (BILDER / f"{n:05d}.jpg").write_bytes(daten)
        elif url.path == "/fertig":
            (BILDER / "fertig.txt").write_text("ok")
        self.send_response(204)
        self.end_headers()


http.server.ThreadingHTTPServer(("127.0.0.1", 8795), Handler).serve_forever()
