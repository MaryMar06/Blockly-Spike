// ============================================================
// TOOLBOX KONFIGURATION
// ============================================================
// Die Toolbox definiert die Struktur der Kategorien und die 
// verfügbaren Blöcke. 
// <shadow>-Blöcke bieten dem Benutzer Standardwerte an, die 
// automatisch in den Blöcken erscheinen, sobald sie in den 
// Workspace gezogen werden.

export const toolboxXml = `
<xml id="toolbox" style="display:none">
  
  <!-- MOTOREN: Steuerung der einzelnen Aktoren -->
  <category name="MOTORS" colour="#E64A19">
    <block type="sp_motor_run"><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_motor_run_rotations"><value name="ROTATIONS"><shadow type="math_number"><field name="NUM">1</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_motor_run_seconds"><value name="SECONDS"><shadow type="math_number"><field name="NUM">2</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_motor_stop"></block>
    <block type="sp_motor_set_speed"><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
  </category>

  <!-- BEWEGUNG: Komplexe Manöver für die Roboter-Basis -->
  <category name="MOVEMENT" colour="#D81B60">
    <block type="sp_move_forward"><value name="ROTATIONS"><shadow type="math_number"><field name="NUM">2</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_move_backward"><value name="ROTATIONS"><shadow type="math_number"><field name="NUM">2</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_move_turn_left"><value name="DEGREES"><shadow type="math_number"><field name="NUM">90</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_move_turn_right"><value name="DEGREES"><shadow type="math_number"><field name="NUM">90</field></shadow></value><value name="SPEED"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_move_tank"><value name="LEFT"><shadow type="math_number"><field name="NUM">5</field></shadow></value><value name="RIGHT"><shadow type="math_number"><field name="NUM">5</field></shadow></value></block>
    <block type="sp_move_stop"></block>
  </category>

  <!-- EVENTS: Start-Trigger für das Programm -->
  <category name="EVENTS" colour="#F57F17">
    <block type="sp_event_program_start"></block>
  </category>

  <!-- KONTROLLE: Programmablauf-Logik (Schleifen, Bedingungen) -->
  <category name="CONTROL" colour="#F9A825">
    <block type="sp_ctrl_wait"><value name="SECS"><shadow type="math_number"><field name="NUM">1</field></shadow></value></block>
    <block type="sp_ctrl_repeat"><value name="TIMES"><shadow type="math_number"><field name="NUM">10</field></shadow></value></block>
    <block type="sp_ctrl_forever"></block>
    <block type="sp_ctrl_if"></block>
    <block type="sp_ctrl_if_else"></block>
    <block type="sp_ctrl_stop_all"></block>
  </category>

  <!-- OPERATOREN: Mathematische und logische Berechnungen -->
  <category name="OPERATORS" colour="#43A047">
    <block type="sp_op_random"><value name="FROM"><shadow type="math_number"><field name="NUM">1</field></shadow></value><value name="TO"><shadow type="math_number"><field name="NUM">10</field></shadow></value></block>
    <block type="math_arithmetic"><field name="OP">ADD</field><value name="A"><shadow type="math_number"><field name="NUM">0</field></shadow></value><value name="B"><shadow type="math_number"><field name="NUM">0</field></shadow></value></block>
    <block type="math_arithmetic"><field name="OP">MINUS</field><value name="A"><shadow type="math_number"><field name="NUM">0</field></shadow></value><value name="B"><shadow type="math_number"><field name="NUM">0</field></shadow></value></block>
    <block type="logic_compare"><field name="OP">EQ</field></block>
    <block type="logic_compare"><field name="OP">LT</field></block>
    <block type="logic_compare"><field name="OP">GT</field></block>
    <block type="sp_touch_sensor"></block>
    </category>

  <!-- DYNAMISCHE KATEGORIEN: Diese werden von Blockly automatisch verwaltet -->
  <category name="VARIABLES" colour="#E53935" custom="VARIABLE"></category>
  <category name="MY BLOCKS" colour="#FF6D00" custom="PROCEDURE"></category>

</xml>
`;
