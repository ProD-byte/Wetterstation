// Baut die beiden YouTube-Videos (Trailer + Update) in den Wetterstation-Abschnitt von daveslab.de/basteln ein,
// direkt unter die Simulation. Der Abschnitt selbst kommt aus basteln-wetter.js und muss schon da sein.
//
// Datenschutz (Zwei-Klick-Loesung): Bis jemand auf ein Video klickt, laedt die Seite nichts von YouTube -
// sie zeigt nur unser eigenes Vorschaubild von GitHub Pages (docs/video/*.jpg) mit Play-Knopf. Erst der Klick
// laedt das Video ueber youtube-nocookie.com. So braucht es fuer die Videos keinen Cookie-Banner.
//
// SO BENUTZEN (selbst, in Chrome):
//   1. Bei https://daveslab.de/wp-admin/ anmelden
//   2. F12 -> Reiter "Console" -> diese ganze Datei hineinkopieren -> Enter
//      (fragt Chrome nach "allow pasting": einmal  allow pasting  eintippen, Enter, nochmal einfuegen)
//   3. Nach den beiden YouTube-Links gefragt werden -> die eingetragenen Links mit Enter uebernehmen.
//      Gibt es das Update-Video noch nicht: Feld leer lassen, dann kommt nur der Trailer auf die Seite.
//   4. Rueckfrage "Speichern?" -> OK. Danach https://daveslab.de/basteln/ neu laden.
//   Nochmal ausfuehren ersetzt die Videos (z. B. um das Update-Video nachzutragen).
//   WordPress hebt die alte Fassung als Revision auf (Seite bearbeiten -> Revisionen).

var VIDEO_BILDER = 'https://prod-byte.github.io/Wetterstation/video';
var VIDEO_KANAL = 'https://www.youtube.com/@Daveslab-byte';
// Vorschlaege fuer die Abfrage (einfach mit Enter uebernehmen)
var LINK_TRAILER = 'https://youtu.be/l8NVJyQLpJs';
var LINK_UPDATE = 'https://youtu.be/fHbe5BCfev0';

// YouTube-Link -> Video-ID (youtu.be/ID, watch?v=ID, shorts/ID, embed/ID oder nur die ID)
function videoId(link) {
  link = (link || '').trim();
  if (/^[\w-]{11}$/.test(link)) return link;
  const m = link.match(/(?:youtu\.be\/|[?&]v=|\/shorts\/|\/embed\/|\/live\/)([\w-]{11})/);
  return m ? m[1] : null;
}

function videoKarte(id, bild, key, titel) {
  return `      <figure style="margin:0">
        <button type="button" class="dl-yt" data-yt="${id}" aria-label="${titel}" style="position:relative;display:block;width:100%;aspect-ratio:16/9;padding:0;border:0;border-radius:14px;overflow:hidden;cursor:pointer;background:#000;box-shadow:0 10px 30px rgba(0,0,0,.35)">
          <img src="${VIDEO_BILDER}/${bild}" alt="" loading="lazy" style="display:block;width:100%;height:100%;object-fit:cover">
          <span aria-hidden="true" style="position:absolute;left:50%;top:50%;width:76px;height:76px;margin:-38px 0 0 -38px;border-radius:50%;background:rgba(0,0,0,.62);box-shadow:0 0 0 3px rgba(255,255,255,.85)"></span>
          <span aria-hidden="true" style="position:absolute;left:50%;top:50%;margin:-15px 0 0 -9px;border-style:solid;border-width:15px 0 15px 26px;border-color:transparent transparent transparent #fff"></span>
        </button>
        <figcaption style="margin-top:8px;font-weight:600" data-t="${key}">${titel}</figcaption>
      </figure>`;
}

function videoBlock(trailer, update) {
  return `<div class="rv" id="wetter-videos" style="margin:0 0 26px">
      <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:18px">
${trailer ? videoKarte(trailer, 'trailer.jpg', 'wet_vid1', 'Trailer: die Wetterstation in einer Minute') : ''}
${update ? videoKarte(update, 'update.jpg', 'wet_vid2', 'Update: Regenbogen, Sturm, Schnee &amp; Silvester-Feuerwerk') : ''}
      </div>
      <p style="margin:10px 0 0;font-size:.85em;opacity:.7" data-t="wet_vid_hin">Erst beim Abspielen wird das Video von YouTube geladen (youtube-nocookie.com) – dabei werden Daten an YouTube übertragen.</p>
      <p style="margin:12px 0 0"><a class="dl-btn" href="${VIDEO_KANAL}" target="_blank" rel="noopener" data-t="wet_vid_kanal">▶ Mehr auf YouTube</a></p>
    </div>`;
}

// Klick auf ein Vorschaubild -> Video laden
var VIDEO_SKRIPT = `document.addEventListener('click',function(e){var b=e.target.closest&&e.target.closest('.dl-yt');if(!b)return;var f=document.createElement('iframe');f.src='https://www.youtube-nocookie.com/embed/'+b.getAttribute('data-yt')+'?autoplay=1&rel=0';f.title=b.getAttribute('aria-label');f.allow='autoplay; encrypted-media; picture-in-picture; fullscreen';f.allowFullscreen=true;f.style.cssText='display:block;width:100%;aspect-ratio:16/9;border:0;border-radius:14px';b.replaceWith(f);});`;

