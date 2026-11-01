#pragma once
#include <Arduino.h>

// Einstellungsseite unter http://wetterstation.local (bzw. dem eigenen Namen, im eigenen WLAN), Deutsch/Englisch.
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
    <div><h1 data-t="titel">Wetterstation</h1><p class="sub" id="kopf">…</p></div>
  </div>

  <div class="card">
    <h2 data-t="standort">Standort</h2>
    <div class="ort"><svg viewBox="0 0 24 24"><path d="M12 22s7-6.2 7-12a7 7 0 0 0-14 0c0 5.8 7 12 7 12z"/><circle cx="12" cy="10" r="2.5"/></svg><span><span id="ortName">…</span><small id="ortArt"></small></span></div>
    <form class="suche" id="sucheForm"><input id="suche" autocomplete="off"><button class="btn" data-t="suchen">Suchen</button></form>
    <div class="treffer" id="treffer"></div>
    <p class="leise"><a href="#" id="ortAuto" style="color:var(--accent)" data-t="ortAuto">Ort wieder automatisch über den Internetanschluss bestimmen</a></p>
  </div>

  <div class="card">
    <h2 data-t="orteTitel">Weitere Orte</h2>
    <div id="orteListe"></div>
    <form class="suche" id="orteForm"><input id="orteSuche" autocomplete="off"><button class="btn" data-t="hinzu">Hinzufügen</button></form>
    <div class="treffer" id="orteTreffer"></div>
    <p class="leise" data-t="orteHinweis">Bis zu 3 Orte, die die Station auf einer eigenen Seite mit deinem Ort vergleicht.</p>
  </div>

  <div class="card">
    <h2 data-t="verlaufTitel">Letzte 7 Tage</h2>
    <canvas id="kurve" style="width:100%;height:180px;display:block"></canvas>
    <p class="leise"><span style="color:#ffb44a">■</span> <span data-t="legT">Temperatur</span> &nbsp; <span style="color:#8cc8ff">■</span> <span data-t="legD">Luftdruck</span> &nbsp; <span style="color:#6eaaff">▮</span> <span data-t="legR">Regen</span></p>
  </div>

  <div class="card">
    <h2 data-t="anzeige">Anzeige</h2>
    <div class="zeile"><span data-t="sprache">Sprache</span><div class="seg" id="sprache"><button data-s="0">Deutsch</button><button data-s="1">English</button></div></div>
    <div class="zeile"><span data-t="helligkeit">Helligkeit</span><div class="seg" id="licht"></div></div>
    <div class="zeile"><span data-t="seiten">Seiten</span></div>
    <div class="seg" id="seiten" style="margin-bottom:6px"></div>
    <div class="zeile"><span data-t="blaettern">Weiterblättern</span><select id="wechsel"></select></div>
    <div class="zeile"><span><span data-t="lage">Ausrichtung</span><small data-t="lageKlein">wie das Board steht – das Bild dreht sich mit</small></span></div>
    <div class="seg" id="lage" style="margin-bottom:6px"></div>
    <div class="zeile"><span><span data-t="radar">Regenradar</span><small id="radarKlein"></small></span><div class="seg" id="radarzoom"></div></div>
    <div class="zeile"><span><span data-t="warnTitel">Unwetterwarnungen</span><small data-t="warnKlein">vom Deutschen Wetterdienst, nur in Deutschland</small></span><div class="seg" id="warnab"></div></div>
  </div>

  <div class="card">
    <h2 data-t="nameTitel">Name im Netz</h2>
    <form class="suche" id="nameForm"><input id="name" autocomplete="off" maxlength="24" spellcheck="false"><button class="btn" data-t="speichern">Speichern</button></form>
    <p class="leise" data-t="nameHinweis">Mehrere Wetterstationen im selben WLAN brauchen verschiedene Namen, z. B. „wetterstation-kueche“. Die Station ist dann unter diesem Namen mit „.local“ erreichbar.</p>
  </div>

  <div class="card">
    <h2 data-t="nachtmodus">Nachtmodus</h2>
    <div class="zeile"><span><span data-t="nachtDunkel">Display nachts dunkel</span><small data-t="nachtKlein">sehr gedimmt, Tasten machen es kurz hell</small></span><label class="schalter"><input type="checkbox" id="nacht"><i></i></label></div>
    <div class="zeile"><span data-t="von">von</span><select id="von"></select></div>
    <div class="zeile"><span data-t="bis">bis</span><select id="bis"></select></div>
  </div>

  <div class="card">
    <h2>Updates</h2>
    <div class="zeile"><span><span data-t="autoUpd">Neue Versionen selbst installieren</span><small data-t="autoKlein">holt sie von GitHub, ohne Computer</small></span><label class="schalter"><input type="checkbox" id="auto"><i></i></label></div>
    <div class="zeile"><span id="updText">…</span><button class="btn" id="updBtn">…</button></div>
  </div>

  <div class="card">
    <h2 data-t="wlan">WLAN</h2>
    <div class="zeile"><span id="wlanText">…</span><button class="btn" id="wlanBtn" data-t="neuEinrichten">Neu einrichten</button></div>
    <p class="leise" data-t="wlanHinweis">Startet neu und öffnet das WLAN „Wetterstation-Setup“ – dann wie beim ersten Mal mit dem Handy einrichten.</p>
  </div>
