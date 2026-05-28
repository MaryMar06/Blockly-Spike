import { log } from '../ui/uiHelpers.js';
import { generateCommands } from '../generators/udpGenerator.js';

// ============================================================
// SERIAL CONNECTION (Serielle Verbindungsschnittstelle)
// ============================================================
// Dieses Modul stellt eine Brücke (Bridge) zur Hardware dar.
// Es funktioniert in zwei Modi, die automatisch erkannt werden:
//   1. Electron-Modus: Nutzt `window.serialAPI` (Node.js 'serialport' Modul über IPC).
//   2. Browser-Modus: Nutzt die native `navigator.serial` (WebSerial API).
// Die abstrakten Methoden (`enviarAPi`, `detenerRobot`) arbeiten immer
// gleich, unabhängig davon, welcher Modus darunterliegt.
// ============================================================

// Prüft, ob die App im Electron-Desktop-Container läuft
const IS_ELECTRON = typeof window !== 'undefined' && window.isElectron === true;

// Globale Flag-Variable, um das Senden von Befehls-Schleifen hart abzubrechen (Not-Stopp)
let runAbortado = false;

// ── ABSTRAKTION: Einheitliches Interface für Electron & WebSerial ────
// Dieses Objekt kapselt die aktuelle Schreib-Funktion. Egal welches 
// Backend aktiv ist, alle anderen Funktionen rufen nur `serial.write()` auf.
const serial = {
  _write: null,   // Funktion: (line: string) => Promise. Null bedeutet "nicht verbunden".

  async connect() {
    if (IS_ELECTRON) return _connectElectron();
    else             return _connectWebSerial();
  },
  async disconnect() {
    if (IS_ELECTRON) return _disconnectElectron();
    else             return _disconnectWebSerial();
  },
  async write(line) {
    if (!this._write) throw new Error('Nicht verbunden');
    return this._write(line);
  },
  // Getter für schnelle Statusprüfungen im UI
  get connected() {
    return this._write !== null;
  }
};

// ── ELECTRON IMPLEMENTIERUNG ─────────────────────────────────

/**
 * Verbindet über die IPC-Schnittstelle von Electron (Node.js serialport).
 */
async function _connectElectron() {
  // Holt alle verfügbaren System-Ports
  const ports = await window.serialAPI.list();
  if (ports.length === 0) throw new Error('Keine seriellen Ports gefunden');

  // Filtert nach relevanten USB-Ports. Ignoriert reine Bluetooth-COMs (soweit möglich).
  const usbPorts = ports.filter(p =>
    p.path.startsWith('COM') || p.path.includes('ttyUSB') || p.path.includes('ttyACM')
  );

  let chosen;
  // Wenn genau ein USB-Gerät erkannt wird, wird "One-Click-Connect" verwendet
  if (usbPorts.length === 1) {
    chosen = usbPorts[0].path;
  } else {
    // Wenn mehrere Geräte vorhanden sind (oder nur nicht-USB-Ports), 
    // muss der Nutzer über ein UI-Overlay wählen.
    chosen = await _showPortPicker(usbPorts.length > 0 ? usbPorts : ports);
    if (!chosen) throw new Error('Abgebrochen'); // User hat auf "Abbrechen" geklickt
  }

  // Öffnet den gewählten Port via IPC im Main-Process
  await window.serialAPI.open(chosen);
  
  // Zuweisung der Schreib-Funktion für die Abstraktions-Schicht
  serial._write = (line) => window.serialAPI.write(line);

  // Registrierung der IPC-Event-Listener für eingehende Daten und Status
  window.serialAPI.onData(_handleLine);
  window.serialAPI.onError(msg => log('Serial Fehler: ' + msg, 'error'));
  window.serialAPI.onClose(() => {
    serial._write = null;
    _setUI(false);
    log('Port geschlossen.', 'warn');
  });

  _setUI(true, chosen);
  log(`✓ Verbunden mit ${chosen}`, 'success');
}

/**
 * Trennt die Electron-Verbindung sauber.
 */
async function _disconnectElectron() {
  window.serialAPI.removeAllListeners();
  await window.serialAPI.close();
  serial._write = null;
  _setUI(false);
  log('Getrennt.', 'warn');
}