var VIDEO_TEXTE = {
  en: [`wet_vid1:'Trailer: the weather station in one minute'`, `wet_vid2:'Update: rainbows, storms, snow &amp; New Year’s fireworks'`,
       `wet_vid_hin:'The video is only loaded from YouTube when you press play (youtube-nocookie.com) – data is then sent to YouTube.'`, `wet_vid_kanal:'▶ More on YouTube'`],
};
// Anker in den Uebersetzungen: der letzte Text der Simulations-Box (aus basteln-wetter.js).
// Die Seite gibt es nur auf Deutsch und Englisch.
var VIDEO_ANKER = {en: "wet_auto_btn:'Open simulation →'"};

function videosEinbauen(raw, trailer, update) {
  const fehler = [];
  let out = raw;
  function ersetze(name, such, neu) {
    const treffer = typeof such === 'string' ? out.split(such).length - 1 : (out.match(new RegExp(such.source, 'g')) || []).length;
    if (treffer !== 1) { fehler.push(name + ': ' + treffer + ' Treffer'); return; }
    out = out.replace(such, typeof neu === 'function' ? neu : () => neu);
  }
  if (!out.includes('id="wetter"')) return {out: raw, fehler: ['Wetterstation-Abschnitt fehlt - erst basteln-wetter.js ausfuehren']};

  const neu = videoBlock(trailer, update);
  if (out.includes('id="wetter-videos"')) {
    // schon eingebaut: nur den Videoblock austauschen (Texte und Skript sind schon da)
    ersetze('videos', /<div class="rv" id="wetter-videos"[\s\S]*?data-t="wet_vid_kanal"[^>]*>[^<]*<\/a><\/p>\s*<\/div>/, neu);
  } else {
    // Block unter die Simulation
    ersetze('simulation', /<iframe id="dl-wetter-sim"[^>]*><\/iframe>\s*<\/div>/, m => m + '\n      ' + neu);
    // englische Uebersetzungen
    ersetze('en texte', VIDEO_ANKER.en, VIDEO_ANKER.en + ',\n' + VIDEO_TEXTE.en.join(',\n'));
    // Klick-Skript hinter das Skript der Wetterstation (holt die Build-Nummer)
    ersetze('skript', /fetch\('https:\/\/prod-byte\.github\.io\/Wetterstation\/version\.json'[^\n]*\n/, m => m + VIDEO_SKRIPT + '\n');
  }
  return {out: fehler.length ? raw : out, fehler};
}

// ---------- Ausfuehren: Links abfragen, Seite holen, einbauen, nach Rueckfrage speichern ----------
(async function videosSpeichern() {
  if (!location.hostname.endsWith('daveslab.de')) { alert('Bitte auf daveslab.de/wp-admin ausfuehren.'); return; }
  const trailer = videoId(prompt('YouTube-Link zum TRAILER:', LINK_TRAILER));
  if (!trailer) { alert('Das ist kein YouTube-Link - nichts geaendert.'); return; }
  const eingabe = prompt('YouTube-Link zum UPDATE-VIDEO (leer lassen, wenn es noch nicht online ist):', LINK_UPDATE);
  const update = eingabe && eingabe.trim() ? videoId(eingabe) : null;
  if (eingabe && eingabe.trim() && !update) { alert('Der Update-Link ist kein YouTube-Link - nichts geaendert.'); return; }

  const nonce = (typeof wpApiSettings !== 'undefined' && wpApiSettings.nonce) ||
                await (await fetch('/wp-admin/admin-ajax.php?action=rest-nonce', {credentials: 'include'})).text();
  if (!nonce || nonce === '0') { alert('Nicht angemeldet - bitte erst bei WordPress anmelden.'); return; }
  const kopf = {'X-WP-Nonce': nonce, 'Content-Type': 'application/json'};
  const liste = await (await fetch('/wp-json/wp/v2/pages?slug=basteln&context=edit&_fields=id,content', {headers: kopf, credentials: 'include'})).json();
  if (!Array.isArray(liste) || !liste[0]) { alert('Seite "basteln" nicht gefunden: ' + JSON.stringify(liste).slice(0, 200)); return; }
  const seite = liste[0];
  const {out, fehler} = videosEinbauen(seite.content.raw, trailer, update);
  if (fehler.length) { alert('Nichts geaendert:\n' + fehler.join('\n')); return; }
  if (!confirm('Videos einbauen und Seite "basteln" speichern?\nTrailer: ' + trailer + '\nUpdate: ' + (update || '(noch keins)') +
               '\n(' + seite.content.raw.length + ' -> ' + out.length + ' Zeichen)')) return;
  const r = await fetch('/wp-json/wp/v2/pages/' + seite.id, {method: 'POST', headers: kopf, credentials: 'include', body: JSON.stringify({content: out})});
  alert(r.ok ? 'Gespeichert! Jetzt https://daveslab.de/basteln/ neu laden.' : 'Fehler beim Speichern: ' + r.status + ' ' + (await r.text()).slice(0, 200));
})();