</div>
<div class="meldung" id="meldung"></div>
<script>
const $ = s => document.querySelector(s);
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const EN = {
  titel: 'Weather station', standort: 'Location', suchen: 'Search', ortAuto: 'Detect location automatically again via your internet connection',
  orteTitel: 'More places', hinzu: 'Add', orteHinweis: 'Up to 3 places the station compares with yours on a page of its own.',
  verlaufTitel: 'Last 7 days', legT: 'Temperature', legD: 'Air pressure', legR: 'Rain',
  anzeige: 'Display', sprache: 'Language', helligkeit: 'Brightness', seiten: 'Pages', blaettern: 'Next page',
  radar: 'Rain radar', warnTitel: 'Weather warnings', warnKlein: 'from the German Weather Service, Germany only',
  nameTitel: 'Name on the network', speichern: 'Save',
  nameHinweis: 'Several weather stations on the same Wi-Fi need different names, e.g. “wetterstation-kitchen”. The station can then be reached under this name with “.local”.',
  lage: 'Orientation', lageKlein: 'how the board stands – the picture turns with it',
  nachtmodus: 'Night mode', nachtDunkel: 'Dark display at night', nachtKlein: 'strongly dimmed, buttons light it up briefly',
  von: 'from', bis: 'until', autoUpd: 'Install new versions by itself', autoKlein: 'gets them from GitHub, no computer needed',
  wlan: 'Wi-Fi', neuEinrichten: 'Set up again', wlanHinweis: 'Restarts and opens the Wi-Fi “Wetterstation-Setup” – then set it up with your phone like the first time.'
};
const DE = {};
document.querySelectorAll('[data-t]').forEach(e => DE[e.dataset.t] = e.textContent);
let st = null;
const en = () => st && st.einst.sprache == 1;
const L = (de, en_) => en() ? en_ : de;

function melden(t) { const m = $('#meldung'); m.textContent = t || L('Gespeichert', 'Saved'); m.classList.add('zeigen'); setTimeout(() => m.classList.remove('zeigen'), 1500); }
async function senden(pfad, daten) {
  const r = await fetch(pfad, {method: 'POST', body: new URLSearchParams(daten || {})});
  return r.ok;
}
// Ortssuche laeuft auf der Station im Hintergrund: nachfragen, bis das Ergebnis da ist
async function ortSuche(q) {
  for (let i = 0; i < 60; i++) {
    const r = await fetch('/orte?q=' + encodeURIComponent(q));
    if (r.status === 200) return r.json();
    await new Promise(f => setTimeout(f, 400));
  }
  return [];
}