/**
 * Erzeugt ein dynamisches UI-Overlay für die Port-Auswahl.
 * Wird nur im Electron-Modus aufgerufen, wenn mehr als ein Gerät gefunden wird.
 * (Im WebSerial-Modus übernimmt der Browser diesen Dialog nativ).
 */
function _showPortPicker(ports) {
  return new Promise(resolve => {
    // Erstellt den abgedunkelten Hintergrund (Overlay)
    const overlay = document.createElement('div');
    overlay.style.cssText = `
      position:fixed;inset:0;background:rgba(0,0,0,0.7);
      display:flex;align-items:center;justify-content:center;z-index:9999`;

    // Erstellt die Dialog-Box
    const box = document.createElement('div');
    box.style.cssText = `
      background:#1a1a2e;border:1px solid #333;border-radius:12px;
      padding:24px;min-width:280px;font-family:Nunito,sans-serif`;

    box.innerHTML = `
      <div style="color:#cdd9e5;font-weight:700;font-size:1em;margin-bottom:16px">
        Port auswählen
      </div>`;

    // Generiert für jeden gefundenen Port einen Button
    ports.forEach(p => {
      const btn = document.createElement('button');
      btn.textContent = `${p.path}${p.manufacturer ? '  —  ' + p.manufacturer : ''}`;
      btn.style.cssText = `
        display:block;width:100%;margin-bottom:8px;padding:10px 14px;
        background:#252540;border:1px solid #444;border-radius:8px;
        color:#cdd9e5;font-size:0.9em;cursor:pointer;text-align:left`;
      
      // Hover-Effekte via JavaScript (da dynamisch generiertes Inline-CSS)
      btn.onmouseenter = () => btn.style.background = '#353560';
      btn.onmouseleave = () => btn.style.background = '#252540';
      
      // Klick löst das Promise mit dem gewählten Pfad auf
      btn.onclick = () => { document.body.removeChild(overlay); resolve(p.path); };
      box.appendChild(btn);
    });

    // Abbrechen-Button
    const cancel = document.createElement('button');
    cancel.textContent = 'Abbrechen';
    cancel.style.cssText = `
      display:block;width:100%;padding:8px;margin-top:4px;
      background:transparent;border:1px solid #555;border-radius:8px;
      color:#888;font-size:0.85em;cursor:pointer`;
    cancel.onclick = () => { document.body.removeChild(overlay); resolve(null); };
    box.appendChild(cancel);

    overlay.appendChild(box);
    document.body.appendChild(overlay);
  });
}

// ── WEBSERIAL IMPLEMENTIERUNG (Browser) ───────────────────────

// Globale Variablen für den Web-Modus
let _wsPort   = null;      // Der WebSerial Port-Instanz
let _wsWriter = null;      // Der Stream-Writer (TX)
let _wsLineBuf = '';       // Zwischenspeicher für unvollständige ankommende Zeilen

/**
 * Verbindet über die im Browser native WebSerial API.
 * Ruft automatisch den nativen Geräte-Auswahldialog des Browsers auf.
 */
async function _connectWebSerial() {
  if (!('serial' in navigator)) {
    throw new Error('WebSerial wird nicht unterstützt. Bitte nutze Chrome oder Edge.');
  }
  
  // Fordert den Nutzer auf, einen Port zu wählen
  _wsPort = await navigator.serial.requestPort();
  
  // Konfiguriert die Baudrate. Muss zwingend mit der Hardware übereinstimmen!
  await _wsPort.open({ baudRate: 115200 });

  // Stream-Setup für das SENDEN (Text -> Bytes)
  const enc = new TextEncoderStream();
  enc.readable.pipeTo(_wsPort.writable);
  _wsWriter = enc.writable.getWriter();

  // Stream-Setup für das EMPFANGEN (Bytes -> Text)
  const dec = new TextDecoderStream();
  _wsPort.readable.pipeTo(dec.writable);
  
  // Startet die asynchrone Leseschleife
  _wsReadLoop(dec.readable.getReader());

  // Zuweisung der Schreib-Funktion (hängt immer ein '\n' als Abschluss an)
  serial._write = (line) => {
    return _wsWriter.write(line + '\n');
  };

  _setUI(true, 'Wemos');
  log('✓ Wemos (WebSerial) verbunden', 'success');
}

