/*
 * ESP32S3 - Robot controller (motors, continuous degree telemetry, and color)
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_AS7341.h"
#include "VL53L1X.h"

// AP CONFIGURATION
const char*    AP_SSID         = "RobotAP";
const char*    AP_PASS         = "robot1234";
const uint16_t UDP_LISTEN_PORT = 4210;
const uint16_t UDP_TELEM_PORT  = 4211;

IPAddress wemosIP(0,0,0,0);
bool      wemosKnown = false;
WiFiUDP   udp;

// MOTOR AND ENCODER PINS
#define AIN1 5
#define AIN2 6
#define PWMA 4
#define BIN1 12
#define BIN2 13
#define PWMB 14
#define STBY 7
#define ENC_A_A 18
#define ENC_A_B 19
#define ENC_B_A 20
#define ENC_B_B 21

#define PWM_FREQ 5000
#define PWM_RES  8

/*
 * Writes the PWM duty cycle for motor A.
 *
 * The caller can pass any integer, and constrain() keeps the value inside the
 * 8-bit PWM range used by this sketch. This protects the LEDC channel from
 * invalid values while keeping the motor-control functions simple.
 */
void setPWM_A(int v) { ledcWrite(PWMA, constrain(v,0,255)); }

/*
 * Writes the PWM duty cycle for motor B.
 *
 * This mirrors setPWM_A() for the second motor. Keeping the wrapper separate
 * makes later pin/channel changes easier because movement code does not need
 * to call ledcWrite() directly.
 */
void setPWM_B(int v) { ledcWrite(PWMB, constrain(v,0,255)); }

const float PASOS_X_VUELTA = 2956.0f;
const float PASOS_X_GRADO  = (PASOS_X_VUELTA * 1.19f) / 180.0f;

// AS7341 COLOR SENSOR
Adafruit_AS7341 as7341;
bool sensorColorOk = false;

VL53L1X distanceSensor;
bool sensorDistanzOk = false;
volatile int sDistanzMM = 0;

const float cieX[8] = {0.0776, 0.3481, 0.0956, 0.0291, 0.5121, 1.0263, 0.6424, 0.0468};
const float cieY[8] = {0.0022, 0.0298, 0.1390, 0.6082, 1.0000, 0.7570, 0.2650, 0.0170};
const float cieZ[8] = {0.3713, 1.7826, 0.8130, 0.1117, 0.0057, 0.0011, 0.0000, 0.0000};

volatile int sR = 0, sG = 0, sB = 0;
char sColorName[32] = "Unbekannt";
volatile bool sColorNew = false;

/*
 * Converts an RGB reading and total light intensity into a color name.
 *
 * The AS7341 task converts spectral readings into normalized RGB values first.
 * This function classifies those values by brightness, saturation, and hue:
 * very low intensity is treated as black, low saturation is treated as white,
 * and saturated colors are mapped into hue ranges. The returned names are the
 * existing UI/protocol strings and are intentionally left unchanged.
 */
String obtenerNombreColor(int r, int g, int b, float intensidadTotal) {
  if (intensidadTotal < 15.0) return "Schwarz";
  
  float rf = r / 255.0;
  float gf = g / 255.0;
  float bf = b / 255.0;

  float cmax = max(rf, max(gf, bf));
  float cmin = min(rf, min(gf, bf));
  float delta = cmax - cmin;

  float s = (cmax == 0) ? 0 : (delta / cmax);

  if (s < 0.15) return "Weiß";

  float h = 0;

  if (delta > 0) {
    if (cmax == rf) {
      h = 60.0 * ((gf - bf) / delta);
      if (h < 0) h += 360.0;
    }
    else if (cmax == gf) {
      h = 60.0 * (((bf - rf) / delta) + 2.0);
    }
    else if (cmax == bf) {
      h = 60.0 * (((rf - gf) / delta) + 4.0);
    }
  }

  if (h >= 0   && h < 12)  return "Rot";
  if (h >= 12  && h < 30)  return "Orange";
  if (h >= 30  && h < 75)  return "Gelb";
  if (h >= 75  && h < 160) return "Grün";
  if (h >= 160 && h < 200) return "Cyan";
  if (h >= 200 && h < 260) return "Blau";
  if (h >= 260 && h < 320) return "Violett";
  if (h >= 320 && h < 350) return "Rosa";
  if (h >= 350 && h <= 360) return "Rot";

  return "Unbekannt";
}