function zeigen() {
  const e = st.einst;
  document.documentElement.lang = en() ? 'en' : 'de';
  document.querySelectorAll('[data-t]').forEach(x => x.textContent = (en() ? EN : DE)[x.dataset.t]);
  document.querySelectorAll('#sprache button').forEach(b => b.classList.toggle('an', b.dataset.s == e.sprache));
  $('#suche').placeholder = L('Ort oder Postleitzahl', 'Town or postcode');
  $('#orteSuche').placeholder = L('Weiteren Ort suchen', 'Search another place');
  $('#ortName').textContent = st.ort;
  $('#ortArt').textContent = st.gewaehlt ? L('selbst gewählt', 'chosen by you') : L('ungefähr ermittelt über den Internetanschluss', 'roughly detected via your internet connection');
  const LICHT = ['100 %', '55 %', '25 %', '6 %'];
  $('#licht').innerHTML = LICHT.map((t, i) => `<button data-i="${i}" class="${i === e.licht ? 'an' : ''}">${t}</button>`).join('');
  const SEITEN = L(['Jetzt', '24 Stunden', 'Nächste Tage', 'Sonne & Mond', 'Luft & Pollen', 'Regenradar', 'Letzte 7 Tage', 'Orte'],
                   ['Now', '24 hours', 'Next days', 'Sun & Moon', 'Air & Pollen', 'Rain radar', 'Last 7 days', 'Places']);
  $('#seiten').innerHTML = SEITEN.map((t, i) => `<button data-i="${i}" class="${e.seiten >> i & 1 ? 'an' : ''}">${esc(t)}</button>`).join('');
  const WECHSEL = [[0, L('nur per Taste', 'button only')], [8, '8 s'], [12, '12 s'], [20, '20 s'], [30, '30 s'], [60, L('1 Minute', '1 minute')]];
  $('#wechsel').innerHTML = WECHSEL.map(([v, t]) => `<option value="${v}">${t}</option>`).join('');
  $('#wechsel').value = e.wechsel;
  $('#nacht').checked = e.nacht; $('#von').value = e.von; $('#bis').value = e.bis;
  $('#auto').checked = e.auto;
  const LAGE = L(['Quer, USB rechts', 'Quer, USB links', 'Hochkant, USB unten', 'Hochkant, USB oben'],
                 ['Landscape, USB right', 'Landscape, USB left', 'Portrait, USB down', 'Portrait, USB up']);
  $('#lage').innerHTML = LAGE.map((t, i) => `<button data-i="${i}" class="${i === e.lage ? 'an' : ''}">${t}</button>`).join('');
  document.querySelectorAll('#lage button').forEach(b => b.onclick = () => speichern({lage: +b.dataset.i}));
  const ZOOM = [[L('Nah', 'Near'), '120 × 55 km'], [L('Mittel', 'Medium'), '240 × 110 km'], [L('Weit', 'Far'), '480 × 220 km']];
  $('#radarzoom').innerHTML = ZOOM.map(([t], i) => `<button data-i="${i}" class="${i === e.radarzoom ? 'an' : ''}">${t}</button>`).join('');
  $('#radarKlein').textContent = L('Ausschnitt etwa ', 'area about ') + ZOOM[e.radarzoom || 0][1];
  const WARN = [[1, L('Alle', 'All')], [2, L('Ab Orange', 'Orange+')], [3, L('Ab Rot', 'Red+')], [5, L('Aus', 'Off')]];
  $('#warnab').innerHTML = WARN.map(([v, t]) => `<button data-i="${v}" class="${v === e.warnab ? 'an' : ''}">${t}</button>`).join('');
  document.querySelectorAll('#warnab button').forEach(b => b.onclick = () => speichern({warnab: +b.dataset.i}));
  if (document.activeElement !== $('#name')) $('#name').value = e.name;
  $('#kopf').textContent = `${e.name}.local · Build ${st.build || L('eigener', 'own')} · ${st.ip}`;
  document.querySelectorAll('#radarzoom button').forEach(b => b.onclick = () => { speichern({radarzoom: +b.dataset.i}); melden(L('Radar wird neu geladen', 'Reloading radar')); });
  $('#updText').innerHTML = !st.build ? L('Selbst gebaute Firmware – keine Updates von GitHub', 'Self-built firmware – no updates from GitHub')
    : st.neu ? `<b>Build ${st.neu}</b> ${L('ist verfügbar', 'is available')}` : `${L('Aktuell', 'Up to date')} (Build ${st.build})`;
  $('#updBtn').textContent = st.neu ? L('Installieren', 'Install') : L('Jetzt suchen', 'Check now');
  $('#updBtn').disabled = !st.build;
  $('#wlanText').innerHTML = `${esc(st.wlan)}<small>${L('Signal', 'Signal')} ${st.rssi} dBm</small>`;
  document.querySelectorAll('#licht button').forEach(b => b.onclick = () => speichern({licht: +b.dataset.i}));
  document.querySelectorAll('#seiten button').forEach(b => b.onclick = () => {
    const s = st.einst.seiten ^ (1 << b.dataset.i);
    if (!s) return melden(L('Mindestens eine Seite', 'At least one page'));
    speichern({seiten: s});
  });
}

