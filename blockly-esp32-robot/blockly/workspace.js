import { toolboxXml } from './toolbox.js';

// ============================================================
// WORKSPACE INITIALISIERUNG
// ============================================================
// Dieses Modul verwaltet die Lebensdauer des Blockly-Arbeitsbereichs.
// Es sorgt dafür, dass die grafische Oberfläche korrekt in den 
// HTML-Container eingebettet wird.

let workspace = null;

/**
 * Initialisiert den Blockly-Workspace im angegebenen DOM-Element.
 * @param {string} containerId - Die ID des DIV-Containers in index.html (z.B. 'blocklyDiv').
 * @returns {Object} Das Blockly-Workspace-Objekt.
 */
export function initWorkspace(containerId) {
  // Blockly.inject injiziert den Editor in das DOM-Element.
  // 'renderer: zelos' sorgt für das moderne Design mit abgerundeten Kanten.
  workspace = Blockly.inject(containerId, {
    toolbox: toolboxXml,      // Lädt die Kategorien und Blöcke aus toolbox.js
    renderer: 'zelos',        // Der Renderer, der für das SPIKE-lookalike Design sorgt
    zoom: { 
      controls: true,         // Zoom-Buttons (+/-) anzeigen
      wheel: true,            // Mausrad-Zoom erlauben
      startScale: 0.9         // Initialer Zoom-Faktor
    },
    trashcan: true            // Aktiviert den Papierkorb zum Löschen von Blöcken
  });
  
  return workspace;
}

/**
 * Gibt eine Referenz auf den aktuell aktiven Workspace zurück.
 * Hilfreich für andere Module (z.B. für den Generator), um auf 
 * die Blöcke zuzugreifen, ohne die Instanz neu erstellen zu müssen.
 */
export function getWorkspace() {
  return workspace;
}