/*
 * FreeRTOS task that continuously reads the AS7341 color sensor.
 *
 * When the sensor is available, the task reads all channels, selects the eight
 * spectral channels used for color estimation, converts them through an
 * approximate CIE XYZ matrix, then converts XYZ to RGB. The result is normalized
 * and gamma-corrected before being published through the shared sR/sG/sB and
 * sColorName variables. sColorNew tells loop() that fresh telemetry should be
 * sent to the WEMOS bridge.
 *
 * The task sleeps for 500 ms between readings so color telemetry does not block
 * robot movement or command reception.
 */
void colorTaskFn(void* arg) {
  uint16_t ch[12];
  for (;;) {
    if (sensorColorOk) {
      if (as7341.readAllChannels(ch)) {
        float spec[8] = {(float)ch[0], (float)ch[1], (float)ch[2], (float)ch[3], 
                         (float)ch[6], (float)ch[7], (float)ch[8], (float)ch[9]};
        float intTotal = 0, X = 0, Y = 0, Z = 0;
        for(int i = 0; i < 8; i++){
          intTotal += spec[i];
          X += spec[i] * cieX[i]; Y += spec[i] * cieY[i]; Z += spec[i] * cieZ[i];
        }
        float R =  3.2406 * X - 1.5372 * Y - 0.4986 * Z;
        float G = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
        float B =  0.0557 * X - 0.2040 * Y + 1.0570 * Z;

        if (R < 0) R = 0; if (G < 0) G = 0; if (B < 0) B = 0;
        float maxRGB = max(R, max(G, B));
        if (maxRGB > 0) { R /= maxRGB; G /= maxRGB; B /= maxRGB; }

        R = pow(R, 1.0 / 2.2); G = pow(G, 1.0 / 2.2); B = pow(B, 1.0 / 2.2);
        int r = (int)(R * 255.0); int g = (int)(G * 255.0); int b = (int)(B * 255.0);

        sR = r; sG = g; sB = b;
        String name = obtenerNombreColor(r, g, b, intTotal);
        strncpy(sColorName, name.c_str(), 31); sColorName[31] = '\0';
        sColorNew = true; 
      }
    }
    if (sensorDistanzOk) {
      distanceSensor.read();
      if (!distanceSensor.timeoutOccurred()) {
        sDistanzMM = distanceSensor.ranging_data.range_mm;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

// ENCODERS
volatile long encA = 0, encB = 0;

/*
 * Interrupt handler for motor A's quadrature encoder.
 *
 * The handler compares the two encoder channels to decide whether the wheel
 * moved forward or backward, then increments or decrements encA. It is kept
 * very small because interrupt handlers must return quickly and avoid blocking
 * work.
 */
void IRAM_ATTR isrA() { encA += (digitalRead(ENC_A_A)==digitalRead(ENC_A_B))?1:-1; }

/*
 * Interrupt handler for motor B's quadrature encoder.
 *
 * This is the same direction-tracking logic as isrA(), but it updates encB for
 * the second wheel. Movement and telemetry functions read encA/encB to measure
 * distance, turns, and live encoder angles.
 */
void IRAM_ATTR isrB() { encB += (digitalRead(ENC_B_A)==digitalRead(ENC_B_B))?1:-1; }

// PROGRAM
#define MAX_LINES   512
#define MAX_LINE_LEN 64
char     prog[MAX_LINES][MAX_LINE_LEN];
int      progLen  = 0;
bool     progDone = true;   

#define MAX_VARS 16
struct Var { char name[16]; float val; };
Var  vars[MAX_VARS];
int  varCount = 0;

/*
 * Looks up the current value of a program variable.
 *
 * Variables are stored in a small fixed-size table because Blockly programs
 * sent to the robot are compact. If a variable has not been assigned yet, the
 * interpreter treats it as zero so expressions can still be evaluated safely.
 */
float getVar(const char* name) {
  for (int i=0;i<varCount;i++) if (strcmp(vars[i].name,name)==0) return vars[i].val;
  return 0;
}

/*
 * Creates or updates a program variable.
 *
 * Existing variables are overwritten in place. New variables are appended until
 * MAX_VARS is reached; extra variables are ignored to avoid writing outside the
 * fixed array.
 */
void setVar(const char* name, float val) {
  for (int i=0;i<varCount;i++) { if (strcmp(vars[i].name,name)==0){vars[i].val=val;return;} }
  if (varCount<MAX_VARS) { strncpy(vars[varCount].name,name,15); vars[varCount].val=val; varCount++; }
}

/*
 * Evaluates the compact expression syntax used by uploaded programs.
 *
 * Supported inputs are numeric literals, variable names, random(a,b), and
 * parenthesized binary expressions using +, -, *, /, =, <, or >. Comparisons
 * return 1 for true and 0 for false so they can drive IF and loop logic.
 *
 * This is intentionally a small interpreter rather than a full parser. Blockly
 * generates predictable expressions, so the function trims the input, handles
 * the known expression forms, and falls back to variable lookup.
 */
float evalExpr(const char* expr) {
  char e[48]; strncpy(e,expr,47); e[47]='\0';
  int s=0; while(e[s]==' ')s++;
  int len=strlen(e+s); while(len>0&&e[s+len-1]==' ')len--;
  e[s+len]='\0'; const char* ex=e+s;

  char* end; float v=strtof(ex,&end);
  if (end!=ex&&*end=='\0') return v;

  // random(a,b) returns a whole-number random value inside the requested range.
  if (strncmp(ex,"random(",7)==0) {
    char inner[40]; strncpy(inner,ex+7,39);
    char* comma=strchr(inner,','); if(!comma)return 0; *comma='\0';
    float a=evalExpr(inner), b=evalExpr(comma+1);
    return a + (float)(esp_random()%(int)(fabsf(b-a)+1));
  }

  // Binary expressions are expected inside parentheses, such as (a+b).
  if (ex[0]=='(') {
    char inner[40]; strncpy(inner,ex+1,39);
    int l=strlen(inner); if(l>0&&inner[l-1]==')')inner[l-1]='\0';
    int depth=0;
    for(int i=strlen(inner)-1;i>=0;i--){
      if(inner[i]==')')depth++; else if(inner[i]=='(')depth--;
      else if(depth==0&&(inner[i]=='+'||inner[i]=='-'||inner[i]=='*'||inner[i]=='/'||
              inner[i]=='='||inner[i]=='<'||inner[i]=='>')){
        char op=inner[i]; inner[i]='\0';
        float a=evalExpr(inner), b=evalExpr(inner+i+1);
        if(op=='+')return a+b; if(op=='-')return a-b; if(op=='*')return a*b;
        if(op=='/')return b!=0?a/b:0; if(op=='=')return (fabsf(a-b)<0.001f)?1:0;
        if(op=='<')return a<b?1:0; if(op=='>')return a>b?1:0;
      }
    }
  }
  if (strncmp(ex, "COLOR_IS_", 9) == 0) {
    char wanted[16];
    strncpy(wanted, ex + 9, 15); wanted[15] = '\0';
    char* paren = strchr(wanted, '(');
    if (paren) *paren = '\0';
    const char* colorMap[][2] = {
      {"RED","Rot"},{"GREEN","Grün"},{"BLUE","Blau"},
      {"YELLOW","Gelb"},{"WHITE","Weiß"},{"BLACK","Schwarz"},
      {"ORANGE","Orange"},{"CYAN","Cyan"},{"VIOLET","Violett"},{"PINK","Rosa"}
    };
    for (auto& m : colorMap) {
      if (strcmp(wanted, m[0]) == 0)
        return (strcmp(sColorName, m[1]) == 0) ? 1.0f : 0.0f;
    }
    return 0.0f;
  }

  // NEU: RGB-Kanäle einzeln abfragen (0-255)
  if (strcmp(ex, "COLOR_RED()")   == 0) return (float)sR;
  if (strcmp(ex, "COLOR_GREEN()") == 0) return (float)sG;
  if (strcmp(ex, "COLOR_BLUE()")  == 0) return (float)sB;

  // NEU: Abstand in mm vom VL53L1X
  if (strcmp(ex, "DISTANCE()") == 0) return (float)sDistanzMM;
  return getVar(ex);
}

/*
 * Extracts one colon-separated parameter from a command line.
 *
 * Robot commands use a compact format such as FORWARD:1:5. This helper returns
 * the requested field by index. Parentheses and square brackets increase depth
 * so colons inside expressions are not treated as separators.
 */
String getP(const char* s, int idx) {
  int f=0, st=0, depth=0;
  for (int i=0;;i++) {
    if (s[i]=='('||s[i]=='[') depth++;
    else if(s[i]==')'||s[i]==']') depth--;
    else if((s[i]==':'&&depth==0)||s[i]=='\0') {
      if(f==idx) return String(s).substring(st,i);
      f++; st=i+1;
    }
    if(s[i]=='\0')break;
  }
  return "";
}

/*
 * Sends one telemetry packet back to the WEMOS bridge.
 *
 * Telemetry is skipped until the WEMOS board has sent at least one packet,
 * because that first packet gives us its IP address. Messages are formatted as
 * TYPE:VALUE and sent over UDP to the telemetry port expected by the bridge.
 */
void sendTelem(const char* type, const char* value) {
  if (!wemosKnown) return;
  char buf[128]; snprintf(buf,sizeof(buf),"%s:%s",type,value);
  udp.beginPacket(wemosIP,UDP_TELEM_PORT);
  udp.write((uint8_t*)buf,strlen(buf));
  udp.endPacket();
}

// MOTORS
volatile bool motAbort = false;

/*
 * Converts a Blockly speed value into an 8-bit PWM duty cycle.
 *
 * Blockly commands use a simple 1-10 speed scale. This maps that scale to a
 * practical PWM range where 1 is still strong enough to move the robot and 10
 * reaches full duty cycle.
 */
int  scalePWM(int v) { return map(constrain(v,1,10),1,10,55,255); }

/*
 * Stops both motors and disables the motor driver standby pin.
 *
 * This is the common safe stop used after completed movements, STOP commands,
 * and aborted programs. Pulling STBY low disables the driver outputs after the
 * PWM channels are set to zero.
 */
void motorOff()      { setPWM_A(0); setPWM_B(0); digitalWrite(STBY,LOW); }

/*
 * Enables the motor driver.
 *
 * The driver must be taken out of standby before direction pins and PWM output
 * can move the motors. Movement functions call this before starting motion.
 */
void motorOn()       { digitalWrite(STBY,HIGH); }

/*
 * Sets the direction pins for motor A.
 *
 * The wiring for motor A uses LOW/HIGH for forward and HIGH/LOW for reverse.
 * Keeping this in one helper prevents the movement code from duplicating the
 * pin polarity details.
 */
void dirA(bool f)    { digitalWrite(AIN1,f?LOW:HIGH); digitalWrite(AIN2,f?HIGH:LOW); }

/*
 * Sets the direction pins for motor B.
 *
 * Motor B is wired with the opposite forward polarity from motor A, so this
 * helper hides that difference. Passing true means "drive forward" at the robot
 * level even though the pin pattern is different.
 */
void dirB(bool f)    { digitalWrite(BIN1,f?HIGH:LOW); digitalWrite(BIN2,f?LOW:HIGH); }

/*
 * Drives both wheels for a target encoder step count.
 *
 * The function resets both encoders, enables the motors, sets a shared forward
 * or backward direction, and drives until both wheels have reached the target.
 * While moving, it compares encoder distances and slightly adjusts each PWM
 * duty cycle so the wheels stay synchronized.
 *
 * motAbort is checked continuously. A STOP command can set that flag from the
 * main loop, causing this function to leave the loop and turn the motors off.
 */
void moveSteps(long steps, bool fwd, int vel) {
  int pwm=scalePWM(vel); encA=0; encB=0; motorOn(); dirA(fwd); dirB(fwd);
  setPWM_A(pwm); setPWM_B(pwm);
  sendTelem("STATUS",fwd?"FORWARD":"BACKWARD");
  while(!motAbort){
    long dA=abs(encA),dB=abs(encB); if(dA>=steps&&dB>=steps)break;
    long diff=dA-dB;
    setPWM_A(dA<steps?constrain(pwm-(int)(diff*0.3f),40,255):0);
    setPWM_B(dB<steps?constrain(pwm+(int)(diff*0.3f),40,255):0);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  motorOff();
}

/*
 * Rotates the robot in place for a target encoder step count.
 *
 * Clockwise turns drive the two motors in opposite directions. The routine
 * waits until both encoders have reached the requested absolute step count, or
 * until motAbort is set. It is used by TURN_RIGHT and TURN_LEFT commands after
 * degrees have been converted into encoder steps.
 */
void turnSteps(long steps, bool cw, int vel) {
  int pwm=scalePWM(vel); encA=0; encB=0; motorOn(); dirA(!cw); dirB(cw);
  setPWM_A(pwm); setPWM_B(pwm);
  sendTelem("STATUS",cw?"TURN_RIGHT":"TURN_LEFT");
  while(!motAbort&&(abs(encA)<steps||abs(encB)<steps)) vTaskDelay(pdMS_TO_TICKS(5));
  motorOff();
}

/*
 * Drives the two motors independently for a fixed amount of time.
 *
 * vA and vB are signed speed values: the sign chooses direction and the
 * absolute value chooses the 1-10 speed that is mapped to PWM. This is used for
 * tank-style movement where each wheel can run at a different speed.
 */
void tankMove(float vA, float vB, float secs) {
  motorOn(); dirA(vA>=0); dirB(vB>=0);
  setPWM_A(scalePWM(constrain((int)fabsf(vA),1,10)));
  setPWM_B(scalePWM(constrain((int)fabsf(vB),1,10)));
  sendTelem("STATUS","TANK");
  unsigned long t0=millis();
  while(!motAbort&&(millis()-t0)<(unsigned long)(secs*1000.0f)) vTaskDelay(pdMS_TO_TICKS(5));
  motorOff();
}

/*
 * Waits without blocking FreeRTOS scheduling.
 *
 * The interpreter uses this for WAIT commands. It repeatedly yields with
 * vTaskDelay() so other tasks, including UDP reception and the color task, can
 * continue running while the program is paused.
 */
void doWait(float secs) {
  unsigned long t0=millis();
  while(!motAbort&&(millis()-t0)<(unsigned long)(secs*1000.0f)) vTaskDelay(pdMS_TO_TICKS(5));
}

// PROGRAM INTERPRETER
#define STACK_SIZE 32
struct StackFrame { char type; int loopStart; int loopCount; };
StackFrame stk[STACK_SIZE];
int stkTop = 0;

#define MAX_DEFS 8
struct Def { char name[32]; int start; int end; };
Def  defs[MAX_DEFS];
int  defCount = 0;

/*
 * Finds the matching end token for a nested block.
 *
 * Program blocks such as IF, REPEAT, FOREVER, and DEF can contain other blocks
 * of the same type. This function walks forward from the current line, tracks
 * nesting depth, and returns the line index where the matching END_* token is
 * found. If no match is found, it returns progLen so callers skip to the end
 * safely.
 */
int findEnd(int from, const char* endToken, const char* startToken) {
  int depth=1;
  for(int i=from+1;i<progLen;i++){
    if(strncmp(prog[i],startToken,strlen(startToken))==0) depth++;
    else if(strcmp(prog[i],endToken)==0) { depth--; if(depth==0)return i; }
  }
  return progLen;
}

/*
 * Scans the uploaded program for function definitions.
 *
 * DEF blocks are not executed when first encountered by the main program flow.
 * This prescan records each definition name and its start/end line positions so
 * CALL commands can execute the definition body later.
 */
void prescanDefs() {
  defCount=0;
  for(int i=0;i<progLen;i++){
    if(strncmp(prog[i],"DEF:",4)==0){
      if(defCount>=MAX_DEFS)continue;
      strncpy(defs[defCount].name, prog[i]+4, 31);
      defs[defCount].start=i; defs[defCount].end=findEnd(i,"END_DEF","DEF:");
      defCount++;
    }
  }
}

/*
 * Forward declaration for the interpreter.
 *
 * CALL execution recursively runs lines inside DEF blocks, and runProgram()
 * also calls execLine(). The declaration lets those functions reference
 * execLine() before its full implementation appears below.
 */
void execLine(int& pc);

/*
 * Runs the currently buffered Blockly program.
 *
 * The UDP loop fills prog[] one command line at a time. When the upload timeout
 * expires, runTaskFn() calls this function on its own FreeRTOS task. It clears
 * the interpreter stack, records function definitions, resets encoders to match
 * Spike-like startup behavior, then executes lines until the program ends or a
 * STOP command sets motAbort.
 */
void runProgram() {
  stkTop=0;
  prescanDefs();
  // Reset encoder values automatically when the program starts, like Spike.
  encA = 0; encB = 0;
  sendTelem("STATUS","RUNNING");

  int pc=0;
  while(pc<progLen && !motAbort){
    execLine(pc);
  }
  if(!motAbort) sendTelem("STATUS","DONE");
  else          sendTelem("STATUS","IDLE");
  progDone=true;
}

/*
 * Executes one line of the compact robot program.
 *
 * pc is passed by reference so the interpreter can advance to the next line,
 * jump over inactive blocks, repeat loops, or execute function bodies. The
 * function handles Blockly control structures first, then assignments and
 * user-defined calls, and finally dispatches movement/wait/reset commands to
 * the motor helpers.
 *
 * The stack stores active control blocks:
 * R = repeat loop, F = forever loop, T = active IF branch, I = inactive IF
 * branch, and X = skipped ELSE branch. The skip logic also recognizes D if a
 * definition body ever needs to be represented on the stack.
 */
void execLine(int& pc) {
  const char* line = prog[pc];
  if(line[0]=='\0'){pc++;return;}

  // If the current stack top represents a skipped branch, jump over its body.
  if(stkTop>0){
    char top=stk[stkTop-1].type;
    if(top=='I'||top=='D'){
      if(top=='I'&&strcmp(line,"ELSE")==0){ stk[stkTop-1].type='X'; pc++;return; }
      if(strcmp(line,"END_IF")==0||strcmp(line,"END_REPEAT")==0||
         strcmp(line,"END_FOREVER")==0||(top=='D'&&strcmp(line,"END_DEF")==0)){
        stkTop--;pc++;return;
      }
      if(strncmp(line,"IF:",3)==0)     { pc=findEnd(pc,"END_IF","IF:")+1; return; }
      if(strncmp(line,"REPEAT:",7)==0) { pc=findEnd(pc,"END_REPEAT","REPEAT:")+1; return; }
      if(strcmp(line,"FOREVER")==0)    { pc=findEnd(pc,"END_FOREVER","FOREVER")+1; return; }
      if(strncmp(line,"DEF:",4)==0)    { pc=findEnd(pc,"END_DEF","DEF:")+1; return; }
      pc++;return;
    }
    if(top=='X'){
      if(strcmp(line,"END_IF")==0){stkTop--;pc++;return;}
      if(strncmp(line,"IF:",3)==0) { pc=findEnd(pc,"END_IF","IF:")+1; return; }
    }
  }

  // REPEAT starts a counted loop and stores the remaining iteration count.
  if(strncmp(line,"REPEAT:",7)==0){
    int times=(int)evalExpr(line+7);
    if(times<=0){ pc=findEnd(pc,"END_REPEAT","REPEAT:")+1; return; }
    if(stkTop<STACK_SIZE){stk[stkTop++]={'R',pc,times-1};}
    pc++;return;
  }

  // END_REPEAT either loops back to the first body line or closes the loop.
  if(strcmp(line,"END_REPEAT")==0){
    if(stkTop>0&&stk[stkTop-1].type=='R'){
      if(stk[stkTop-1].loopCount>0){ stk[stkTop-1].loopCount--; pc=stk[stkTop-1].loopStart+1; return; } 
      else { stkTop--; }
    }
    pc++;return;
  }

  // FOREVER loops until motAbort is set by STOP or by another abort path.
  if(strcmp(line,"FOREVER")==0){
    if(stkTop<STACK_SIZE){stk[stkTop++]={'F',pc,0};}
    pc++;return;
  }

  // END_FOREVER jumps back to the line after FOREVER.
  if(strcmp(line,"END_FOREVER")==0){
    if(stkTop>0&&stk[stkTop-1].type=='F'){ pc=stk[stkTop-1].loopStart+1; return; }
    pc++;return;
  }

  // IF stores whether the true branch should run or be skipped.
  if(strncmp(line,"IF:",3)==0){
    float cond=evalExpr(line+3);
    if(cond!=0){ if(stkTop<STACK_SIZE){stk[stkTop++]={'T',pc,0};} } 
    else       { if(stkTop<STACK_SIZE){stk[stkTop++]={'I',pc,0};} }
    pc++;return;
  }

  // ELSE switches an already-executed IF branch into skip mode.
  if(strcmp(line,"ELSE")==0){
    if(stkTop>0&&stk[stkTop-1].type=='T'){ stk[stkTop-1].type='X'; }
    pc++;return;
  }

  // END_IF closes any IF/ELSE state that is still on the stack.
  if(strcmp(line,"END_IF")==0){
    if(stkTop>0){ char t=stk[stkTop-1].type; if(t=='T'||t=='I'||t=='X') stkTop--; }
    pc++;return;
  }

  // DEF bodies are stored by prescanDefs() and skipped during normal flow.
  if(strncmp(line,"DEF:",4)==0){ pc=findEnd(pc,"END_DEF","DEF:")+1; return; }
  if(strcmp(line,"END_DEF")==0){ pc++;return; }

  // CALL executes the stored definition body line by line.
  if(strncmp(line,"CALL:",5)==0){
    const char* name=line+5;
    for(int i=0;i<defCount;i++){
      if(strcmp(defs[i].name,name)==0){
        int sub=defs[i].start+1;
        while(sub<defs[i].end&&!motAbort) execLine(sub);
        break;
      }
    }
    pc++;return;
  }

  // SET stores the evaluated expression result in the variable table.
  if(strncmp(line,"SET:",4)==0){
    String l=String(line); int c1=l.indexOf(':',4);
    if(c1>0){
      String vname=l.substring(4,c1); String expr=l.substring(c1+1);
      setVar(vname.c_str(),(float)evalExpr(expr.c_str()));
    }
    pc++;return;
  }

  sendTelem("CMD",line);
  String t=getP(line,0); t.toUpperCase();

  // Recognize PING so it does not return an error.
  if      (t=="PING")       { sendTelem("STATUS", "IDLE"); }
  else if (t=="STOP")       { motorOff(); }
  else if (t=="FORWARD")    { float r=evalExpr(getP(line,1).c_str()); int sp=(int)evalExpr(getP(line,2).c_str()); moveSteps((long)(r*PASOS_X_VUELTA),true,sp); }
  else if (t=="BACKWARD")   { float r=evalExpr(getP(line,1).c_str()); int sp=(int)evalExpr(getP(line,2).c_str()); moveSteps((long)(r*PASOS_X_VUELTA),false,sp); }
  else if (t=="TURN_RIGHT") { float g=evalExpr(getP(line,1).c_str()); int sp=(int)evalExpr(getP(line,2).c_str()); turnSteps((long)(g*PASOS_X_GRADO),true,sp); }
  else if (t=="TURN_LEFT")  { float g=evalExpr(getP(line,1).c_str()); int sp=(int)evalExpr(getP(line,2).c_str()); turnSteps((long)(g*PASOS_X_GRADO),false,sp); }
  else if (t=="MOTOR_A")    { float r=evalExpr(getP(line,1).c_str()); bool fwd=(getP(line,2)!="0"); int sp=(int)evalExpr(getP(line,3).c_str()); moveSteps((long)(r*PASOS_X_VUELTA),fwd,sp); }
  else if (t=="MOTOR_B")    { float r=evalExpr(getP(line,1).c_str()); bool fwd=(getP(line,2)!="0"); int sp=(int)evalExpr(getP(line,3).c_str()); moveSteps((long)(r*PASOS_X_VUELTA),fwd,sp); }
  else if (t=="TANK")       { float vA=evalExpr(getP(line,1).c_str()); float vB=evalExpr(getP(line,2).c_str()); float s=evalExpr(getP(line,3).c_str()); tankMove(vA,vB,s); }
  else if (t=="WAIT")       { float s=evalExpr(getP(line,1).c_str()); doWait(s); }
  else if (t=="RESET_ENC")  { encA=0; encB=0; sendTelem("STATUS","ENC_RESET"); }
  else                      { sendTelem("WARN",line); }

  pc++;
}

// UDP RECEPTION AND TASKS
#define RX_TIMEOUT_MS 300
unsigned long lastRxMs = 0;
bool          receiving = false;

TaskHandle_t runTask = NULL;

/*
 * FreeRTOS worker task that runs uploaded programs.
 *
 * The main loop receives UDP lines and decides when a complete program has
 * arrived. It then notifies this task. Running the interpreter here keeps
 * movement commands, waits, and loops from blocking the UDP/color work that
 * continues in loop() and colorTaskFn().
 */
void runTaskFn(void*) {
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    runProgram();
  }
}

/*
 * Appends one received program line to the program buffer.
 *
 * Lines are truncated to MAX_LINE_LEN - 1 and always null-terminated. If the
 * program buffer is full, the new line is ignored so the sketch never writes
 * past the fixed-size prog array.
 */
void addLine(const char* line) {
  if(progLen>=MAX_LINES) return;
  strncpy(prog[progLen],line,MAX_LINE_LEN-1);
  prog[progLen][MAX_LINE_LEN-1]='\0';
  progLen++;
}

// SETUP
/*
 * Initializes sensors, motors, encoders, WiFi, UDP, and background tasks.
 *
 * setup() prepares the hardware in a safe order: Serial starts first for debug
 * output, the color sensor is initialized and its task is created only if the
 * sensor responds, motor pins are configured with the driver disabled, encoder
 * interrupts are attached, the ESP32 access point is started, UDP listening is
 * enabled, and finally the program-runner task is created.
 */
void setup() {
  Serial.begin(115200); delay(200);

  Wire.begin(15, 5);
  if (as7341.begin()) {
    as7341.setATIME(100); as7341.setASTEP(999); as7341.setGain(AS7341_GAIN_128X);
    sensorColorOk = true;
    xTaskCreatePinnedToCore(colorTaskFn, "ColorTask", 8192, NULL, 1, NULL, 0);
  }

  distanceSensor.setTimeout(500);
  if (distanceSensor.init()) {
    distanceSensor.setDistanceMode(VL53L1X::Long);
    distanceSensor.setMeasurementTimingBudget(50000);
    distanceSensor.startContinuous(50);
    sensorDistanzOk = true;
  }

  pinMode(STBY,OUTPUT); digitalWrite(STBY,LOW);
  pinMode(AIN1,OUTPUT); pinMode(AIN2,OUTPUT);
  pinMode(BIN1,OUTPUT); pinMode(BIN2,OUTPUT);
  digitalWrite(AIN1,LOW); digitalWrite(AIN2,LOW);
  digitalWrite(BIN1,LOW); digitalWrite(BIN2,LOW);

  ledcAttach(PWMA,PWM_FREQ,PWM_RES); ledcAttach(PWMB,PWM_FREQ,PWM_RES);
  ledcWrite(PWMA,0); ledcWrite(PWMB,0);

  pinMode(ENC_A_A,INPUT_PULLUP); pinMode(ENC_A_B,INPUT_PULLUP);
  pinMode(ENC_B_A,INPUT_PULLUP); pinMode(ENC_B_B,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A_A),isrA,CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B_A),isrB,CHANGE);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID,AP_PASS);
  udp.begin(UDP_LISTEN_PORT);

  xTaskCreatePinnedToCore(runTaskFn,"RunTask",8192,NULL,2,&runTask,1);
}

// LOOP
/*
 * Handles real-time communication and telemetry for the robot controller.
 *
 * The interpreter runs in runTaskFn(), so loop() focuses on short recurring
 * jobs: send fresh color telemetry, send encoder telemetry every 200 ms,
 * receive UDP command/program lines from the WEMOS bridge, handle STOP
 * immediately, and start program execution after a short receive timeout.
 *
 * The receive timeout lets the desktop app send a multi-line program as a burst
 * of UDP packets. When no new line arrives for RX_TIMEOUT_MS, the buffered lines
 * are treated as one complete program.
 */
void loop() {
  // 1. Send color telemetry.
  if (sColorNew && wemosKnown) {
    sColorNew = false;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d,%d,%d,%s", sR, sG, sB, sColorName);
    sendTelem("COLOR", buf);
  }
 // NEU: Abstands-Telemetrie alle 200ms senden
  static unsigned long lastDistMs = 0;
  if (sensorDistanzOk && wemosKnown && (millis() - lastDistMs > 200)) {
    lastDistMs = millis();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", sDistanzMM);
    sendTelem("DIST", buf);
  }
  // 2. Send continuous encoder telemetry in degrees.
  static unsigned long lastEncMs = 0;
  if (wemosKnown && (millis() - lastEncMs > 200)) {
    lastEncMs = millis();
    int degA = (int)(((float)encA / PASOS_X_VUELTA) * 360.0f);
    int degB = (int)(((float)encB / PASOS_X_VUELTA) * 360.0f);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d,%d", degA, degB);
    sendTelem("ENC", buf);
  }

  // 3. Receive UDP commands.
  int pktSize = udp.parsePacket();
  if (pktSize > 0) {
    // Learn the WEMOS IP from the first incoming packet so replies know where to go.
    if (!wemosKnown){ wemosIP=udp.remoteIP(); wemosKnown=true; }
    char buf[MAX_LINE_LEN]; int len=udp.read(buf,MAX_LINE_LEN-1);
    if(len>0){
      buf[len]='\0'; String line=String(buf); line.trim();

      if(line=="STOP"){
        // STOP must interrupt immediately, even while a previous program is running.
        motAbort=true; motorOff();
        progLen=0; receiving=false; progDone=true;
        sendTelem("STATUS","IDLE");
        vTaskDelay(pdMS_TO_TICKS(5)); return;
      }

      if(!progDone) { vTaskDelay(pdMS_TO_TICKS(5)); return; }
      // The first line after an idle state starts a new buffered program.
      if(!receiving){ progLen=0; receiving=true; }
      addLine(line.c_str()); lastRxMs=millis();
    }
  }

  // 4. Start execution.
  if(receiving && progDone && (millis()-lastRxMs)>RX_TIMEOUT_MS){
    receiving=false;
    if(progLen>0){
      // Notify the worker task instead of executing the program in loop().
      motAbort=false; progDone=false;
      xTaskNotifyGive(runTask);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(5));
}
