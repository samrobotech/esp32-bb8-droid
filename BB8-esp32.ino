#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_wifi.h>

//AP setting_____________________________________________________________________________

const char* AP_SSID = "BB-8";
const char* AP_PASS = "123456789";
IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GW(192, 168, 4, 1);
IPAddress AP_SN(255, 255, 255, 0);

//TB6612FNG pin setting__________________________________________________________________

#define AIN1  4
#define AIN2  5
#define PWMA  6
#define BIN1  7
#define BIN2  15
#define PWMB  16
#define STBY  17

// PWM Setting ─────────────────────────────────────────────

#define PWM_FREQ    20000
#define PWM_RES     8
#define CH_A        0
#define CH_B        1
#define SPD         200

// webserver setting_________________________________________________________________________

WebServer        httpServer(80);
WebSocketsServer wsServer(81);   // WebSocket on 81 port

// ──────────────────────────────────────────────────────────
// MOTORS:
// ──────────────────────────────────────────────────────────
void motorInit() {
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PWMA, CH_A);
  ledcAttachPin(PWMB, CH_B);
  digitalWrite(STBY, HIGH);
  stopMotors();
}

void motorA(int s) {
  if      (s > 0) { digitalWrite(AIN1,HIGH); digitalWrite(AIN2,LOW);  ledcWrite(CH_A, s);  }
  else if (s < 0) { digitalWrite(AIN1,LOW);  digitalWrite(AIN2,HIGH); ledcWrite(CH_A,-s);  }
  else            { digitalWrite(AIN1,LOW);  digitalWrite(AIN2,LOW);  ledcWrite(CH_A, 0);  }
}

void motorB(int s) {
  if      (s > 0) { digitalWrite(BIN1,HIGH); digitalWrite(BIN2,LOW);  ledcWrite(CH_B, s);  }
  else if (s < 0) { digitalWrite(BIN1,LOW);  digitalWrite(BIN2,HIGH); ledcWrite(CH_B,-s);  }
  else            { digitalWrite(BIN1,LOW);  digitalWrite(BIN2,LOW);  ledcWrite(CH_B, 0);  }
}

void stopMotors()  { motorA(0);    motorB(0);    }
void goForward()   { motorA(SPD);  motorB(SPD);  }
void goBackward()  { motorA(-SPD); motorB(-SPD); }
void goRight()     { motorA(SPD);  motorB(-SPD); }
void goLeft()      { motorA(-SPD); motorB(SPD);  }

void execCmd(const String& cmd) {
  if      (cmd == "F") goForward();
  else if (cmd == "B") goBackward();
  else if (cmd == "L") goLeft();
  else if (cmd == "R") goRight();
  else                 stopMotors();   
}

// ──────────────────────────────────────────────────────────
// WebSocket handler
// ──────────────────────────────────────────────────────────
void onWebSocketEvent(uint8_t clientId,
                      WStype_t type,
                      uint8_t* payload,
                      size_t length) {
  switch (type) {

    case WStype_CONNECTED:
      Serial.printf("[WS] Client #%u connected\n", clientId);
      wsServer.sendTXT(clientId, "READY");
      break;

    case WStype_TEXT: {
      String cmd = "";
      for (size_t i = 0; i < length; i++) cmd += (char)payload[i];
      cmd.trim();
      Serial.printf("[WS] CMD: %s\n", cmd.c_str());
      execCmd(cmd);
      break;
    }

    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", clientId);
      stopMotors();   
      break;

    default: break;
  }
}

