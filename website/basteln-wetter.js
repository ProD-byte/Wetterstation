// Baut die "Wetterstation" in den Rohinhalt von daveslab.de/basteln ein
// (eigener Abschnitt ganz unten, hinter der ESPCam, mit eingebetteter Simulation).
// Gleiches Prinzip wie basteln-app.js im ESPCam-Repo: jede Ersetzung muss genau einmal passen,
// sonst wird nichts gespeichert. Firmware, Flasher und Simulation liegen auf GitHub Pages
// (https://prod-byte.github.io/Wetterstation/) und werden dort bei jedem Hochladen neu gebaut -
// auf daveslab.de muss man danach nichts mehr aendern.
//
// SO BENUTZEN (selbst, in Chrome):
//   1. Bei https://daveslab.de/wp-admin/ anmelden
//   2. F12 -> Reiter "Console" -> diese ganze Datei hineinkopieren -> Enter
//      (fragt Chrome nach "allow pasting": einmal  allow pasting  eintippen, Enter, nochmal einfuegen)
//   3. Es kommt eine Rueckfrage "Speichern?" -> OK. Danach https://daveslab.de/basteln/ neu laden.
//   WordPress hebt die alte Fassung als Revision auf (Seite bearbeiten -> Revisionen).
function wetterEinbauen(raw) {
  const fehler = [];
  let out = raw;
  function ersetze(name, such, neu) {
    const treffer = typeof such === 'string' ? out.split(such).length - 1 : (out.match(new RegExp(such.source, 'g')) || []).length;
    if (treffer !== 1) { fehler.push(name + ': ' + treffer + ' Treffer'); return; }
    out = out.replace(such, typeof neu === 'function' ? neu : () => neu);
  }
  if (out.includes('id="wetter"')) return {out: raw, fehler: ['Wetterstation ist schon eingebaut']};

  // --- Knopf oben im Kopfbereich (hinter der App) ---
  ersetze('hero knopf', /<a class="dl-btn" href="#app">[^<]*<\/a>/,
    m => m + '<a class="dl-btn" href="#wetter">🌦️ Wetterstation</a>');

  // --- Abschnitt ganz unten, vor dem Fuss ---
  ersetze('abschnitt', '<footer class="dl-foot">', WETTER_ABSCHNITT + '\n\n  <footer class="dl-foot">');

  // --- Uebersetzungen (hinter den App-Texten) ---
  ersetze('en wetter', "app_qr:'Scan with your phone',", "app_qr:'Scan with your phone',\n" + WETTER_EN + ',\n' + WETTER_WEB_EN + ',');
  ersetze('es wetter', "app_qr:'Escanea con el móvil',", "app_qr:'Escanea con el móvil',\n" + WETTER_ES + ',\n' + WETTER_WEB_ES + ',');
  ersetze('fr wetter', "app_qr:'Scanne avec ton téléphone',", "app_qr:'Scanne avec ton téléphone',\n" + WETTER_FR + ',\n' + WETTER_WEB_FR + ',');

  // --- Build-Nummer anzeigen + Hoehe der Simulation anpassen ---
  ersetze('var build', "var sprache='de',build=null,buildCam=null,buildXiao=null;",
    "var sprache='de',build=null,buildCam=null,buildXiao=null,buildWetter=null;");
  ersetze('setze ver', "if(vx&&buildXiao!==null)vx.textContent=(W.ver||DE.ver).replace('{0}',buildXiao);",
    "if(vx&&buildXiao!==null)vx.textContent=(W.ver||DE.ver).replace('{0}',buildXiao);\n" +
    "  var vw=document.getElementById('dl-ver-wetter');if(vw&&buildWetter!==null)vw.textContent=(W.ver||DE.ver).replace('{0}',buildWetter);");
  ersetze('fetch wetter', "buildXiao=j.build;setze(sprache);}).catch(function(){});",
    "buildXiao=j.build;setze(sprache);}).catch(function(){});\n" + WETTER_SKRIPT);

  return {out: fehler.length ? raw : out, fehler};
}