/**
 * Trennt die WebSerial Verbindung sicher auf und schließt alle Streams.
 */
async function _disconnectWebSerial() {
  try { if (_wsWriter) { await _wsWriter.close(); _wsWriter = null; } } catch(_) {}
  try { if (_wsPort)   { await _wsPort.close();   _wsPort   = null; } } catch(_) {}
  serial._write = null;
  _setUI(false);
  log('Getrennt.', 'warn');
}

/**
 * Endlos-Schleife, die eingehende Bytes vom WebSerial-Port liest.
 * Fügt Bruchstücke im Puffer zusammen und trennt sie bei Zeilenumbrüchen ('\n').
 */
function _wsReadLoop(reader) {
  (async () => {
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break; // Stream wurde geschlossen
        
        _wsLineBuf += value; // Hängt den neuen Chunk an den Puffer an
        let nl;
        
        // Solange es Zeilenumbrüche im Puffer gibt, extrahiere komplette Zeilen
        while ((nl = _wsLineBuf.indexOf('\n')) !== -1) {
          const line = _wsLineBuf.slice(0, nl).trim();
          _wsLineBuf = _wsLineBuf.slice(nl + 1);
          
          if (line) _handleLine(line); // Schickt die fertige Zeile an den Parser
        }
      }
    } catch(_) {} // Fehler leise schlucken (z. B. wenn das USB-Kabel gezogen wird)
  })();
}

// ── GEMEINSAME HANDLER-LOGIK FÜR EINGEHENDE DATEN ────────────

/**
 * Analysiert jede vom Wemos/ESP32 ankommende Zeile.
 * @param {string} line - Die reine Textzeile
 */
function _handleLine(line) {
  // Protokoll von der Brücke (Wemos Dongle)
  if (line.startsWith('WEMOS:')) {
    const msg = line.slice(6);
    
    // Status der Wi-Fi-Verbindung zwischen Wemos (Bridge) und ESP32 (Roboter)
    if (msg.startsWith('WIFI_OK:'))   log('✓ WiFi: ' + msg.slice(8), 'success');
    else if (msg === 'READY') {
      _telemSet('tWemos', 'Bridge OK', '#58A6FF');
      
      // Sobald die Bridge bereit ist, pingen wir den Roboter an, 
      // damit er anfängt, aktiv Sensordaten (Telemetrie) zu schicken.
      serial.write('PING'); 
    }
    else if (msg === 'WIFI_LOST')     log('⚠ WiFi verloren', 'warn');
    
    // Bestätigung, dass die Bridge ein Kommando an den Roboter weitergeleitet hat
    else if (msg.startsWith('SENT:')) document.getElementById('tCmd').textContent = msg.slice(5);
    return;
  }
  
  // Protokoll vom Endgerät (ESP32-S3 Roboter)
  if (line.startsWith('ESP32:')) {
    const rest  = line.slice(6);
    const colon = rest.indexOf(':');
    if (colon === -1) return; // Ungültiges Format verwerfen
    
    // Trennt Typ ("ENC", "COLOR") vom Wert und schickt es an die Telemetrie-Funktion
    _handleTelem(rest.slice(0, colon).toUpperCase(), rest.slice(colon + 1));
    return;
  }
  
  // Wenn es weder WEMOS noch ESP32 ist, loggen wir es einfach als rohen Text
  log('[Serial] ' + line, 'sys');
}

/**
 * Aktualisiert die Telemetrie-Leiste im UI basierend auf dem Datentyp.
 */
