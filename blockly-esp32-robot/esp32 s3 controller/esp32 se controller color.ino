/*
 * ESP32S3 — Robot Controller (Motores + Telemetría Continua en Grados + Color)
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "Adafruit_AS7341.h"

// ── CONFIG AP ────────────────────────────────────────────────
const char*    AP_SSID         = "RobotAP";
const char*    AP_PASS         = "robot1234";
const uint16_t UDP_LISTEN_PORT = 4210;
const uint16_t UDP_TELEM_PORT  = 4211;

IPAddress wemosIP(0,0,0,0);
bool      wemosKnown = false;
WiFiUDP   udp;

// ── PINES MOTORES Y ENCODERS ─────────────────────────────────
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

void setPWM_A(int v) { ledcWrite(PWMA, constrain(v,0,255)); }
void setPWM_B(int v) { ledcWrite(PWMB, constrain(v,0,255)); }

const float PASOS_X_VUELTA = 2956.0f;
const float PASOS_X_GRADO  = (PASOS_X_VUELTA * 1.19f) / 180.0f;

// ── SENSOR DE COLOR AS7341 ───────────────────────────────────
Adafruit_AS7341 as7341;
bool sensorColorOk = false;

const float cieX[8] = {0.0776, 0.3481, 0.0956, 0.0291, 0.5121, 1.0263, 0.6424, 0.0468};
const float cieY[8] = {0.0022, 0.0298, 0.1390, 0.6082, 1.0000, 0.7570, 0.2650, 0.0170};
const float cieZ[8] = {0.3713, 1.7826, 0.8130, 0.1117, 0.0057, 0.0011, 0.0000, 0.0000};

volatile int sR = 0, sG = 0, sB = 0;
char sColorName[32] = "Desconocido";
volatile bool sColorNew = false;

String obtenerNombreColor(int r, int g, int b, float intensidadTotal) {
  if (intensidadTotal < 15.0) return "Oscuro / Negro";
  float rf = r / 255.0; float gf = g / 255.0; float bf = b / 255.0;
  float cmax = max(rf, max(gf, bf)); float cmin = min(rf, min(gf, bf));
  float delta = cmax - cmin;
  float s = (cmax == 0) ? 0 : (delta / cmax);
  if (s < 0.15) return "Blanco / Gris";

  float h = 0;
  if (delta > 0) {
    if (cmax == rf) { h = 60.0 * ((gf - bf) / delta); if (h < 0) h += 360.0; }
    else if (cmax == gf) h = 60.0 * (((bf - rf) / delta) + 2.0);
    else if (cmax == bf) h = 60.0 * (((rf - gf) / delta) + 4.0);
  }

  if (h >= 0   && h < 12)  return "Rojo";
  if (h >= 12  && h < 30)  return "Naranja"; 
  if (h >= 30  && h < 75)  return "Amarillo";
  if (h >= 75  && h < 160) return "Verde";
  if (h >= 160 && h < 200) return "Cian";
  if (h >= 200 && h < 260) return "Azul";
  if (h >= 260 && h < 320) return "Morado / Violeta";
  if (h >= 320 && h < 350) return "Rosa";
  if (h >= 350 && h <= 360) return "Rojo";
  return "Desconocido";
}

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
    vTaskDelay(pdMS_TO_TICKS(500)); 
  }
}

// ── ENCODERS ─────────────────────────────────────────────────
volatile long encA = 0, encB = 0;
void IRAM_ATTR isrA() { encA += (digitalRead(ENC_A_A)==digitalRead(ENC_A_B))?1:-1; }
void IRAM_ATTR isrB() { encB += (digitalRead(ENC_B_A)==digitalRead(ENC_B_B))?1:-1; }

// ── PROGRAMA ─────────────────────────────────────────────────
#define MAX_LINES   512
#define MAX_LINE_LEN 64
char     prog[MAX_LINES][MAX_LINE_LEN];
int      progLen  = 0;
bool     progDone = true;   

#define MAX_VARS 16
struct Var { char name[16]; float val; };
Var  vars[MAX_VARS];
int  varCount = 0;

float getVar(const char* name) {
  for (int i=0;i<varCount;i++) if (strcmp(vars[i].name,name)==0) return vars[i].val;
  return 0;
}
void setVar(const char* name, float val) {
  for (int i=0;i<varCount;i++) { if (strcmp(vars[i].name,name)==0){vars[i].val=val;return;} }
  if (varCount<MAX_VARS) { strncpy(vars[varCount].name,name,15); vars[varCount].val=val; varCount++; }
}

float evalExpr(const char* expr) {
  char e[48]; strncpy(e,expr,47); e[47]='\0';
  int s=0; while(e[s]==' ')s++;
  int len=strlen(e+s); while(len>0&&e[s+len-1]==' ')len--;
  e[s+len]='\0'; const char* ex=e+s;

  char* end; float v=strtof(ex,&end);
  if (end!=ex&&*end=='\0') return v;

  if (strncmp(ex,"random(",7)==0) {
    char inner[40]; strncpy(inner,ex+7,39);
    char* comma=strchr(inner,','); if(!comma)return 0; *comma='\0';
    float a=evalExpr(inner), b=evalExpr(comma+1);
    return a + (float)(esp_random()%(int)(fabsf(b-a)+1));
  }

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
  return getVar(ex);
}

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

void sendTelem(const char* type, const char* value) {
  if (!wemosKnown) return;
  char buf[128]; snprintf(buf,sizeof(buf),"%s:%s",type,value);
  udp.beginPacket(wemosIP,UDP_TELEM_PORT);
  udp.write((uint8_t*)buf,strlen(buf));
  udp.endPacket();
}

// ── MOTORES ──────────────────────────────────────────────────
volatile bool motAbort = false;

int  scalePWM(int v) { return map(constrain(v,1,10),1,10,55,255); }
void motorOff()      { setPWM_A(0); setPWM_B(0); digitalWrite(STBY,LOW); }
void motorOn()       { digitalWrite(STBY,HIGH); }
void dirA(bool f)    { digitalWrite(AIN1,f?LOW:HIGH); digitalWrite(AIN2,f?HIGH:LOW); }
void dirB(bool f)    { digitalWrite(BIN1,f?HIGH:LOW); digitalWrite(BIN2,f?LOW:HIGH); }

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

void turnSteps(long steps, bool cw, int vel) {
  int pwm=scalePWM(vel); encA=0; encB=0; motorOn(); dirA(!cw); dirB(cw);
  setPWM_A(pwm); setPWM_B(pwm);
  sendTelem("STATUS",cw?"TURN_RIGHT":"TURN_LEFT");
  while(!motAbort&&(abs(encA)<steps||abs(encB)<steps)) vTaskDelay(pdMS_TO_TICKS(5));
  motorOff();
}

void tankMove(float vA, float vB, float secs) {
  motorOn(); dirA(vA>=0); dirB(vB>=0);
  setPWM_A(scalePWM(constrain((int)fabsf(vA),1,10)));
  setPWM_B(scalePWM(constrain((int)fabsf(vB),1,10)));
  sendTelem("STATUS","TANK");
  unsigned long t0=millis();
  while(!motAbort&&(millis()-t0)<(unsigned long)(secs*1000.0f)) vTaskDelay(pdMS_TO_TICKS(5));
  motorOff();
}

void doWait(float secs) {
  unsigned long t0=millis();
  while(!motAbort&&(millis()-t0)<(unsigned long)(secs*1000.0f)) vTaskDelay(pdMS_TO_TICKS(5));
}

// ── INTÉRPRETE DE PROGRAMA ───────────────────────────────────
#define STACK_SIZE 32
struct StackFrame { char type; int loopStart; int loopCount; };
StackFrame stk[STACK_SIZE];
int stkTop = 0;

#define MAX_DEFS 8
struct Def { char name[32]; int start; int end; };
Def  defs[MAX_DEFS];
int  defCount = 0;

int findEnd(int from, const char* endToken, const char* startToken) {
  int depth=1;
  for(int i=from+1;i<progLen;i++){
    if(strncmp(prog[i],startToken,strlen(startToken))==0) depth++;
    else if(strcmp(prog[i],endToken)==0) { depth--; if(depth==0)return i; }
  }
  return progLen;
}

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

void execLine(int& pc);

void runProgram() {
  stkTop=0;
  prescanDefs();
  // <-- AQUÍ: Se resetean los valores automáticamente al arrancar el programa (como en Spike)
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

void execLine(int& pc) {
  const char* line = prog[pc];
  if(line[0]=='\0'){pc++;return;}

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

  if(strncmp(line,"REPEAT:",7)==0){
    int times=(int)evalExpr(line+7);
    if(times<=0){ pc=findEnd(pc,"END_REPEAT","REPEAT:")+1; return; }
    if(stkTop<STACK_SIZE){stk[stkTop++]={'R',pc,times-1};}
    pc++;return;
  }
  if(strcmp(line,"END_REPEAT")==0){
    if(stkTop>0&&stk[stkTop-1].type=='R'){
      if(stk[stkTop-1].loopCount>0){ stk[stkTop-1].loopCount--; pc=stk[stkTop-1].loopStart+1; return; } 
      else { stkTop--; }
    }
    pc++;return;
  }

  if(strcmp(line,"FOREVER")==0){
    if(stkTop<STACK_SIZE){stk[stkTop++]={'F',pc,0};}
    pc++;return;
  }
  if(strcmp(line,"END_FOREVER")==0){
    if(stkTop>0&&stk[stkTop-1].type=='F'){ pc=stk[stkTop-1].loopStart+1; return; }
    pc++;return;
  }

  if(strncmp(line,"IF:",3)==0){
    float cond=evalExpr(line+3);
    if(cond!=0){ if(stkTop<STACK_SIZE){stk[stkTop++]={'T',pc,0};} } 
    else       { if(stkTop<STACK_SIZE){stk[stkTop++]={'I',pc,0};} }
    pc++;return;
  }
  if(strcmp(line,"ELSE")==0){
    if(stkTop>0&&stk[stkTop-1].type=='T'){ stk[stkTop-1].type='X'; }
    pc++;return;
  }
  if(strcmp(line,"END_IF")==0){
    if(stkTop>0){ char t=stk[stkTop-1].type; if(t=='T'||t=='I'||t=='X') stkTop--; }
    pc++;return;
  }

  if(strncmp(line,"DEF:",4)==0){ pc=findEnd(pc,"END_DEF","DEF:")+1; return; }
  if(strcmp(line,"END_DEF")==0){ pc++;return; }

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

  // <-- AQUÍ: Reconocemos PING para no devolver error
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

// ── RECEPCIÓN UDP Y TAREAS ───────────────────────────────────
#define RX_TIMEOUT_MS 300
unsigned long lastRxMs = 0;
bool          receiving = false;

TaskHandle_t runTask = NULL;

void runTaskFn(void*) {
  for(;;){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    runProgram();
  }
}

void addLine(const char* line) {
  if(progLen>=MAX_LINES) return;
  strncpy(prog[progLen],line,MAX_LINE_LEN-1);
  prog[progLen][MAX_LINE_LEN-1]='\0';
  progLen++;
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); delay(200);

  Wire.begin(10, 11);
  if (as7341.begin()) {
    as7341.setATIME(100); as7341.setASTEP(999); as7341.setGain(AS7341_GAIN_128X);
    sensorColorOk = true;
    xTaskCreatePinnedToCore(colorTaskFn, "ColorTask", 4096, NULL, 1, NULL, 0);
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

// ── LOOP ─────────────────────────────────────────────────────
void loop() {
  // 1. Envío de Telemetría de Color
  if (sColorNew && wemosKnown) {
    sColorNew = false;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d,%d,%d,%s", sR, sG, sB, sColorName);
    sendTelem("COLOR", buf);
  }

  // 2. Envío de Telemetría de Encoders (Continua y en Grados)
  static unsigned long lastEncMs = 0;
  if (wemosKnown && (millis() - lastEncMs > 200)) {
    lastEncMs = millis();
    int degA = (int)(((float)encA / PASOS_X_VUELTA) * 360.0f);
    int degB = (int)(((float)encB / PASOS_X_VUELTA) * 360.0f);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d,%d", degA, degB);
    sendTelem("ENC", buf);
  }

  // 3. Recepción de Comandos (UDP)
  int pktSize = udp.parsePacket();
  if (pktSize > 0) {
    if (!wemosKnown){ wemosIP=udp.remoteIP(); wemosKnown=true; }
    char buf[MAX_LINE_LEN]; int len=udp.read(buf,MAX_LINE_LEN-1);
    if(len>0){
      buf[len]='\0'; String line=String(buf); line.trim();

      if(line=="STOP"){
        motAbort=true; motorOff();
        progLen=0; receiving=false; progDone=true;
        sendTelem("STATUS","IDLE");
        vTaskDelay(pdMS_TO_TICKS(5)); return;
      }

      if(!progDone) { vTaskDelay(pdMS_TO_TICKS(5)); return; }
      if(!receiving){ progLen=0; receiving=true; }
      addLine(line.c_str()); lastRxMs=millis();
    }
  }

  // 4. Iniciar ejecución
  if(receiving && progDone && (millis()-lastRxMs)>RX_TIMEOUT_MS){
    receiving=false;
    if(progLen>0){
      motAbort=false; progDone=false;
      xTaskNotifyGive(runTask);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(5));
}