var WETTER_SEITE = 'https://prod-byte.github.io/Wetterstation';
var WETTER_GH = '<a href="https://github.com/ProD-byte/Wetterstation" target="_blank" rel="noopener">GitHub</a>';

// Kasten "Zur Einstellungsseite" (wie "Zur Auto-Steuerung" beim Roboauto)
var WETTER_WEB = `      <div class="dl-auto rv" style="margin-top:14px">
        <div>
          <h3 data-t="wet_web_h">⚙️ Zur Einstellungsseite</h3>
          <p data-t="wet_web_p">Ist die Wetterstation im WLAN, stellst du Ort, weitere Orte, Regenradar-Ausschnitt, Helligkeit, Nachtmodus und Updates im Browser unter <code>http://wetterstation.local</code> ein – Handy oder PC müssen im selben WLAN sein. Hast du der Station dort einen eigenen Namen gegeben (für mehrere Stationen), heißt die Adresse entsprechend. Klappt es nicht, die IP-Adresse der Station aus dem Router nehmen.</p>
        </div>
        <a class="dl-btn" href="http://wetterstation.local" target="_blank" rel="noopener" data-t="wet_web_btn">Einstellungen öffnen →</a>
      </div>`;
var WETTER_WEB_EN = [
  `wet_web_h:'⚙️ To the settings page'`,
  `wet_web_p:'Once the weather station is on your Wi-Fi, set location, more places, rain radar area, brightness, night mode and updates in the browser at <code>http://wetterstation.local</code> – phone or PC must be on the same Wi-Fi. If you gave the station its own name there (for several stations), the address changes accordingly. If that does not work, take the station’s IP address from your router.'`,
  `wet_web_btn:'Open settings →'`,
].join(',\n');
var WETTER_WEB_ES = [
  `wet_web_h:'⚙️ A la página de ajustes'`,
  `wet_web_p:'Cuando la estación está en tu Wi-Fi, ajustas la ubicación, otros lugares, la zona del radar de lluvia, el brillo, el modo nocturno y las actualizaciones en el navegador en <code>http://wetterstation.local</code> – el móvil o el PC deben estar en la misma Wi-Fi. Si le has dado allí un nombre propio (para varias estaciones), la dirección cambia en consecuencia. Si no funciona, toma la dirección IP de la estación del router.'`,
  `wet_web_btn:'Abrir ajustes →'`,
].join(',\n');
var WETTER_WEB_FR = [
  `wet_web_h:'⚙️ Vers la page de réglages'`,
  `wet_web_p:'Une fois la station sur ton Wi-Fi, règle le lieu, d’autres lieux, la zone du radar de pluie, la luminosité, le mode nuit et les mises à jour dans le navigateur sur <code>http://wetterstation.local</code> – le téléphone ou le PC doivent être sur le même Wi-Fi. Si tu lui as donné un nom à elle (pour plusieurs stations), l’adresse change en conséquence. Si ça ne marche pas, prends l’adresse IP de la station dans le routeur.'`,
  `wet_web_btn:'Ouvrir les réglages →'`,
].join(',\n');

