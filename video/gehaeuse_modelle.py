"""Exportiert die Gehaeuse-Teile fuer gehaeuse.html (3D-Animation) nach video/modelle/.

Alle Teile in Einbaulage (Gehaeuse-Koordinaten: X laengs, Y oben, Z Tiefe, Front bei Z = 0),
nur der Standfuss in seinem eigenen System (Z oben). Feinere Rundungen ($fn = 96) als die Druck-STL.
Platine: LILYGOs eigenes Modell (github.com/Xinyuan-LilyGO/T-Display-S3, dimensions/t-display-s3-full.stl)
wird geladen und der Kopf der Binaer-STL repariert (beginnt mit "solid", das verwirrt manche Leser).

Aufruf: python gehaeuse_modelle.py     (OpenSCAD 2021.01 oder neuer, Internet fuer das Board-Modell)
"""
import pathlib
import shutil
import subprocess
import urllib.request

HIER = pathlib.Path(__file__).parent
AUS = HIER / "modelle"
AUS.mkdir(exist_ok=True)
SCAD = HIER.parent / "gehaeuse" / "wetterstation_gehaeuse.scad"
OPENSCAD = shutil.which("openscad") or r"C:\Program Files\OpenSCAD\openscad.com"

for teil in ["front", "deckel_eingebaut", "taste", "staender"]:
    subprocess.run([OPENSCAD, "-o", str(AUS / f"{teil}.stl"), "-D", f'teil="{teil}"', "-D", "$fn=96", str(SCAD)],
                   check=True, capture_output=True)
    print(teil)

board = AUS / "board.stl"
if not board.exists():
    url = "https://raw.githubusercontent.com/Xinyuan-LilyGO/T-Display-S3/main/dimensions/t-display-s3-full.stl"
    daten = urllib.request.urlopen(url).read()
    board.write_bytes(b"BINARY STL".ljust(80) + daten[80:])
    print("board")
