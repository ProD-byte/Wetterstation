#pragma once
#include <Arduino.h>

// Einrichtungsseite im Setup-WLAN (wie bei der ESPCam). Komplett eigenstaendig, weil es im
// Setup-WLAN kein Internet gibt. Holt die Netzliste von /scan, sendet an /save und fragt /status ab.
static const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0d12">
<title>Wetterstation einrichten</title>
<style>
:root{--bg:#0b0d12;--card:#151922;--card2:#1d222d;--line:#262c39;--text:#e8ebf1;--muted:#8b93a5;--accent:#f0a030;--accent2:#e5642e;--ok:#2fd27a;--bad:#ff5468}
@media (prefers-color-scheme:light){:root{--bg:#f3f5f9;--card:#fff;--card2:#f0f2f7;--line:#e2e6ee;--text:#151922;--muted:#647089}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;-webkit-font-smoothing:antialiased}
svg{fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.wrap{max-width:440px;margin:auto;padding:32px 16px 40px}
.brand{display:flex;align-items:center;gap:14px;margin-bottom:24px}
.logo{width:48px;height:48px;border-radius:14px;flex:none;background:linear-gradient(135deg,var(--accent),var(--accent2));display:grid;place-items:center;color:#fff;box-shadow:0 10px 30px -10px var(--accent)}
.logo svg{width:26px;height:26px}
h1{font-size:22px;margin:0;font-weight:700;letter-spacing:-.01em}
.sub{color:var(--muted);font-size:14px;margin:2px 0 0}
.card{background:var(--card);border:1px solid var(--line);border-radius:20px;overflow:hidden;box-shadow:0 20px 40px -24px rgba(0,0,0,.45)}
.head{display:flex;align-items:center;justify-content:space-between;padding:12px 12px 12px 18px;border-bottom:1px solid var(--line)}
.head span{font-size:12px;font-weight:650;color:var(--muted);text-transform:uppercase;letter-spacing:.08em}
.icon-btn{border:0;background:var(--card2);color:var(--text);width:38px;height:38px;border-radius:11px;display:grid;place-items:center;cursor:pointer}
.icon-btn svg{width:18px;height:18px}
.spin svg{animation:spin 1s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.net{display:flex;align-items:center;gap:14px;width:100%;padding:15px 18px;border:0;border-bottom:1px solid var(--line);background:none;color:inherit;font:inherit;text-align:left;cursor:pointer;transition:background .15s}
.net:last-child{border-bottom:0}
.net:hover,.net:active{background:var(--card2)}
.net .name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-weight:550}
.net svg{flex:none;width:20px;height:20px}
.net .lock{color:var(--muted);width:16px;height:16px}
.bars{color:var(--accent)}
.bars rect{fill:currentColor;stroke:none;opacity:.22}
.bars rect.on{opacity:1}
.skel{height:54px;border-bottom:1px solid var(--line);background:linear-gradient(90deg,transparent 0,var(--card2) 50%,transparent 100%);background-size:200% 100%;animation:sh 1.2s infinite}
@keyframes sh{from{background-position:200% 0}to{background-position:-200% 0}}
.empty{padding:28px 18px;color:var(--muted);text-align:center}
.other{display:flex;align-items:center;gap:14px;width:100%;padding:15px 18px;border:0;border-top:1px solid var(--line);background:none;color:var(--accent);font:inherit;font-weight:550;cursor:pointer}
.other svg{width:20px;height:20px}
.pad{padding:20px 18px 22px}
.chosen{display:flex;align-items:center;gap:12px;padding:14px;background:var(--card2);border-radius:14px;font-weight:600}
.chosen .name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.chosen svg{width:20px;height:20px;flex:none}
label{display:block;font-size:13px;font-weight:550;color:var(--muted);margin:18px 0 8px}
.field{position:relative}
input{width:100%;font:inherit;font-size:17px;padding:14px 50px 14px 14px;border-radius:14px;border:1px solid var(--line);background:var(--card2);color:var(--text);outline:none;transition:border-color .15s,box-shadow .15s}
input:focus{border-color:var(--accent);box-shadow:0 0 0 4px rgba(240,160,48,.2)}
.eye{position:absolute;right:6px;top:50%;transform:translateY(-50%);border:0;background:none;color:var(--muted);width:40px;height:40px;display:grid;place-items:center;cursor:pointer}
.eye svg{width:20px;height:20px}
.primary{width:100%;margin-top:22px;padding:16px;border:0;border-radius:14px;background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;font:inherit;font-weight:650;cursor:pointer;box-shadow:0 10px 24px -12px var(--accent)}
.primary:active{transform:scale(.99)}
.ghost{display:block;width:100%;margin-top:8px;padding:14px;border:0;background:none;color:var(--muted);font:inherit;font-weight:550;cursor:pointer}
.state{padding:40px 22px 30px;text-align:center}
.state h2{margin:0 0 6px;font-size:20px}
.state p{margin:0;color:var(--muted)}
.big{width:72px;height:72px;border-radius:50%;margin:0 auto 18px;display:grid;place-items:center}
.big svg{width:34px;height:34px;stroke-width:2.5}
.ring{border:4px solid var(--card2);border-top-color:var(--accent);animation:spin .9s linear infinite}
.okc{background:rgba(47,210,122,.15);color:var(--ok)}
.badc{background:rgba(255,84,104,.15);color:var(--bad)}
.hint{margin-top:18px!important;font-size:14px}
.foot{text-align:center;color:var(--muted);font-size:13px;margin-top:18px}
[hidden]{display:none!important}
</style>
</head>
<body>
<div class="wrap">
  <div class="brand">
    <div class="logo"><svg viewBox="0 0 24 24"><circle cx="15" cy="8" r="3"/><path d="M15 2v1M20.2 4.8l-.7.7M22 9h-1M7 20h10a4 4 0 0 0 0-8 5 5 0 0 0-9.6 1.2A3.5 3.5 0 0 0 7 20z"/></svg></div>
    <div><h1>Wetterstation einrichten</h1><p class="sub">Wähle das WLAN für deine Wetterstation</p></div>
  </div>

  <div class="card" id="list">
    <div class="head"><span>Netzwerke in der Nähe</span>
      <button class="icon-btn" id="refresh" aria-label="Neu suchen"><svg viewBox="0 0 24 24"><polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/></svg></button>
    </div>
    <div id="nets"></div>
    <button class="other" id="otherBtn"><svg viewBox="0 0 24 24"><line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/></svg>Anderes Netzwerk</button>
  </div>

  <form class="card pad" id="form" hidden autocomplete="off">
    <div class="chosen" id="chosen"><span id="chosenBars"></span><span class="name" id="chosenName"></span></div>
    <div id="manual"><label for="ssid">Name des Netzwerks</label><div class="field"><input id="ssid" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="WLAN-Name"></div></div>
    <div id="passWrap">
      <label for="pass">Passwort</label>
      <div class="field">
        <input id="pass" type="password" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="WLAN-Passwort">
        <button type="button" class="eye" id="eye" aria-label="Passwort anzeigen"><svg viewBox="0 0 24 24"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/><circle cx="12" cy="12" r="3"/></svg></button>
      </div>
    </div>
    <button class="primary">Verbinden</button>
    <button type="button" class="ghost" id="back">Zurück</button>
  </form>

  <div class="card state" id="busy" hidden>
    <div class="big ring"></div>
    <h2>Verbinde …</h2>
    <p>mit <b id="busyName"></b></p>
  </div>

  <div class="card state" id="done" hidden>
    <div class="big okc"><svg viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"/></svg></div>
    <h2>Verbunden!</h2>
    <p>Die Wetterstation holt jetzt das Wetter und zeigt es gleich auf dem Display an.</p>
    <p class="hint">Verbinde dein Handy wieder mit deinem normalen WLAN.</p>
  </div>

  <div class="card state" id="fail" hidden>
    <div class="big badc"><svg viewBox="0 0 24 24"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg></div>
    <h2>Verbindung fehlgeschlagen</h2>
    <p>Passwort falsch oder Netz nicht erreichbar.</p>
    <button class="primary" id="retry">Nochmal versuchen</button>
  </div>

  <p class="foot">Gespeicherte Netze merkt sich die Wetterstation automatisch.</p>
</div>
<script>
const $ = s => document.querySelector(s);
const VIEWS = ['list', 'form', 'busy', 'done', 'fail'];
const show = v => VIEWS.forEach(x => $('#' + x).hidden = x !== v);
const esc = s => s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const LOCK = '<svg class="lock" viewBox="0 0 24 24"><rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>';
function bars(rssi) {
  const level = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
  let s = '<svg class="bars" viewBox="0 0 20 20">';
  for (let i = 0; i < 4; i++) {
    const h = 5 + i * 4;
    s += `<rect x="${1 + i * 5}" y="${18 - h}" width="3.5" height="${h}" rx="1"${i < level ? ' class="on"' : ''}/>`;
  }
  return s + '</svg>';
}

let nets = [], sel = null;
async function scan() {
  $('#refresh').classList.add('spin');
  $('#nets').innerHTML = '<div class="skel"></div>'.repeat(4);
  try {
    nets = await (await fetch('/scan')).json();
    $('#nets').innerHTML = nets.length
      ? nets.map((n, i) => `<button class="net" data-i="${i}">${bars(n.rssi)}<span class="name">${esc(n.ssid)}</span>${n.open ? '' : LOCK}</button>`).join('')
      : '<div class="empty">Keine Netzwerke gefunden</div>';
    document.querySelectorAll('.net').forEach(b => b.onclick = () => pick(nets[b.dataset.i]));
  } catch (e) {
    $('#nets').innerHTML = '<div class="empty">Suche fehlgeschlagen</div>';
  }
  $('#refresh').classList.remove('spin');
}

function pick(n) {
  sel = n;
  show('form');
  $('#chosen').hidden = !n;
  $('#manual').hidden = !!n;
  $('#passWrap').hidden = !!(n && n.open);
  $('#pass').value = '';
  if (n) { $('#chosenName').textContent = n.ssid; $('#chosenBars').innerHTML = bars(n.rssi); }
  setTimeout(() => { const f = !n ? $('#ssid') : n.open ? null : $('#pass'); if (f) f.focus(); }, 50);
}

async function poll() {
  try {
    const s = await (await fetch('/status')).json();
    if (s.state === 'ok') { show('done'); return; }
    if (s.state === 'failed') { show('fail'); return; }
  } catch (e) {}
  setTimeout(poll, 1000);
}

$('#form').onsubmit = async e => {
  e.preventDefault();
  const ssid = sel ? sel.ssid : $('#ssid').value.trim();
  if (!ssid) { $('#ssid').focus(); return; }
  $('#busyName').textContent = ssid;
  show('busy');
  try { await fetch('/save', {method: 'POST', body: new URLSearchParams({ssid, pass: $('#pass').value})}); } catch (e) {}
  setTimeout(poll, 1500);
};
$('#eye').onclick = () => { const p = $('#pass'); p.type = p.type === 'password' ? 'text' : 'password'; };
$('#refresh').onclick = scan;
$('#otherBtn').onclick = () => pick(null);
$('#back').onclick = () => show('list');
$('#retry').onclick = () => pick(sel);
scan();
</script>
</body>
</html>)rawliteral";