async function laden() { st = await (await fetch('/api/status')).json(); zeigen(); orteLaden(); kurveLaden(); }

// --- Weitere Orte ---
async function orteLaden() {
  const liste = await (await fetch('/api/orte')).json();
  $('#orteListe').innerHTML = liste.map((o, i) => `<div class="zeile"><span>${esc(o.name)}<small>${o.temp === null ? '…' : Math.round(o.temp) + '°'}</small></span><button class="btn" data-i="${i}">✕</button></div>`).join('')
    || `<p class="leise">${L('Noch keine weiteren Orte.', 'No other places yet.')}</p>`;
  document.querySelectorAll('#orteListe button').forEach(b => b.onclick = async () => { await senden('/api/orte/weg', {i: b.dataset.i}); orteLaden(); });
  $('#orteForm').hidden = liste.length >= 3;
}
$('#orteForm').onsubmit = async e => {
  e.preventDefault();
  const q = $('#orteSuche').value.trim();
  if (q.length < 2) return;
  $('#orteTreffer').innerHTML = `<p class="leise">${L('suche …', 'searching …')}</p>`;
  const liste = await ortSuche(q);
  $('#orteTreffer').innerHTML = liste.length ? liste.map((o, i) => `<button data-i="${i}">${esc(o.name)} <small>${esc(o.info)}</small></button>`).join('') : `<p class="leise">${L('Nichts gefunden', 'Nothing found')}</p>`;
  document.querySelectorAll('#orteTreffer button').forEach(b => b.onclick = async () => {
    const o = liste[b.dataset.i];
    await senden('/api/orte/neu', {name: o.name, lat: o.lat, lon: o.lon});
    $('#orteTreffer').innerHTML = ''; $('#orteSuche').value = '';
    melden(L('Ort hinzugefügt', 'Place added')); setTimeout(orteLaden, 3000); orteLaden();
  });
};