var WETTER_ABSCHNITT = `  <section class="dl-sec" id="wetter">
    <div class="dl-wrap">
      <h2 class="dl-h2 rv" data-t="wet_h2">Wetterstation <span class="dl-grad">installieren</span></h2>
      <p class="dl-sub rv"><span data-t="wet_sub">Animiertes Wetter für deinen Ort auf dem <b>LILYGO T-Display-S3</b> (ESP32-S3). Erst in der Simulation ausprobieren – die runden Tasten funktionieren wie die echten –, dann per USB-C direkt hier aufspielen.</span> <span id="dl-ver-wetter"></span></p>
      <div class="rv" style="margin:6px 0 26px">
        <iframe id="dl-wetter-sim" src="${WETTER_SEITE}/simulation.html?einbetten" title="Simulation der Wetterstation" loading="lazy" style="display:block;width:100%;height:560px;border:0;background:transparent"></iframe>
      </div>
      <div class="dl-flash">
        <div class="dl-opt rv">
          <h3 data-t="upd_h">🔄 Update</h3>
          <p data-t="wet_upd_p">Neue Firmware aufspielen – <b>gespeicherte WLANs bleiben erhalten.</b></p>
          <esp-web-install-button manifest="${WETTER_SEITE}/manifest-update.json">
            <button slot="activate" data-t="upd_btn">⚡ Update installieren</button>
            <span slot="unsupported" data-t="unsup"><b>Dieser Browser kann das leider nicht.</b> Bitte Chrome oder Edge am Computer verwenden.</span>
            <span slot="not-allowed" data-t="notall"><b>Nur über https möglich.</b></span>
          </esp-web-install-button>
        </div>
        <div class="dl-opt rot rv">
          <h3 data-t="neu_h">🧹 Neuinstallation</h3>
          <p data-t="wet_neu_p">Speicher komplett löschen und neu aufspielen – für <b>neue Boards</b> oder wenn gar nichts mehr geht. <b>Gespeicherte WLANs sind danach weg.</b></p>
          <esp-web-install-button manifest="${WETTER_SEITE}/manifest-neu.json">
            <button slot="activate" data-t="neu_btn">🗑️ Löschen &amp; neu installieren</button>
            <span slot="unsupported" data-t="unsup"><b>Dieser Browser kann das leider nicht.</b> Bitte Chrome oder Edge am Computer verwenden.</span>
            <span slot="not-allowed" data-t="notall"><b>Nur über https möglich.</b></span>
          </esp-web-install-button>
        </div>
      </div>
      <ol class="dl-schritte" style="margin-top:28px">
        <li class="rv" data-t="wet_s1"><b>Board anschließen.</b> T-Display per <b>USB-C</b> mit dem Computer verbinden (Kabel mit Datenleitungen) und <b>Chrome</b> oder <b>Edge</b> öffnen.</li>
        <li class="rv" data-t="wet_s2"><b>Anschluss auswählen.</b> Meist „USB JTAG/serial debug unit“ – ein Treiber ist nicht nötig.</li>
        <li class="rv" data-t="wet_s3"><b>Keine Verbindung?</b> Taste <b>BOOT</b> gedrückt halten, kurz <b>RST</b> an der Seite drücken, BOOT loslassen – nochmal versuchen.</li>
        <li class="rv" data-t="wet_s4"><b>WLAN einrichten</b> (nur beim ersten Mal): Mit dem Handy ins WLAN <b>„Wetterstation-Setup“</b>, dein WLAN antippen, Passwort eingeben – dann deinen Ort suchen (Name oder PLZ) oder den Vorschlag antippen.</li>
        <li class="rv" data-t="wet_s5"><b>Bedienen.</b> <b>IO0</b> blättert weiter (2 Sekunden halten: Deutsch/Englisch), <b>IO14</b> ändert die Helligkeit – 3 Sekunden halten richtet das WLAN neu ein.</li>
      </ol>
      <div class="dl-auto rv">
        <div>
          <h3 data-t="wet_auto_h">🌦️ Simulation in groß</h3>
          <p data-t="wet_auto_p">Alle Wetterlagen, Tag und Nacht und der Einrichtungs-Bildschirm zum Durchklicken. Wetterdaten von Open-Meteo, alle 15 Minuten neu. Quellcode und Anleitung: ${WETTER_GH}.</p>
        </div>
        <a class="dl-btn" href="${WETTER_SEITE}/simulation.html" target="_blank" rel="noopener" data-t="wet_auto_btn">Simulation öffnen →</a>
      </div>
${WETTER_WEB}
    </div>
  </section>`;

var WETTER_SKRIPT = `fetch('${WETTER_SEITE}/version.json',{cache:'no-store'}).then(function(r){return r.json();}).then(function(j){buildWetter=j.build;setze(sprache);}).catch(function(){});
addEventListener('message',function(e){if(e.origin==='https://prod-byte.github.io'&&e.data&&e.data.wetterstationHoehe){var f=document.getElementById('dl-wetter-sim');if(f)f.style.height=e.data.wetterstationHoehe+'px';}});`;

