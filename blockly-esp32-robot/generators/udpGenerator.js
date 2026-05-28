// ============================================================
// COMMAND GENERATOR — Wandelt Blockly-Blöcke in Befehlsprotokoll
// ============================================================
// Dieser Generator übersetzt die visuelle Logik des Benutzers 
// in eine zeilenbasierte Text-Struktur. Der ESP32-S3 empfängt 
// diesen Befehlsstrom und verarbeitet ihn als sequenziellen Stack.

const udpGen = new Blockly.Generator('UDP');
udpGen.INDENT = ''; // Keine Einrückung nötig, da Protokoll zeilenbasiert
udpGen.ORDER_ATOMIC = 0;
udpGen.ORDER_NONE   = 99;

// Die "Scrub"-Funktion steuert, wie Blöcke verkettet werden.
// Sie nimmt den aktuellen Block, führt ihn aus und ruft rekursiv 
// den nächsten Block (nextConnection) auf, um den Befehlsstrom zu bilden.
udpGen.scrub_ = function(block, code, opt_thisOnly) {
  const current = (code === null || code === undefined) ? '' : code;
  if (opt_thisOnly || !block.nextConnection?.targetBlock()) return current;
  const next = udpGen.blockToCode(block.nextConnection.targetBlock());
  return current + (Array.isArray(next) ? next[0] : (next || ''));
};

// Hilfsfunktion: Registriert einen Generator für einen bestimmten Blocktyp
function add(type, fn) { udpGen.forBlock[type] = fn; }

// ── WERTE-BLÖCKE: Liefern evaluierbare Ausdrücke (z.B. für Zahlen oder Bedingungen)
add('math_number',    b => [String(b.getFieldValue('NUM')), udpGen.ORDER_ATOMIC]);
add('variables_get',  b => [b.getFieldValue('VAR'), udpGen.ORDER_ATOMIC]);

add('sp_op_random', b => {
  const f = udpGen.valueToCode(b, 'FROM', udpGen.ORDER_NONE) || '1';
  const t = udpGen.valueToCode(b, 'TO',   udpGen.ORDER_NONE) || '10';
  return [`random(${f},${t})`, udpGen.ORDER_ATOMIC];
});

add('math_arithmetic', b => {
  const OPS = { ADD:'+', MINUS:'-', MULTIPLY:'*', DIVIDE:'/' };
  const op = OPS[b.getFieldValue('OP')];
  const a  = udpGen.valueToCode(b, 'A', udpGen.ORDER_NONE) || '0';
  const bv = udpGen.valueToCode(b, 'B', udpGen.ORDER_NONE) || '0';
  return [`(${a}${op}${bv})`, udpGen.ORDER_ATOMIC];
});

add('logic_compare', b => {
  const OPS = { EQ:'==', LT:'<', GT:'>' };
  const op = OPS[b.getFieldValue('OP')];
  const a  = udpGen.valueToCode(b, 'A', udpGen.ORDER_NONE) || '0';
  const bv = udpGen.valueToCode(b, 'B', udpGen.ORDER_NONE) || '0';
  return [`(${a}${op}${bv})`, udpGen.ORDER_ATOMIC];
});

// ── VARIABLEN-Zuweisung
add('variables_set', b => {
  const v = udpGen.valueToCode(b, 'VALUE', udpGen.ORDER_NONE) || '0';
  return `SET:${b.getFieldValue('VAR')}:${v}\n`;
});

// ── MOTOREN-STEUERUNG
// Hier werden die Dropdowns (Motor A-D, Richtung) in das Protokoll übersetzt.
add('sp_motor_run', b => {
  const id  = b.getFieldValue('MOTOR');
  const dir = b.getFieldValue('DIR') === 'forward' ? 1 : 0;
  const sp  = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `MOTOR_${id}:9999:${dir}:${sp}\n`;
});

add('sp_motor_run_rotations', b => {
  const id  = b.getFieldValue('MOTOR');
  const dir = b.getFieldValue('DIR') === 'forward' ? 1 : 0;
  const r   = udpGen.valueToCode(b, 'ROTATIONS', udpGen.ORDER_NONE) || '1';
  const sp  = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `MOTOR_${id}:${r}:${dir}:${sp}\n`;
});

add('sp_motor_run_seconds', b => {
  const id  = b.getFieldValue('MOTOR');
  const dir = b.getFieldValue('DIR') === 'forward' ? 1 : 0;
  const s   = udpGen.valueToCode(b, 'SECONDS', udpGen.ORDER_NONE) || '2';
  const sp  = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  // Konvertierung der Richtung in positive/negative Motorwerte für TANK-Befehl
  const vA  = id === 'A' ? (dir ? sp : `(0-${sp})`) : '0';
  const vB  = id === 'B' ? (dir ? sp : `(0-${sp})`) : '0';
  return `TANK:${vA}:${vB}:${s}\n`;
});

