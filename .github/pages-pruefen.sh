#!/usr/bin/env bash
# ============================================================
#  Prueft vor dem Veroeffentlichen, ob GitHub Pages richtig eingestellt ist.
#
#  Richtig: Settings -> Pages -> Source = "GitHub Actions".
#  Steht dort "Deploy from a branch", baut GitHub bei JEDEM Push zusaetzlich
#  den ganzen Zweig mit Jekyll ("pages build and deployment"). Wer zuletzt
#  fertig ist, gewinnt: dann zeigt die Seite die README statt des Flashers,
#  und simulation.html, version.json, Manifeste und Firmware fehlen (404) -
#  die Simulation (auch auf daveslab.de/basteln) und die Auto-Updates sind weg.
#
#  Ist das so, meldet dieser Schritt einen deutlichen Fehler im Lauf und
#  wartet, bis der Zweig-Build fuer diesen Commit fertig ist, damit unsere
#  Seite als letzte veroeffentlicht wird. Den Lauf bricht er nicht ab.
#
#  Braucht: GH_TOKEN (github.token mit pages: read und actions: read),
#           GITHUB_REPOSITORY, GITHUB_SHA. Laeuft auch lokal (ohne Token
#           ist die Pages-Einstellung nicht lesbar -> nur Hinweis).
# ============================================================
set -u
API="https://api.github.com/repos/${GITHUB_REPOSITORY}"
AUTH=()
[ -n "${GH_TOKEN:-}" ] && AUTH=(-H "Authorization: Bearer ${GH_TOKEN}")
holen() { curl -sS --max-time 20 -H "Accept: application/vnd.github+json" "${AUTH[@]}" "$1"; }

TYP=$(holen "$API/pages" | python3 -c 'import sys,json
try: print(json.load(sys.stdin).get("build_type") or "unbekannt")
except Exception: print("unbekannt")')
TYP=${TYP:-unbekannt}
echo "Pages-Quelle: $TYP"

if [ "$TYP" = "workflow" ]; then
  echo "In Ordnung (GitHub Actions)."
  exit 0
fi

if [ "$TYP" = "legacy" ]; then
  MELDUNG='GitHub Pages steht auf "Deploy from a branch". Dann veroeffentlicht GitHub bei jedem Push zusaetzlich die README, und die Simulation / der Flasher verschwinden. Bitte unter Settings -> Pages -> Build and deployment -> Source auf "GitHub Actions" umstellen.'
  echo "::error title=GitHub Pages falsch eingestellt::$MELDUNG"
  [ -n "${GITHUB_STEP_SUMMARY:-}" ] && printf '### ⚠️ GitHub Pages falsch eingestellt\n\n%s\n' "$MELDUNG" >> "$GITHUB_STEP_SUMMARY"
else
  echo "::warning title=Pages-Einstellung nicht lesbar::Konnte nicht pruefen, ob GitHub Pages auf \"GitHub Actions\" steht."
fi

# Notbehelf: auf den Zweig-Build fuer diesen Commit warten (hoechstens 10 min),
# damit unsere Seite zuletzt kommt und gewinnt.
for i in $(seq 1 60); do
  OFFEN=$(holen "$API/actions/runs?head_sha=${GITHUB_SHA}&per_page=50" | python3 -c 'import sys,json
try:
    laeufe = json.load(sys.stdin).get("workflow_runs", [])
    print(sum(1 for r in laeufe if r.get("name") == "pages build and deployment" and r.get("status") != "completed"))
except Exception: print(0)')
  if [ "${OFFEN:-0}" = "0" ]; then   # leer = Abfrage kaputt -> nicht sinnlos warten
    echo "Kein Zweig-Build (mehr) offen."
    exit 0
  fi
  echo "Warte auf Zweig-Build ($OFFEN offen) ... $i/60"
  sleep 10
done
echo "::warning::Zweig-Build nach 10 min noch nicht fertig - veroeffentliche trotzdem."
exit 0
