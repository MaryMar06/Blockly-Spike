// ============================================================
// UI HELPERS & TABS
// ============================================================
// Dieses Modul stellt Hilfsfunktionen bereit, um den Status 
// der Benutzeroberfläche zu steuern (Tab-Wechsel) und 
// systeminterne Meldungen in der Konsole auszugeben.

/**
 * Wechselt zwischen den Tabs im Seitenpanel (Commands vs. Console).
 * @param {string} tab - Der Bezeichner des Tabs ('code' oder 'console').
 */
export function switchTab(tab) {
  // Aktualisiert die 'active'-Klasse der Tab-Buttons für das visuelle Feedback
  document.querySelectorAll('.side-tab').forEach((t, i) => 
    t.classList.toggle('active', (i === 0 && tab === 'code') || (i === 1 && tab === 'console'))
  );
  
  // Entfernt die 'active'-Klasse von allen Inhaltsbereichen und blendet sie aus
  document.querySelectorAll('.side-content').forEach(c => c.classList.remove('active'));
  
  // Zeigt den gewählten Inhaltsbereich an
  document.getElementById('tab-' + tab).classList.add('active');
}

/**
 * Leert die Hauptkonsole im unteren Bereich.
 */
export function clearConsole() { 
  document.getElementById('mainConsole').innerHTML = ''; 
}

/**
 * Schreibt eine neue Nachricht in die Konsole.
 * @param {string|number} msg - Die Nachricht, die protokolliert werden soll.
 * @param {string} type - Der Typ der Nachricht (wird als CSS-Klasse genutzt: 'sys', 'error', 'warn', 'success', 'info').
 */
export function log(msg, type = 'sys') {
  const con = document.getElementById('mainConsole');
  const div = document.createElement('div');
  
  // CSS-Klasse hinzufügen, um Nachrichten farblich zu unterscheiden
  div.className = 'console-line ' + type;
  
  // Erstellt einen Zeitstempel im Format HH:MM:SS
  const ts = new Date().toLocaleTimeString('en', {
    hour12: false, 
    hour: '2-digit', 
    minute: '2-digit', 
    second: '2-digit'
  });
  
  // HTML-Escaping (Sicherheitsmaßnahme gegen XSS, falls Nachrichten Sonderzeichen enthalten)
  const safeMsg = String(msg).replace(/</g, '&lt;').replace(/>/g, '&gt;');
  
  // Erstellt den finalen Konsoleneintrag mit Zeitstempel und Nachricht
  div.innerHTML = `<span style="color:#444">[${ts}]</span> ${safeMsg}`;
  
  // Fügt die Zeile hinzu und scrollt automatisch zum Ende der Konsole
  con.appendChild(div);
  con.scrollTop = con.scrollHeight;
}