var WETTER_EN = [
  `wet_h2:'Install the <span class="dl-grad">weather station</span>'`,
  `wet_sub:'Animated weather for your town on the <b>LILYGO T-Display-S3</b> (ESP32-S3). Try it in the simulation first – the round buttons work like the real ones –, then flash it right here via USB-C.'`,
  `wet_upd_p:'Install new firmware – <b>saved Wi-Fi networks are kept.</b>'`,
  `wet_neu_p:'Erase everything and install from scratch – for <b>new boards</b> or when nothing works anymore. <b>Saved Wi-Fi networks will be gone.</b>'`,
  `wet_s1:'<b>Connect the board.</b> Plug the T-Display into the computer via <b>USB-C</b> (a cable with data lines) and open <b>Chrome</b> or <b>Edge</b>.'`,
  `wet_s2:'<b>Select the port.</b> Usually “USB JTAG/serial debug unit” – no driver needed.'`,
  `wet_s3:'<b>No connection?</b> Hold the <b>BOOT</b> button, briefly press <b>RST</b> on the side, release BOOT – try again.'`,
  `wet_s4:'<b>Set up Wi-Fi</b> (first time only): connect your phone to the Wi-Fi <b>“Wetterstation-Setup”</b>, tap your Wi-Fi, enter the password – then search for your town (name or postcode) or tap the suggestion.'`,
  `wet_s5:'<b>Controls.</b> <b>IO0</b> turns the page (hold 2 seconds: German/English), <b>IO14</b> changes the brightness – hold it for 3 seconds to set up Wi-Fi again.'`,
  `wet_auto_h:'🌦️ Full-size simulation'`,
  `wet_auto_p:'Every kind of weather, day and night and the setup screen to click through. Weather data from Open-Meteo, refreshed every 15 minutes. Source code and guide: ${WETTER_GH}.'`,
  `wet_auto_btn:'Open simulation →'`,
].join(',\n');

var WETTER_ES = [
  `wet_h2:'Instalar la <span class="dl-grad">estación meteorológica</span>'`,
  `wet_sub:'El tiempo de tu ciudad animado en la <b>LILYGO T-Display-S3</b> (ESP32-S3). Pruébala primero en la simulación – los botones redondos funcionan como los de verdad – y luego instálala aquí mismo por USB-C.'`,
  `wet_upd_p:'Instala el nuevo firmware – <b>las redes Wi-Fi guardadas se conservan.</b>'`,
  `wet_neu_p:'Borra toda la memoria e instala de nuevo – para <b>placas nuevas</b> o cuando ya nada funciona. <b>Las redes Wi-Fi guardadas se borran.</b>'`,
  `wet_s1:'<b>Conecta la placa.</b> Conecta la T-Display al ordenador por <b>USB-C</b> (un cable con datos) y abre <b>Chrome</b> o <b>Edge</b>.'`,
  `wet_s2:'<b>Elige el puerto.</b> Normalmente «USB JTAG/serial debug unit»; no hace falta ningún controlador.'`,
  `wet_s3:'<b>¿Sin conexión?</b> Mantén pulsado <b>BOOT</b>, pulsa brevemente <b>RST</b> en el lateral, suelta BOOT – y vuelve a intentarlo.'`,
  `wet_s4:'<b>Configura el Wi-Fi</b> (solo la primera vez): conecta el móvil a la red <b>«Wetterstation-Setup»</b>, toca tu Wi-Fi e introduce la contraseña; luego busca tu ciudad (nombre o código postal) o toca la sugerencia.'`,
  `wet_s5:'<b>Manejo.</b> <b>IO0</b> pasa de página (mantenlo 2 segundos: alemán/inglés), <b>IO14</b> cambia el brillo – mantenlo 3 segundos para configurar el Wi-Fi de nuevo.'`,
  `wet_auto_h:'🌦️ Simulación en grande'`,
  `wet_auto_p:'Todos los tipos de tiempo, día y noche y la pantalla de configuración para probar. Datos de Open-Meteo, actualizados cada 15 minutos. Código fuente y guía: ${WETTER_GH}.'`,
  `wet_auto_btn:'Abrir simulación →'`,
].join(',\n');

