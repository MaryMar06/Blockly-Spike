/**
 * electron/preload.js
 * Expone la API serial de Node al renderer de forma segura (contextBridge)
 * El renderer usa window.serialAPI igual que antes usaba navigator.serial
 */

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('serialAPI', {
  // Listar puertos disponibles
  list: () => ipcRenderer.invoke('serial:list'),

  // Abrir puerto por path (ej: "COM6" o "/dev/ttyUSB0")
  open: (path) => ipcRenderer.invoke('serial:open', path),

  // Escribir una línea
  write: (line) => ipcRenderer.invoke('serial:write', line),

  // Cerrar puerto
  close: () => ipcRenderer.invoke('serial:close'),

  // Escuchar datos entrantes (líneas del Wemos/ESP32)
  onData: (callback) => {
    ipcRenderer.on('serial:data',   (_, line) => callback(line));
  },
  onError: (callback) => {
    ipcRenderer.on('serial:error',  (_, msg)  => callback(msg));
  },
  onClose: (callback) => {
    ipcRenderer.on('serial:closed', ()        => callback());
  },

  // Limpiar listeners (llamar al desconectar)
  removeAllListeners: () => {
    ipcRenderer.removeAllListeners('serial:data');
    ipcRenderer.removeAllListeners('serial:error');
    ipcRenderer.removeAllListeners('serial:closed');
  }
});

// Exponer si estamos en Electron (para que serialConnection.js sepa qué API usar)
contextBridge.exposeInMainWorld('isElectron', true);