function _handleTelem(type, value) {
  switch (type) {
    case 'STATUS': {
      // Ist der Roboter fertig mit der Bewegung oder wartet er auf Befehle?
      const ok = value === 'DONE' || value === 'IDLE';
      _telemSet('tStatus', value, ok ? '#3FB950' : '#58A6FF');
      break;
    }
    case 'ENC': {
      // Erwartetes Format: GradMotorA,GradMotorB
      const [a, b] = value.split(',');
      document.getElementById('tEncA').textContent = a ?? '—';
      document.getElementById('tEncB').textContent = b ?? '—';
      break;
    }
    case 'COLOR': {
      // Erwartetes Format: R,G,B,FarbenName (z.B. "255,0,0,Rot")
      const parts = value.split(',');
      if (parts.length >= 4) {
        const r = parts[0];
        const g = parts[1];
        const b = parts[2];
        const name = parts[3];

        const elText = document.getElementById('tColorText');
        const elBox = document.getElementById('tColorBox');
        
        if (elText) {
          elText.textContent = name;
          elText.style.color = '#cdd9e5'; 
        }
        if (elBox) {
          // Malt den kleinen UI-Kasten in der echten, vom Sensor gemessenen Farbe aus
          elBox.style.backgroundColor = `rgb(${r}, ${g}, ${b})`;
        }
      }
      break;
    }
    case 'WARN': 
      log('⚠ ESP32: ' + value, 'warn'); 
      break;
    case 'CMD':  
      // Der Roboter bestätigt das Kommando, an dem er gerade arbeitet
      document.getElementById('tCmd').textContent = value; 
      break;
  }
}

// ── ÖFFENTLICHE EXPORTS (Aufgerufen durch UI-Buttons) ────────

/**
 * Toggelt den Verbindungsstatus (Verbinden / Trennen).
 */
export async function conectarSerial() {
  if (serial.connected) {
    await serial.disconnect();
    return;
  }
  try {
    await serial.connect();
  } catch(e) {
    log('Fehler: ' + (e.message || e), 'error');
  }
}

/**
 * Erzwungene Trennung.
 */
export async function desconectarSerial() {
  await serial.disconnect();
}

/**
 * Sendeingabe einer rohen Zeile (z.B. aus der UI-Konsole).
 */
export async function serialSend(line) {
  await serial.write(line);
}

/**
 * Kompiliert das Blockly-Programm und sendet es iterativ an den Roboter.
 */
export async function enviarAPi() {
  if (!serial.connected) { log('Verbinde zuerst den Wemos.', 'warn'); return; }

  // Ruft die Array-Liste der Textbefehle aus dem Generator ab
  const cmds = generateCommands();
  if (cmds.length === 0) { log('Keine Befehle vorhanden. Füge Blöcke hinzu.', 'warn'); return; }

  runAbortado = false; // Reset des Not-Stopps
  log(`▶ Sende Programm (${cmds.length} Zeilen)…`, 'success');

  // Sicherheit: Beende alle aktuellen Bewegungen, bevor das neue Programm hochgeladen wird
  await serial.write('STOP');
  await new Promise(r => setTimeout(r, 150)); // Kurze Pause für den Buffer

  // Senden in einer asynchronen Schleife
  for (const cmd of cmds) {
    if (runAbortado) { log('⛔ Abgebrochen.', 'warn'); return; } // Abbruchbedingung prüfen
    
    try {
      await serial.write(cmd);
      // Winzige Verzögerung, um den USB-Serial-Puffer des ESP32-Pico nicht zu überfluten
      await new Promise(r => setTimeout(r, 10));
    } catch(e) {
      log('Fehler beim Senden: ' + e, 'error'); return;
    }
  }
  log('✓ Programm gesendet. Roboter führt es aus…', 'success');
}

/**
 * Löst den Not-Stopp aus. Bricht Uploads ab und sendet STOP an die Motoren.
 */
export async function detenerRobot() {
  runAbortado = true;
  try {
    await serial.write('STOP');
    log('■ STOP gesendet.', 'warn');
  } catch(_) {}
}

// ── INTERNE UI HILFSFUNKTIONEN ───────────────────────────────

/**
 * Aktualisiert Buttons und Texte in der Oberfläche basierend auf dem Verbindungsstatus.
 */
function _setUI(connected, portName = '') {
  document.getElementById('btnConnect').innerHTML = connected
    ? `<div class="conn-dot connected"></div> ${portName || 'Trennen'}`
    : '<div class="conn-dot"></div> Connect Wemos';
    
  const bt = document.getElementById('btStatusText');
  bt.textContent = connected ? `Verbunden (${portName})` : 'Getrennt';
  bt.style.color  = connected ? '#3FB950' : '#555';
  
  _telemSet('tWemos', connected ? 'Verbunden' : '—', connected ? '#3FB950' : '#555');
}

/**
 * Helfer, um ein einzelnes Element in der Telemetrie-Leiste zu aktualisieren.
 */
function _telemSet(id, text, color) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.style.color  = color;
}