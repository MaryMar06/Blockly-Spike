// ============================================================
// BLOCK DEFINITIONEN
// ============================================================
// Dieser Bereich definiert das visuelle Erscheinungsbild und die 
// Logik-Struktur der Blöcke in Blockly unter Verwendung eines JSON-Arrays.
// 'type' ist die eindeutige ID, 'message0' definiert den Text/Inputs 
// und 'args0' spezifiziert die Eingabetypen (Dropdowns, Zahlen, Statements).

Blockly.defineBlocksWithJsonArray([

  // --- MOTOREN-BLÖCKE ---
  // Diese Blöcke steuern einzelne Motoren (A-D) mit spezifischen Laufparametern.
  { "type": "sp_motor_run", "message0": "⚙ Motor %1 run %2 speed %3 (1-10)", "args0": [ {"type":"field_dropdown","name":"MOTOR","options":[["A","A"],["B","B"],["C","C"],["D","D"]]}, {"type":"field_dropdown","name":"DIR","options":[["→ forward","forward"],["← backward","backward"]]}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#E64A19" },
  { "type": "sp_motor_run_rotations", "message0": "⚙ Motor %1 run %2 %3 rotations speed %4 (1-10)", "args0": [ {"type":"field_dropdown","name":"MOTOR","options":[["A","A"],["B","B"],["C","C"],["D","D"]]}, {"type":"field_dropdown","name":"DIR","options":[["→ forward","forward"],["← backward","backward"]]}, {"type":"input_value","name":"ROTATIONS","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#E64A19" },
  { "type": "sp_motor_run_seconds", "message0": "⚙ Motor %1 run %2 for %3 sec speed %4 (1-10)", "args0": [ {"type":"field_dropdown","name":"MOTOR","options":[["A","A"],["B","B"],["C","C"],["D","D"]]}, {"type":"field_dropdown","name":"DIR","options":[["→ forward","forward"],["← backward","backward"]]}, {"type":"input_value","name":"SECONDS","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#E64A19" },
  { "type": "sp_motor_stop", "message0": "⚙ Motor %1 stop", "args0": [{"type":"field_dropdown","name":"MOTOR","options":[["A","A"],["B","B"],["C","C"],["D","D"],["all","all"]]}], "previousStatement":null,"nextStatement":null, "colour":"#E64A19" },
  { "type": "sp_motor_set_speed", "message0": "⚙ Motor %1 set speed %2 (1-10)", "args0": [ {"type":"field_dropdown","name":"MOTOR","options":[["A","A"],["B","B"],["C","C"],["D","D"]]}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#E64A19" },

  // --- BEWEGUNGS-BLÖCKE ---
  // Diese Blöcke kombinieren mehrere Motoren für komplexere Roboter-Manöver (Tank-Steuerung, Drehen, Vorwärts).
  { "type": "sp_move_forward", "message0": "🤖 move forward %1 rotations speed %2 (1-10)", "args0": [ {"type":"input_value","name":"ROTATIONS","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },
  { "type": "sp_move_backward", "message0": "🤖 move backward %1 rotations speed %2 (1-10)", "args0": [ {"type":"input_value","name":"ROTATIONS","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },
  { "type": "sp_move_turn_left", "message0": "🤖 turn left %1 ° speed %2 (1-10)", "args0": [ {"type":"input_value","name":"DEGREES","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },
  { "type": "sp_move_turn_right", "message0": "🤖 turn right %1 ° speed %2 (1-10)", "args0": [ {"type":"input_value","name":"DEGREES","check":"Number"}, {"type":"input_value","name":"SPEED","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },
  { "type": "sp_move_tank", "message0": "🤖 tank left %1 % right %2 %", "args0": [ {"type":"input_value","name":"LEFT","check":"Number"}, {"type":"input_value","name":"RIGHT","check":"Number"} ], "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },
  { "type": "sp_move_stop", "message0": "🤖 stop movement", "previousStatement":null,"nextStatement":null, "colour":"#D81B60" },

  // --- EREIGNIS-BLÖCKE (Hat-Blöcke) ---
  // 'hat: cap' bedeutet, dass dieser Block nur am Anfang eines Stacks stehen kann.
  { "type": "sp_event_program_start", "message0": "▶ when program starts", "nextStatement":null, "colour":"#F57F17", "hat":"cap" },

  // --- KONTROLLSTRUKTUREN ---
  // Diese Blöcke definieren Programmablauf-Logik (Wait, Schleifen, If/Else).
  // 'input_statement' ermöglicht es, andere Blöcke in diese Blöcke hineinzuziehen.
  { "type": "sp_ctrl_wait", "message0": "⏱ wait %1 seconds", "args0": [{"type":"input_value","name":"SECS","check":"Number"}], "previousStatement":null,"nextStatement":null, "colour":"#F9A825" },
  { "type": "sp_ctrl_repeat", "message0": "🔁 repeat %1 times", "args0": [{"type":"input_value","name":"TIMES","check":"Number"}], "message1": "%1", "args1": [{"type":"input_statement","name":"DO"}], "previousStatement":null,"nextStatement":null, "colour":"#F9A825" },
  { "type": "sp_ctrl_forever", "message0": "🔄 forever", "message1": "%1", "args1": [{"type":"input_statement","name":"DO"}], "previousStatement":null, "colour":"#F9A825" },
  { "type": "sp_ctrl_if", "message0": "❓ if %1 then", "args0": [{"type":"input_value","name":"COND","check":"Boolean"}], "message1": "%1", "args1": [{"type":"input_statement","name":"DO"}], "previousStatement":null,"nextStatement":null, "colour":"#F9A825" },
  { "type": "sp_ctrl_if_else", "message0": "❓ if %1 then", "args0": [{"type":"input_value","name":"COND","check":"Boolean"}], "message1": "%1", "args1": [{"type":"input_statement","name":"DO"}], "message2": "else", "message3": "%1", "args3": [{"type":"input_statement","name":"ELSE"}], "previousStatement":null,"nextStatement":null, "colour":"#F9A825" },
  { "type": "sp_ctrl_stop_all", "message0": "🛑 stop all", "previousStatement":null, "colour":"#F9A825" },

  // --- OPERATOREN ---
  // Blöcke mit 'output' geben einen Wert zurück und können daher in 'input_value' Felder eingesetzt werden.
  { "type": "sp_op_random", "message0": "🎲 random %1 to %2", "args0": [ {"type":"input_value","name":"FROM","check":"Number"}, {"type":"input_value","name":"TO","check":"Number"} ], "output":"Number", "colour":"#43A047" },
{ "type":"sp_touch_sensor", "message0":"👆 touch %1 %2", "args0":[ {"type":"input_value","name":"PIN","check":"Number"}, {"type":"field_dropdown","name":"STATE","options":[ ["pressed","PRESSED"], ["released","RELEASED"] ]} ], "output":"Boolean", "colour":"#29B6F6" },]);