var WETTER_FR = [
  `wet_h2:'Installer la <span class="dl-grad">station météo</span>'`,
  `wet_sub:'La météo de ta ville animée sur le <b>LILYGO T-Display-S3</b> (ESP32-S3). Essaie-la d’abord dans la simulation – les boutons ronds marchent comme les vrais –, puis installe-la directement ici en USB-C.'`,
  `wet_upd_p:'Installe le nouveau firmware – <b>les Wi-Fi enregistrés sont conservés.</b>'`,
  `wet_neu_p:'Efface toute la mémoire et réinstalle – pour les <b>nouvelles cartes</b> ou quand plus rien ne marche. <b>Les Wi-Fi enregistrés seront effacés.</b>'`,
  `wet_s1:'<b>Branche la carte.</b> Relie le T-Display à l’ordinateur en <b>USB-C</b> (câble avec données) et ouvre <b>Chrome</b> ou <b>Edge</b>.'`,
  `wet_s2:'<b>Choisis le port.</b> En général « USB JTAG/serial debug unit » – aucun pilote nécessaire.'`,
  `wet_s3:'<b>Pas de connexion ?</b> Maintiens le bouton <b>BOOT</b>, appuie brièvement sur <b>RST</b> sur le côté, relâche BOOT – puis réessaie.'`,
  `wet_s4:'<b>Configure le Wi-Fi</b> (la première fois seulement) : connecte ton téléphone au Wi-Fi <b>« Wetterstation-Setup »</b>, touche ton Wi-Fi, saisis le mot de passe – puis cherche ta ville (nom ou code postal) ou touche la suggestion.'`,
  `wet_s5:'<b>Commandes.</b> <b>IO0</b> change de page (maintiens-le 2 secondes : allemand/anglais), <b>IO14</b> règle la luminosité – maintiens-le 3 secondes pour reconfigurer le Wi-Fi.'`,
  `wet_auto_h:'🌦️ Simulation en grand'`,
  `wet_auto_p:'Toutes les météos, le jour et la nuit et l’écran de configuration à tester. Données d’Open-Meteo, mises à jour toutes les 15 minutes. Code source et guide : ${WETTER_GH}.'`,
  `wet_auto_btn:'Ouvrir la simulation →'`,
].join(',\n');

// ---------- Ausfuehren: Seite holen, einbauen, nach Rueckfrage speichern ----------
(async function wetterSpeichern() {
  if (!location.hostname.endsWith('daveslab.de')) { alert('Bitte auf daveslab.de/wp-admin ausfuehren.'); return; }
  const nonce = (typeof wpApiSettings !== 'undefined' && wpApiSettings.nonce) ||
                await (await fetch('/wp-admin/admin-ajax.php?action=rest-nonce', {credentials: 'include'})).text();
  if (!nonce || nonce === '0') { alert('Nicht angemeldet - bitte erst bei WordPress anmelden.'); return; }
  const kopf = {'X-WP-Nonce': nonce, 'Content-Type': 'application/json'};
  const liste = await (await fetch('/wp-json/wp/v2/pages?slug=basteln&context=edit&_fields=id,content', {headers: kopf, credentials: 'include'})).json();
  if (!Array.isArray(liste) || !liste[0]) { alert('Seite "basteln" nicht gefunden: ' + JSON.stringify(liste).slice(0, 200)); return; }
  const seite = liste[0];
  const {out, fehler} = wetterEinbauen(seite.content.raw);
  if (fehler.length) { alert('Nichts geaendert:\n' + fehler.join('\n')); return; }
  if (!confirm('Wetterstation einbauen und Seite "basteln" speichern?\n(' + seite.content.raw.length + ' -> ' + out.length + ' Zeichen)')) return;
  const r = await fetch('/wp-json/wp/v2/pages/' + seite.id, {method: 'POST', headers: kopf, credentials: 'include', body: JSON.stringify({content: out})});
  alert(r.ok ? 'Gespeichert! Jetzt https://daveslab.de/basteln/ neu laden.' : 'Fehler beim Speichern: ' + r.status + ' ' + (await r.text()).slice(0, 200));
})();
