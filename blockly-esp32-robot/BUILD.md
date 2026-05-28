# SPIKE Pi — Build Instructions

## Estructura de carpetas esperada

```
spike-pi/
├── electron/
│   ├── main.js
│   └── preload.js
├── styles/
├── blocks/
├── blockly/
├── generators/
├── serial/
├── ui/
├── assets/
│   ├── icon.png     (512x512 recomendado)
│   ├── icon.ico     (Windows)
│   └── icon.icns    (Mac)
├── index.html
├── main.js          (tu main.js de la app)
└── package.json
```

## 1. Instalar dependencias

```bash
npm install
```

## 2. Probar en desarrollo (sin compilar)

```bash
npm start
```

## 3. Compilar ejecutables

### Solo Windows (.exe installer)
```bash
npm run dist:win
```

### Solo Mac (.dmg)
```bash
npm run dist:mac
```

### Solo Linux (.AppImage)
```bash
npm run dist:linux
```

### Los tres a la vez
```bash
npm run dist:all
```

Los ejecutables se generan en la carpeta `dist/`.

---

## Notas importantes

### Icono
Necesitas crear los tres formatos de icono:
- `assets/icon.png` — 512×512px PNG (usado en Linux y como base)
- `assets/icon.ico` — formato Windows (puedes convertir con https://icoconvert.com)
- `assets/icon.icns` — formato Mac (en Mac: `iconutil`, en Windows: usa https://cloudconvert.com)

Si no tienes iconos aún, puedes omitir las líneas de icono en package.json temporalmente.

### Cross-compilation
- Compilar para Windows desde Mac/Linux requiere Wine o hacerlo en Windows
- Compilar para Mac desde Windows/Linux NO es posible sin un Mac (restricción de Apple)
- Linux AppImage se puede compilar desde cualquier sistema

### Módulo serialport
electron-builder reconstruye automáticamente los módulos nativos (como serialport)
para el target correcto. Si hay errores de native modules, ejecuta:
```bash
npx electron-rebuild
```

### Diferencia con la versión web
- En Electron aparece un selector de puerto al conectar (si hay varios puertos USB)
- Si solo hay un puerto USB conectado, se conecta automáticamente
- No requiere Chrome — es un ejecutable independiente
- WebSerial sigue funcionando si abres index.html en Chrome directamente