// ──────────────────────────────────────────────────────────
// HTML
// ──────────────────────────────────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="fa">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>SRT</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0;
    touch-action:manipulation;-webkit-tap-highlight-color:transparent;}
  body{
    background:#0d0d0d;
    display:flex;flex-direction:column;
    align-items:center;justify-content:center;
    height:100vh;overflow:hidden;
    font-family:'Arial Black',Arial,sans-serif;
    user-select:none;
  }
  h1{
    color:#fff;font-size:4rem;font-weight:900;letter-spacing:0.4em;
    margin-bottom:6px;
    text-shadow:0 0 25px #e63946,0 0 55px #e63946aa;
  }
  .sub{color:#3a3a3a;font-size:0.68rem;letter-spacing:0.25em;margin-bottom:50px;}
  .pad{
    display:grid;
    grid-template-columns:repeat(3,105px);
    grid-template-rows:repeat(3,105px);
    gap:12px;
  }
  .btn{
    background:#161616;border:2px solid #252525;border-radius:22px;
    color:#ccc;font-size:2.3rem;
    display:flex;align-items:center;justify-content:center;
    cursor:pointer;transition:background .07s,transform .07s,box-shadow .07s;
  }
  .btn.on{
    background:#e63946;border-color:#e63946;color:#fff;
    transform:scale(0.90);box-shadow:0 0 28px #e6394677;
  }
  .stop{font-size:0.72rem;letter-spacing:0.15em;color:#3a3a3a;}
  .stop.on{background:#2a2a2a;border-color:#444;color:#fff;transform:scale(0.90);}

  /* نوار وضعیت */
  .bar{
    margin-top:45px;display:flex;align-items:center;gap:9px;
    font-size:0.68rem;letter-spacing:0.2em;color:#2a2a2a;
  }
  .led{width:8px;height:8px;border-radius:50%;background:#e63946;
       animation:blink 1.2s infinite;}
  .led.ok{background:#2ecc71;animation:none;}
  @keyframes blink{0%,100%{opacity:1;}50%{opacity:0.15;}}
</style>
</head>
<body>

<h1>SRT</h1>
<p class="sub">REAL-TIME · BB-8 CONTROLLER</p>

<div class="pad">
  <div></div>
  <div class="btn" id="F">▲</div>
  <div></div>

  <div class="btn" id="L">◀</div>
  <div class="btn stop" id="S">STOP</div>
  <div class="btn" id="R">▶</div>

  <div></div>
  <div class="btn" id="B">▼</div>
  <div></div>
</div>

<div class="bar">
  <div class="led" id="led"></div>
  <span id="stTxt">CONNECTING...</span>
</div>

<script>
// ── WebSocket ──────────────────────────────────────────────
const WS_URL = 'ws://' + location.hostname + ':81/';
let ws, reconnTimer;
let activeBtn = null;

function connect() {
  ws = new WebSocket(WS_URL);

  ws.onopen = () => {
    document.getElementById('led').className  = 'led ok';
    document.getElementById('stTxt').textContent = 'CONNECTED';
    clearTimeout(reconnTimer);
  };

  ws.onclose = () => {
    document.getElementById('led').className  = 'led';
    document.getElementById('stTxt').textContent = 'RECONNECTING...';
   // if stop reconnect after 800 ms
    reconnTimer = setTimeout(connect, 800);
  };

  ws.onerror = () => ws.close();
}

function send(cmd) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(cmd);
}

//control button __________________________________________________________________________________________
const cmdMap = {F:'F', B:'B', L:'L', R:'R', S:'S'};

function press(id) {
  if (activeBtn === id) return;
  if (activeBtn) release(activeBtn);
  activeBtn = id;
  document.getElementById(id).classList.add('on');
  send(cmdMap[id]);
}

function release(id) {
  if (!activeBtn) return;
  document.getElementById(activeBtn).classList.remove('on');
  activeBtn = null;
  send('S');
}

// Mouse events
['F','B','L','R','S'].forEach(id => {
  const el = document.getElementById(id);
  el.addEventListener('mousedown',  () => press(id));
  el.addEventListener('mouseup',    () => release(id));
  el.addEventListener('mouseleave', () => { if(activeBtn===id) release(id); });
});

// Touch events
['F','B','L','R','S'].forEach(id => {
  const el = document.getElementById(id);
  el.addEventListener('touchstart', e => { e.preventDefault(); press(id); },   {passive:false});
  el.addEventListener('touchend',   e => { e.preventDefault(); release(id); }, {passive:false});
  el.addEventListener('touchcancel',e => { e.preventDefault(); release(id); }, {passive:false});
});

// جلوگیری از scroll صفحه
document.addEventListener('touchmove',   e => e.preventDefault(), {passive:false});
document.addEventListener('contextmenu', e => e.preventDefault());
document.addEventListener('mouseup',     () => { if(activeBtn) release(activeBtn); });

// شروع اتصال
connect();
</script>
</body>
</html>
)rawhtml";

// ──────────────────────────────────────────────────────────
// Setup & Loop
// ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  motorInit();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_SN);
  WiFi.softAP(AP_SSID, AP_PASS);
  esp_wifi_set_ps(WIFI_PS_NONE);   

  httpServer.on("/", [](){
    httpServer.sendHeader("Cache-Control","no-store");
    httpServer.send_P(200, "text/html", HTML_PAGE);
  });
  httpServer.begin();

  wsServer.begin();
  wsServer.onEvent(onWebSocketEvent);

  Serial.printf("AP: %s  →  http://%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
}

void loop() {
  httpServer.handleClient();
  wsServer.loop();
 
