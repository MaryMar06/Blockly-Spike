/**
 * main.js — Der Orchestrator
 * Dieses Skript initialisiert den Workspace, überwacht Änderungen 
 * an den Blöcken in Echtzeit und verbindet UI-Buttons mit Funktionen.
 */

// Importe der Module
import './blocks/blockDefinitions.js';
import { initWorkspace, getWorkspace } from './blockly/workspace.js';
import { generateCode, highlightCommands } from './generators/codeRenderer.js';
import { switchTab, clearConsole, log } from './ui/uiHelpers.js';
import { conectarSerial, enviarAPi, detenerRobot } from './serial/serialConnection.js';

// ── INITIALISIERUNG ──────────────────────────────────────────
// Erstellt den Blockly-Arbeitsbereich im DIV-Container 'blocklyDiv'
const workspace = initWorkspace('blocklyDiv');

/**
 * Aktualisiert das "Commands"-Panel.
 * Holt den generierten Text, wendet Syntax-Highlighting an 
 * und zählt die aktuelle Anzahl der Blöcke für die Statusleiste.
 */
function updateCode() {
  const text = generateCode();
  // Hinweis: Hier wird 'pythonCode' als ID referenziert (in index.html definierter Container)
  document.getElementById('pythonCode').innerHTML = highlightCommands(text);
  document.getElementById('blockCount').textContent = workspace.getAllBlocks().length;
}

// ── EVENT-LISTENER ──────────────────────────────────────────
// 'addChangeListener' überwacht jede Interaktion auf dem Arbeitsbereich.
// UI-Events (wie Scrollen) werden ignoriert, um die Performance zu schonen.
workspace.addChangeListener(e => {
  if (e.type !== Blockly.Events.UI && e.type !== Blockly.Events.VIEWPORT_CHANGE) {
    updateCode();
  }
});

// Initiale Anzeige beim Laden
updateCode();

// Sorgt dafür, dass der Workspace bei Fenstergrößenänderungen korrekt skaliert.
window.addEventListener('resize', () => Blockly.svgResize(workspace));

// ── GLOBAL EXPORTS (Schnittstelle zum HTML) ──────────────────
// Da die HTML-Buttons einfache onclick-Attribute verwenden, müssen die
// Funktionen explizit an das 'window'-Objekt angehängt werden.
window.switchTab      = switchTab;
window.clearConsole   = clearConsole;
window.conectarSerial = conectarSerial;
window.enviarAPi      = enviarAPi;
window.detenerRobot   = detenerRobot;

// ── PROJEKT-VERWALTUNG ───────────────────────────────────────

/**
 * Serialisiert den aktuellen Workspace in eine XML-Datei.
 * Dies ermöglicht das Speichern von Projekten lokal auf dem Rechner.
 */
window.saveProject = function () {
  const xml  = Blockly.Xml.workspaceToDom(workspace);
  const text = Blockly.Xml.domToText(xml);
  const blob = new Blob([text], { type: 'text/xml' });
  const a    = Object.assign(document.createElement('a'), {
    href: URL.createObjectURL(blob), download: 'robot_project.xml'
  });
  a.click();
};

/**
 * Lädt ein XML-Projektfile und injiziert es in den bestehenden Workspace.
 */
window.loadProject = function (event) {
  const file   = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = e => {
    try {
      const xml = Blockly.utils.xml.textToDom(e.target.result);
      Blockly.Xml.clearWorkspaceAndLoadFromXml(xml, workspace);
      log('✓ Proyecto cargado', 'success');
    } catch (err) {
      log('Error al cargar: ' + err, 'error');
    }
  };
  reader.readAsText(file);
};

/**
 * Ermöglicht den manuellen Versand von Befehlen über das Konsolen-Inputfeld.
 */
window.sendConsoleCmd = async function () {
  const input = document.getElementById('consoleInput');
  const cmd   = input.value.trim();
  if (!cmd) return;
  input.value = '';
  log('> ' + cmd, 'info'); // Loggt den gesendeten Befehl in die Konsole
  try {
    // Dynamischer Import stellt sicher, dass der Befehl auch bei späten Initialisierungen funktioniert
    const { serialSend } = await import('./serial/serialConnection.js');
    await serialSend(cmd);
  } catch (e) {
    log('Error: ' + e, 'error');
  }
};

log('Robot Controller listo.', 'success');