#pragma once
#include <Arduino.h>

// Einstellungsseite unter http://wetterstation.local (im eigenen WLAN).
// Holt den Stand von /api/status und schickt Aenderungen sofort an die Wetterstation.
static const char WEB_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0d12">
<title>Wetterstation</title>
<style>
:root{--bg:#0b0d12;--card:#151922;--card2:#1d222d;--line:#262c39;--text:#e8ebf1;--muted:#8b93a5;--accent:#f0a030;--accent2:#e5642e;--ok:#2fd27a;--bad:#ff5468}
@media (prefers-color-scheme:light){:root{--bg:#f3f5f9;--card:#fff;--card2:#f0f2f7;--line:#e2e6ee;--text:#151922;--muted:#647089}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
svg{fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.wrap{max-width:520px;margin:auto;padding:28px 16px 40px;display:grid;gap:16px}
.brand{display:flex;align-items:center;gap:14px}
.logo{width:48px;height:48px;border-radius:14px;flex:none;background:linear-gradient(135deg,var(--accent),var(--accent2));display:grid;place-items:center;color:#fff}
.logo svg{width:26px;height:26px}
h1{font-size:22px;margin:0}
.sub{color:var(--muted);font-size:14px;margin:2px 0 0}
.card{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:18px}
h2{margin:0 0 12px;font-size:13px;font-weight:650;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.zeile{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid var(--line)}
.zeile:first-of-type{border-top:0}
.zeile span{min-width:0}
.zeile small{display:block;color:var(--muted);font-size:13px}
.seg{display:flex;gap:6px;flex-wrap:wrap}
.seg button,.btn{font:inherit;font-size:14px;font-weight:600;padding:9px 13px;border-radius:12px;border:1px solid var(--line);background:var(--card2);color:var(--text);cursor:pointer}
.seg button.an{background:var(--accent);border-color:var(--accent);color:#1a1206}
.btn.haupt{background:linear-gradient(135deg,var(--accent),var(--accent2));border:0;color:#fff}
.btn:disabled{opacity:.5}
.schalter{position:relative;width:48px;height:28px;flex:none}
.schalter input{opacity:0;width:0;height:0}
.schalter i{position:absolute;inset:0;border-radius:99px;background:var(--card2);border:1px solid var(--line);transition:.2s}
.schalter i:before{content:"";position:absolute;width:20px;height:20px;left:3px;top:3px;border-radius:50%;background:var(--muted);transition:.2s}
.schalter input:checked+i{background:var(--accent);border-color:var(--accent)}
.schalter input:checked+i:before{transform:translateX(20px);background:#fff}
select{font:inherit;font-size:15px;padding:8px 10px;border-radius:12px;border:1px solid var(--line);background:var(--card2);color:var(--text)}
.ort{display:flex;align-items:center;gap:12px;padding:12px;border-radius:14px;background:var(--card2);font-weight:650}
.ort svg{width:20px;height:20px;color:var(--accent);flex:none}
.ort small{display:block;font-weight:500;color:var(--muted);font-size:13px}
.suche{display:flex;gap:8px;margin-top:10px}
.suche input{flex:1;min-width:0;font:inherit;padding:10px 12px;border-radius:12px;border:1px solid var(--line);background:var(--card2);color:var(--text)}
.treffer button{display:block;width:100%;text-align:left;margin-top:6px;padding:10px 12px;border-radius:12px;border:1px solid var(--line);background:none;color:inherit;font:inherit;cursor:pointer}
.treffer small{color:var(--muted)}
.leise{color:var(--muted);font-size:13px;margin:8px 0 0}
.meldung{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);padding:10px 16px;border-radius:12px;background:var(--ok);color:#06210f;font-weight:650;opacity:0;transition:opacity .3s;pointer-events:none}
.meldung.zeigen{opacity:1}
</style>
</head>
<body>
<div class="wrap">
  <div class="brand">
    <div class="logo"><svg viewBox="0 0 24 24"><circle cx="15" cy="8" r="3"/><path d="M15 2v1M20.2 4.8l-.7.7M22 9h-1M7 20h10a4 4 0 0 0 0-8 5 5 0 0 0-9.6 1.2A3.5 3.5 0 0 0 7 20z"/></svg></div>
    <div><h1>Wetterstation</h1><p class="sub" id="kopf">verbinde …</p></div>
  </div>

  <div class="card">
    <h2>Standort</h2>
    <div class="ort"><svg viewBox="0 0 24 24"><path d="M12 22s7-6.2 7-12a7 7 0 0 0-14 0c0 5.8 7 12 7 12z"/><circle cx="12" cy="10" r="2.5"/></svg><span><span id="ortName">…</span><small id="ortArt"></small></span></div>
    <form class="suche" id="sucheForm"><input id="suche" placeholder="Ort oder Postleitzahl" autocomplete="off"><button class="btn">Suchen</button></form>
    <div class="treffer" id="treffer"></div>
    <p class="leise"><a href="#" id="ortAuto" style="color:var(--accent)">Ort wieder automatisch über den Internetanschluss bestimmen</a></p>
  </div>

  <div class="card">
    <h2>Anzeige</h2>
    <div class="zeile"><span>Helligkeit</span><div class="seg" id="licht"></div></div>
    <div class="zeile"><span>Seiten</span></div>
    <div class="seg" id="seiten" style="margin-bottom:6px"></div>
    <div class="zeile"><span>Weiterblättern</span><select id="wechsel"><option value="0">nur per Taste</option><option value="8">alle 8 s</option><option value="12">alle 12 s</option><option value="20">alle 20 s</option><option value="30">alle 30 s</option><option value="60">jede Minute</option></select></div>
  </div>

  <div class="card">
    <h2>Nachtmodus</h2>
    <div class="zeile"><span>Display nachts dunkel<small>sehr gedimmt, Tasten machen es kurz hell</small></span><label class="schalter"><input type="checkbox" id="nacht"><i></i></label></div>
    <div class="zeile"><span>von</span><select id="von"></select></div>
    <div class="zeile"><span>bis</span><select id="bis"></select></div>
  </div>

  <div class="card">
    <h2>Updates</h2>
    <div class="zeile"><span>Neue Versionen selbst installieren<small>holt sie von GitHub, ohne Computer</small></span><label class="schalter"><input type="checkbox" id="auto"><i></i></label></div>
    <div class="zeile"><span id="updText">…</span><button class="btn" id="updBtn">Jetzt suchen</button></div>
  </div>

  <div class="card">
    <h2>WLAN</h2>
    <div class="zeile"><span id="wlanText">…</span><button class="btn" id="wlanBtn">Neu einrichten</button></div>
    <p class="leise">Startet neu und öffnet das WLAN „Wetterstation-Setup“ – dann wie beim ersten Mal mit dem Handy einrichten.</p>
  </div>
</div>
<div class="meldung" id="meldung">Gespeichert</div>
<script>
const $ = s => document.querySelector(s);
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const SEITEN = ['Jetzt', '24 Stunden', 'Nächste Tage', 'Sonne & Mond', 'Luft & Pollen'];
const LICHT = ['100 %', '55 %', '25 %', '6 %'];
let st = null;

function melden(t) { const m = $('#meldung'); m.textContent = t || 'Gespeichert'; m.classList.add('zeigen'); setTimeout(() => m.classList.remove('zeigen'), 1500); }
async function senden(pfad, daten) {
  const r = await fetch(pfad, {method: 'POST', body: new URLSearchParams(daten || {})});
  return r.ok;
}

function zeigen() {
  const e = st.einst;
  $('#kopf').textContent = `Build ${st.build || 'eigener'} · ${st.ip}`;
  $('#ortName').textContent = st.ort;
  $('#ortArt').textContent = st.gewaehlt ? 'selbst gewählt' : 'ungefähr ermittelt über den Internetanschluss';
  $('#licht').innerHTML = LICHT.map((t, i) => `<button data-i="${i}" class="${i === e.licht ? 'an' : ''}">${t}</button>`).join('');
  $('#seiten').innerHTML = SEITEN.map((t, i) => `<button data-i="${i}" class="${e.seiten >> i & 1 ? 'an' : ''}">${t}</button>`).join('');
  $('#wechsel').value = e.wechsel;
  $('#nacht').checked = e.nacht; $('#von').value = e.von; $('#bis').value = e.bis;
  $('#auto').checked = e.auto;
  $('#updText').innerHTML = !st.build ? 'Selbst gebaute Firmware – keine Updates von GitHub'
    : st.neu ? `<b>Build ${st.neu}</b> ist verfügbar` : `Aktuell (Build ${st.build})`;
  $('#updBtn').textContent = st.neu ? 'Installieren' : 'Jetzt suchen';
  $('#updBtn').disabled = !st.build;
  $('#wlanText').innerHTML = `${esc(st.wlan)}<small>Signal ${st.rssi} dBm</small>`;
  document.querySelectorAll('#licht button').forEach(b => b.onclick = () => speichern({licht: +b.dataset.i}));
  document.querySelectorAll('#seiten button').forEach(b => b.onclick = () => {
    const s = st.einst.seiten ^ (1 << b.dataset.i);
    if (!s) return melden('Mindestens eine Seite');
    speichern({seiten: s});
  });
}

async function laden() { st = await (await fetch('/api/status')).json(); zeigen(); }
async function speichern(teil) {
  Object.assign(st.einst, teil);
  zeigen();
  if (await senden('/api/einstellungen', st.einst)) melden();
}

for (let h = 0; h < 24; h++) { const o = `<option value="${h}">${String(h).padStart(2, '0')}:00</option>`; $('#von').insertAdjacentHTML('beforeend', o); $('#bis').insertAdjacentHTML('beforeend', o); }
$('#wechsel').onchange = e => speichern({wechsel: +e.target.value});
$('#nacht').onchange = e => speichern({nacht: e.target.checked ? 1 : 0});
$('#von').onchange = e => speichern({von: +e.target.value});
$('#bis').onchange = e => speichern({bis: +e.target.value});
$('#auto').onchange = e => speichern({auto: e.target.checked ? 1 : 0});

$('#sucheForm').onsubmit = async e => {
  e.preventDefault();
  const q = $('#suche').value.trim();
  if (q.length < 2) return;
  $('#treffer').innerHTML = '<p class="leise">suche …</p>';
  const liste = await (await fetch('/orte?q=' + encodeURIComponent(q))).json();
  $('#treffer').innerHTML = liste.length ? liste.map((o, i) => `<button data-i="${i}">${esc(o.name)} <small>${esc(o.info)}</small></button>`).join('') : '<p class="leise">Nichts gefunden</p>';
  document.querySelectorAll('#treffer button').forEach(b => b.onclick = async () => {
    const o = liste[b.dataset.i];
    await senden('/api/ort', {name: o.name, lat: o.lat, lon: o.lon});
    $('#treffer').innerHTML = ''; $('#suche').value = '';
    melden('Ort gespeichert'); laden();
  });
};
$('#ortAuto').onclick = async e => { e.preventDefault(); await senden('/api/ort-auto'); melden('Ort wird neu bestimmt'); setTimeout(laden, 2500); };
$('#updBtn').onclick = async () => {
  if (st.neu) { if (await senden('/api/update/installieren')) melden('Update startet – siehe Display'); }
  else { await senden('/api/update/suchen'); melden('Suche …'); setTimeout(laden, 4000); }
};
$('#wlanBtn').onclick = async () => { if (confirm('WLAN neu einrichten? Die Wetterstation startet neu.')) { await senden('/api/wlan'); melden('Startet neu'); } };
laden();
</script>
</body>
</html>)rawliteral";