add('sp_motor_stop',      _ => 'STOP\n');
add('sp_motor_set_speed', _ => ''); // Aktuell leer: Sollte hier implementiert werden, falls das Protokoll es unterstützt

// ── BEWEGUNGS-KOMBINATIONEN (High-Level)
add('sp_move_forward', b => {
  const r  = udpGen.valueToCode(b, 'ROTATIONS', udpGen.ORDER_NONE) || '2';
  const sp = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `FORWARD:${r}:${sp}\n`;
});

add('sp_move_backward', b => {
  const r  = udpGen.valueToCode(b, 'ROTATIONS', udpGen.ORDER_NONE) || '2';
  const sp = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `BACKWARD:${r}:${sp}\n`;
});

add('sp_move_turn_left', b => {
  const d  = udpGen.valueToCode(b, 'DEGREES', udpGen.ORDER_NONE) || '90';
  const sp = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `TURN_LEFT:${d}:${sp}\n`;
});

add('sp_move_turn_right', b => {
  const d  = udpGen.valueToCode(b, 'DEGREES', udpGen.ORDER_NONE) || '90';
  const sp = udpGen.valueToCode(b, 'SPEED', udpGen.ORDER_NONE) || '5';
  return `TURN_RIGHT:${d}:${sp}\n`;
});

add('sp_move_tank', b => {
  const l = udpGen.valueToCode(b, 'LEFT', udpGen.ORDER_NONE) || '5';
  const r = udpGen.valueToCode(b, 'RIGHT', udpGen.ORDER_NONE) || '5';
  return `TANK:${l}:${r}:1\n`;
});
add('sp_move_stop', _ => 'STOP\n');

// ── KONTROLLSTRUKTUREN
add('sp_event_program_start', _ => ''); // Start-Block erzeugt keinen Text
add('sp_ctrl_wait',     b => `WAIT:${udpGen.valueToCode(b,'SECS',udpGen.ORDER_NONE)||'1'}\n`);
add('sp_ctrl_stop_all', _ => 'STOP\n');

// Schleifen und If-Abfragen: 'statementToCode' holt den Inhalt innerhalb der Klammern
add('sp_ctrl_repeat', b => {
  const times = udpGen.valueToCode(b, 'TIMES', udpGen.ORDER_NONE) || '1';
  const inner = udpGen.statementToCode(b, 'DO') || '';
  return `REPEAT:${times}\n${inner}END_REPEAT\n`;
});

add('sp_ctrl_forever', b => {
  const inner = udpGen.statementToCode(b, 'DO') || '';
  return `FOREVER\n${inner}END_FOREVER\n`;
});

add('sp_ctrl_if', b => {
  const cond  = udpGen.valueToCode(b, 'COND', udpGen.ORDER_NONE) || '0';
  const inner = udpGen.statementToCode(b, 'DO') || '';
  return `IF:${cond}\n${inner}END_IF\n`;
});

add('sp_ctrl_if_else', b => {
  const cond     = udpGen.valueToCode(b, 'COND', udpGen.ORDER_NONE) || '0';
  const doBody   = udpGen.statementToCode(b, 'DO')   || '';
  const elseBody = udpGen.statementToCode(b, 'ELSE') || '';
  return `IF:${cond}\n${doBody}ELSE\n${elseBody}END_IF\n`;
});

// ── EIGENE FUNKTIONEN (My Blocks / Procedures)
add('procedures_defnoreturn', b => {
  const name  = b.getFieldValue('NAME');
  const inner = udpGen.statementToCode(b, 'STACK') || '';
  return `DEF:${name}\n${inner}END_DEF\n`;
});

add('procedures_callnoreturn', b => {
  const name = b.getFieldValue('NAME');
  const args = (b.arguments_ || []).map((_, i) =>
    udpGen.valueToCode(b, 'ARG' + i, udpGen.ORDER_NONE) || '0'
  );
  return `CALL:${name}${args.length ? ':' + args.join(':') : ''}\n`;
});

// ── EXPORT-FUNKTIONEN

/**
 * Sammelt alle Top-Blöcke auf dem Arbeitsbereich und serialisiert sie.
 * @returns {Array<string>} Liste von Befehlen
 */
export function generateCommands() {
  const ws = Blockly.getMainWorkspace();
  let raw = '';
  for (const block of ws.getTopBlocks(true)) {
    const r = udpGen.blockToCode(block);
    raw += Array.isArray(r) ? r[0] : (r || '');
  }
  return raw.split('\n').map(l => l.trim()).filter(l => l.length > 0);
}

/**
 * Erzeugt den formatierten String für das Seitenpanel (inkl. Zeilennummerierung).
 */
export function generateCommandsText() {
  const cmds = generateCommands();
  if (cmds.length === 0) return '— No commands —';
  return cmds.map((c, i) => `${String(i+1).padStart(2,'0')}  ${c}`).join('\n');
}