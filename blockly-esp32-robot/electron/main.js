/**
 * electron/main.js — Electron-Hauptprozess
 * Öffnet das Fenster, stellt serialport über IPC dem Renderer zur Verfügung
 */

const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// Fenster
function createWindow() {
  const win = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 900,
    minHeight: 600,
    title: 'SPIKE Pi',
    icon: path.join(__dirname, '..', 'assets', 'icon.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    }
  });

  win.loadFile(path.join(__dirname, '..', 'index.html'));

  // Im Entwicklungsmodus kann dies aktiviert werden
  // win.webContents.openDevTools();

  // Native Menüleiste entfernen (optional — die App hat einen eigenen Header)
  win.setMenuBarVisibility(false);
}

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

// Serielle IPC-Kommunikation
// Der Renderer ruft window.serialAPI.xxx() auf
// Der Hauptprozess verwaltet den echten Port über Node serialport

let port = null;
let parser = null;
let win = null;

// Fensterreferenz speichern, um Daten zurückzusenden
app.on('browser-window-created', (_, w) => { win = w; });

// Verfügbare Ports auflisten
ipcMain.handle('serial:list', async () => {
  const ports = await SerialPort.list();
  return ports.map(p => ({ path: p.path, manufacturer: p.manufacturer || '' }));
});

// Port öffnen
ipcMain.handle('serial:open', async (_, portPath) => {
  if (port && port.isOpen) {
    try { await new Promise(r => port.close(r)); } catch (_) {}
  }

  return new Promise((resolve, reject) => {
    port = new SerialPort({ path: portPath, baudRate: 115200 }, err => {
      if (err) return reject(err.message);

      parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

      parser.on('data', line => {
        // Jede Zeile an den Renderer senden
        if (win && !win.isDestroyed()) {
          win.webContents.send('serial:data', line.trim());
        }
      });

      port.on('error', e => {
        if (win && !win.isDestroyed()) {
          win.webContents.send('serial:error', e.message);
        }
      });

      port.on('close', () => {
        if (win && !win.isDestroyed()) {
          win.webContents.send('serial:closed');
        }
      });

      resolve('ok');
    });
  });
});

// Zeile schreiben
ipcMain.handle('serial:write', async (_, line) => {
  if (!port || !port.isOpen) throw new Error('Port nicht geöffnet');
  return new Promise((resolve, reject) => {
    port.write(line + '\n', err => err ? reject(err.message) : resolve('ok'));
  });
});

// Port schließen
ipcMain.handle('serial:close', async () => {
  if (port && port.isOpen) {
    return new Promise(r => port.close(r));
  }
});