// --- Verlauf als Diagramm ---
async function kurveLaden() {
  const v = await (await fetch('/api/verlauf')).json();
  const cv = $('#kurve'), dpr = devicePixelRatio || 1, W = cv.clientWidth, H = 180;
  cv.width = W * dpr; cv.height = H * dpr;
  const c = cv.getContext('2d'); c.scale(dpr, dpr);
  const n = v.temp.length; if (n < 2) return;
  const T = v.temp.filter(x => x !== null), D = v.druck.filter(x => x !== null);
  let tmin = Math.min(...T), tmax = Math.max(...T), dmin = Math.min(...D), dmax = Math.max(...D);
  if (tmax - tmin < 4) { tmin -= 2; tmax += 2; } if (dmax - dmin < 10) { dmin -= 5; dmax += 5; }
  const X0 = 30, X1 = W - 40, Y0 = 10, Y1 = 130, RB = 150;
  const px = i => X0 + (X1 - X0) * i / (n - 1), pt = x => Y1 - (x - tmin) / (tmax - tmin) * (Y1 - Y0), pd = x => Y1 - (x - dmin) / (dmax - dmin) * (Y1 - Y0);
  const muted = getComputedStyle(document.body).getPropertyValue('--muted');
  c.font = '11px system-ui'; c.fillStyle = muted; c.strokeStyle = muted; c.globalAlpha = .35;
  const WT = L(['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa'], ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat']);
  for (let d = 0; d <= 8; d++) {
    const a = Math.max(0, d * 24 - v.stunde), e = Math.min(n - 1, (d + 1) * 24 - v.stunde - 1);
    if (a >= n) break;
    if (a > 0) { c.beginPath(); c.moveTo(px(a), Y0); c.lineTo(px(a), RB); c.stroke(); }
    if (e - a >= 8) { c.globalAlpha = 1; c.textAlign = 'center'; c.fillText(WT[(v.wochentag + d) % 7], (px(a) + px(e)) / 2, H - 6); c.globalAlpha = .35; }
  }
  c.globalAlpha = 1;
  c.fillStyle = '#6eaaff';
  v.regen.forEach((r, i) => { const h = Math.min(20, r * 8); if (h > 0) c.fillRect(px(i) - 1, RB - h, 2, h); });
  const linie = (werte, f, farbe, breite) => { c.strokeStyle = farbe; c.lineWidth = breite; c.beginPath(); let an = false;
    werte.forEach((x, i) => { if (x === null) { an = false; return; } an ? c.lineTo(px(i), f(x)) : c.moveTo(px(i), f(x)); an = true; }); c.stroke(); };
  linie(v.druck, pd, '#8cc8ff', 1.5);
  linie(v.temp, pt, '#ffb44a', 2.5);
  c.fillStyle = '#ffb44a'; c.textAlign = 'left';
  c.fillText(Math.round(tmax) + '°', 2, Y0 + 8); c.fillText(Math.round(tmin) + '°', 2, Y1);
  c.fillStyle = '#8cc8ff'; c.textAlign = 'right';
  c.fillText(Math.round(dmax) + ' hPa', W, Y0 + 8); c.fillText(Math.round(dmin), W, Y1);
}
async function speichern(teil) {
  Object.assign(st.einst, teil);
  zeigen();
  if (await senden('/api/einstellungen', st.einst)) melden();
}

for (let h = 0; h < 24; h++) { const o = `<option value="${h}">${String(h).padStart(2, '0')}:00</option>`; $('#von').insertAdjacentHTML('beforeend', o); $('#bis').insertAdjacentHTML('beforeend', o); }
document.querySelectorAll('#sprache button').forEach(b => b.onclick = () => speichern({sprache: +b.dataset.s}));
$('#wechsel').onchange = e => speichern({wechsel: +e.target.value});
$('#nacht').onchange = e => speichern({nacht: e.target.checked ? 1 : 0});
$('#von').onchange = e => speichern({von: +e.target.value});
$('#bis').onchange = e => speichern({bis: +e.target.value});
$('#auto').onchange = e => speichern({auto: e.target.checked ? 1 : 0});

$('#sucheForm').onsubmit = async e => {
  e.preventDefault();
  const q = $('#suche').value.trim();
  if (q.length < 2) return;
  $('#treffer').innerHTML = `<p class="leise">${L('suche …', 'searching …')}</p>`;
  const liste = await ortSuche(q);
  $('#treffer').innerHTML = liste.length ? liste.map((o, i) => `<button data-i="${i}">${esc(o.name)} <small>${esc(o.info)}</small></button>`).join('') : `<p class="leise">${L('Nichts gefunden', 'Nothing found')}</p>`;
  document.querySelectorAll('#treffer button').forEach(b => b.onclick = async () => {
    const o = liste[b.dataset.i];
    await senden('/api/ort', {name: o.name, lat: o.lat, lon: o.lon});
    $('#treffer').innerHTML = ''; $('#suche').value = '';
    melden(L('Ort gespeichert', 'Location saved')); laden();
  });
};
$('#ortAuto').onclick = async e => { e.preventDefault(); await senden('/api/ort-auto'); melden(L('Ort wird neu bestimmt', 'Detecting location')); setTimeout(laden, 4000); };
$('#nameForm').onsubmit = async e => {
  e.preventDefault();
  // Vorschlag aufraeumen: klein, Umlaute umschreiben, Leerzeichen -> "-"
  const n = $('#name').value.trim().toLowerCase().replace(/ä/g, 'ae').replace(/ö/g, 'oe').replace(/ü/g, 'ue').replace(/ß/g, 'ss')
    .replace(/[^a-z0-9-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 24);
  if (!n) return melden(L('Name ungültig', 'Invalid name'));
  if (n === st.einst.name) return melden(L('Unverändert', 'Unchanged'));
  if (!confirm(L(`Neuer Name: ${n}.local\nDie Station startet dazu neu.`, `New name: ${n}.local\nThe station restarts.`))) return;
  if (!await senden('/api/name', {name: n})) return melden(L('Name ungültig', 'Invalid name'));
  melden(L('Startet neu …', 'Restarting …'));
  setTimeout(() => location.href = `http://${n}.local/`, 8000);
};
$('#updBtn').onclick = async () => {
  if (st.neu) { if (await senden('/api/update/installieren')) melden(L('Update startet – siehe Display', 'Update starting – see display')); }
  else { await senden('/api/update/suchen'); melden(L('Suche …', 'Checking …')); setTimeout(laden, 4000); }
};
$('#wlanBtn').onclick = async () => { if (confirm(L('WLAN neu einrichten? Die Wetterstation startet neu.', 'Set up Wi-Fi again? The weather station will restart.'))) { await senden('/api/wlan'); melden(L('Startet neu', 'Restarting')); } };
laden();
</script>
</body>
</html>)rawliteral";
