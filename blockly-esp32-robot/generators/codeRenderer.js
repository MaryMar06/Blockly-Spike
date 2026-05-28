import { generateCommandsText } from './udpGenerator.js';

// ============================================================
// COMMAND-RENDERER & SYNTAX-HIGHLIGHTING
// ============================================================
// Dieses Modul wandelt die generierten Textbefehle in HTML um,
// um sie im Seitenpanel mit Syntax-Hervorhebung anzuzeigen.

// Definiert die Farbzuordnung basierend auf dem Befehls-Typ (Schlüsselwort).
// Dies hilft dem Benutzer, die Programmstruktur visuell besser zu erfassen.
const COLORS = {
  FORWARD:     '#58A6FF',
  BACKWARD:    '#58A6FF',
  TURN_LEFT:   '#79C0FF',
  TURN_RIGHT:  '#79C0FF',
  TANK:        '#79C0FF',
  MOTOR_A:     '#FFA657',
  MOTOR_B:     '#FFA657',
  WAIT:        '#F2CC60',
  STOP:        '#FF7B72',
  SET:         '#E8C06A',
  REPEAT:      '#D2A8FF',
  END_REPEAT:  '#9A7FBF',
  FOREVER:     '#D2A8FF',
  END_FOREVER: '#9A7FBF',
  IF:          '#D2A8FF',
  ELSE:        '#D2A8FF',
  END_IF:      '#9A7FBF',
  DEF:         '#7EE787',
  END_DEF:     '#5AAD65',
  CALL:        '#7EE787',
};

// Hilfsfunktion: Ruft den generierten Befehls-Text aus dem Generator ab.
export function generateCode() {
  return generateCommandsText();
}

/**
 * Wandelt einen Rohtext-Block in HTML um und wendet Farben an.
 * @param {string} text - Der Zeilen-basierte Befehls-String.
 * @returns {string} - HTML-String mit formatierten Spans.
 */
export function highlightCommands(text) {
  // Fallback, wenn keine Befehle vorhanden sind
  if (!text || text === '— No commands —') {
    return '<span style="color:#555;font-style:italic">— No commands —</span>';
  }

  // HTML-Escaping (Sicherheitsmaßnahme) und Aufteilung in Zeilen
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .split('\n')
    .map(line => {
      if (!line.trim()) return '';
      
      // Prüft, ob die Zeile mit einer Nummer beginnt (z.B. "01  FORWARD:...")
      const match = line.match(/^(\d+\s+)(.+)$/);
      
      // Wenn kein Match gefunden wird, einfache graue Darstellung
      if (!match) return `<span style="color:#CDD9E5">${line}</span>`;
      
      const [, num, rest] = match;
      
      // Extrahiert den Befehlstyp (z.B. "FORWARD") für die Farbwahl
      const key   = rest.split(':')[0].toUpperCase();
      const color = COLORS[key] || '#CDD9E5';
      
      // Gibt die Zeile mit farbiger Nummer und spezifischem Befehl-Color zurück
      return `<span style="color:#444">${num}</span><span style="color:${color}">${rest}</span>`;
    })
    .filter(Boolean) // Entfernt leere Zeilen
    .join('\n');
}

