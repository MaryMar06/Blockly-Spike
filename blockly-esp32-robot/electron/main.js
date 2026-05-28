/**
 * electron/main.js — Proceso principal de Electron
 * Abre la ventana, expone serialport al renderer via IPC
 */

const { app, BrowserWindow, ipcMain } = require('electron');
const path       = require('path');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// ── VENTANA ──────────────────────────────────────────────────
function createWindow() {
  const win = new BrowserWindow({
    width:  1400,
    height: 900,
    minWidth:  900,
    minHeight: 600,
    title: 'SPIKE Pi',
    icon: path.join(__dirname, '..', 'assets', 'icon.png'),
    webPreferences: {
      preload:         path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration:  false,
    }
  });

  win.loadFile(path.join(__dirname, '..', 'index.html'));

  // En desarrollo puedes descomentar esto:
  // win.webContents.openDevTools();

  // Quitar menú nativo (opcional — la app tiene su propio header)
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

// ── SERIAL IPC ───────────────────────────────────────────────
// El renderer llama a window.serialAPI.xxx()
// El main process maneja el puerto real con Node serialport

let port   = null;
let parser = null;
let win    = null;

// Guardar referencia a la ventana para mandar datos de vuelta
app.on('browser-window-created', (_, w) => { win = w; });

// Listar puertos disponibles
ipcMain.handle('serial:list', async () => {
  const ports = await SerialPort.list();
  return ports.map(p => ({ path: p.path, manufacturer: p.manufacturer || '' }));
});

// Abrir puerto
ipcMain.handle('serial:open', async (_, portPath) => {
  if (port && port.isOpen) {
    try { await new Promise(r => port.close(r)); } catch(_) {}
  }
  return new Promise((resolve, reject) => {
    port = new SerialPort({ path: portPath, baudRate: 115200 }, err => {
      if (err) return reject(err.message);

      parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));
      parser.on('data', line => {
        // Mandar cada línea al renderer
        if (win && !win.isDestroyed()) {
          win.webContents.send('serial:data', line.trim());
        }
      });

      port.on('error', e => {
        if (win && !win.isDestroyed()) win.webContents.send('serial:error', e.message);
      });
      port.on('close', () => {
        if (win && !win.isDestroyed()) win.webContents.send('serial:closed');
      });

      resolve('ok');
    });
  });
});

// Escribir línea
ipcMain.handle('serial:write', async (_, line) => {
  if (!port || !port.isOpen) throw new Error('Puerto no abierto');
  return new Promise((resolve, reject) => {
    port.write(line + '\n', err => err ? reject(err.message) : resolve('ok'));
  });
});

// Cerrar puerto
ipcMain.handle('serial:close', async () => {
  if (port && port.isOpen) {
    return new Promise(r => port.close(r));
  }
});
