// Simulation auf daveslab.de/basteln in der Sprache der Seite (Deutsch oder Englisch): stellt jemand die Seite auf
// Englisch, wechselt die eingebettete Simulation mit, und der Link "Simulation oeffnen" oeffnet sie auf Englisch
// (?lang=en). Fuer eine Seite, auf der die Wetterstation (basteln-wetter.js) schon eingebaut ist -
// neue Einbauten mit basteln-wetter.js haben das bereits.
// Die Simulation muss das koennen (simulation/wetterstation.html, ab dem Build mit "setzeSeitenSprache"); eine aeltere
// Simulation ignoriert die Meldung einfach.
//
// SO BENUTZEN (selbst, in Chrome):
//   1. Bei https://daveslab.de/wp-admin/ anmelden
//   2. F12 -> Reiter "Console" -> diese ganze Datei hineinkopieren -> Enter
//      (fragt Chrome nach "allow pasting": einmal  allow pasting  eintippen, Enter, nochmal einfuegen)
//   3. Rueckfrage "Speichern?" -> OK. Danach https://daveslab.de/basteln/ neu laden.
//   WordPress hebt die alte Fassung als Revision auf (Seite bearbeiten -> Revisionen).

var WETTER_SEITE = 'https://prod-byte.github.io/Wetterstation';
// gleicher Text wie WETTER_SPRACHE in basteln-wetter.js
var WETTER_SPRACHE = `addEventListener('message',function(e){if(e.origin==='https://prod-byte.github.io'&&e.data&&e.data.wetterstationHoehe){var f=document.getElementById('dl-wetter-sim');if(f){f.style.height=e.data.wetterstationHoehe+'px';f.contentWindow.postMessage({wetterstationSprache:document.documentElement.lang||'de'},'https://prod-byte.github.io');}}});
(function(){function melde(){var l=document.documentElement.lang||'de',f=document.getElementById('dl-wetter-sim'),a=document.querySelector('a[data-t="wet_auto_btn"]');if(a)a.href='${WETTER_SEITE}/simulation.html?lang='+l;if(f&&f.contentWindow)f.contentWindow.postMessage({wetterstationSprache:l},'https://prod-byte.github.io');}melde();new MutationObserver(melde).observe(document.documentElement,{attributes:true,attributeFilter:['lang']});})();`;
// was basteln-wetter.js bisher eingebaut hat (nur Hoehe anpassen)
var ALT_HOEHE = `addEventListener('message',function(e){if(e.origin==='https://prod-byte.github.io'&&e.data&&e.data.wetterstationHoehe){var f=document.getElementById('dl-wetter-sim');if(f)f.style.height=e.data.wetterstationHoehe+'px';}});`;

function spracheEinbauen(raw) {
  if (!raw.includes('id="dl-wetter-sim"')) return {out: raw, fehler: ['Wetterstation-Abschnitt fehlt - erst basteln-wetter.js ausfuehren']};
  if (raw.includes('wetterstationSprache')) return {out: raw, fehler: ['Ist schon eingebaut']};
  const treffer = raw.split(ALT_HOEHE).length - 1;
  if (treffer !== 1) return {out: raw, fehler: ['Hoehen-Skript: ' + treffer + ' Treffer']};
  return {out: raw.replace(ALT_HOEHE, () => WETTER_SPRACHE), fehler: []};
}

(async function spracheSpeichern() {
  if (!location.hostname.endsWith('daveslab.de')) { alert('Bitte auf daveslab.de/wp-admin ausfuehren.'); return; }
  const nonce = (typeof wpApiSettings !== 'undefined' && wpApiSettings.nonce) ||
                await (await fetch('/wp-admin/admin-ajax.php?action=rest-nonce', {credentials: 'include'})).text();
  if (!nonce || nonce === '0') { alert('Nicht angemeldet - bitte erst bei WordPress anmelden.'); return; }
  const kopf = {'X-WP-Nonce': nonce, 'Content-Type': 'application/json'};
  const liste = await (await fetch('/wp-json/wp/v2/pages?slug=basteln&context=edit&_fields=id,content', {headers: kopf, credentials: 'include'})).json();
  if (!Array.isArray(liste) || !liste[0]) { alert('Seite "basteln" nicht gefunden: ' + JSON.stringify(liste).slice(0, 200)); return; }
  const seite = liste[0];
  const {out, fehler} = spracheEinbauen(seite.content.raw);
  if (fehler.length) { alert('Nichts geaendert:\n' + fehler.join('\n')); return; }
  if (!confirm('Simulation in der Sprache der Seite - Seite "basteln" speichern?\n(' + seite.content.raw.length + ' -> ' + out.length + ' Zeichen)')) return;
  const r = await fetch('/wp-json/wp/v2/pages/' + seite.id, {method: 'POST', headers: kopf, credentials: 'include', body: JSON.stringify({content: out})});
  alert(r.ok ? 'Gespeichert! Jetzt https://daveslab.de/basteln/ neu laden.' : 'Fehler beim Speichern: ' + r.status + ' ' + (await r.text()).slice(0, 200));
})();
