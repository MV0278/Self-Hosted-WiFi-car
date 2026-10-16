/*
 * SerBot — ESP32 WiFi Robot Car (Instrumented / Data-Logging Build)
 * Course: 21MEO106T Introduction to Robotics, SRM IST
 *
 * Same motor logic as Robot_Car.cpp, plus:
 *   1. Server-side command processing time (micros) returned on every /go
 *   2. /stats endpoint -> uptime, command count, client RSSI, free heap
 *   3. Browser-side round-trip latency (RTT) logging for every command
 *   4. Live metrics: mean, SD, min, max, P95, jitter, packet loss,
 *      IQR-filtered mean, Pearson r(RSSI, RTT)
 *   5. Speed & 90-degree-turn trial calculator: theoretical vs measured,
 *      abs error, % error, MAE, RMSE
 *   6. One-click CSV export of all collected data
 *
 * Small fixes vs original (listed in deck "Observations"):
 *   - Pointer events replace mouse+touch handlers (no double commands on phones)
 *   - Releasing A/D while W is still held resumes FORWARD instead of stopping
 *   - Unknown commands rejected with HTTP 400 (input validation)
 *
 * NOTE: GPIO12 (IN4) is an ESP32 boot-strapping pin. If the board fails to
 * boot while the motor driver is connected, move IN4 to GPIO13 or GPIO33.
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>

// Motor pins
#define IN1 26   // Left motor: forward
#define IN2 27   // Left motor: backward
#define IN3 14   // Right motor: forward
#define IN4 12   // Right motor: backward
#define ENA 25   // Left motor enable
#define ENB 15   // Right motor enable

const char* ssid = "RoboCar";
const char* password = "robot123";

WebServer server(80);
bool driving_forward = false;
unsigned long cmdCount = 0;
unsigned long rejectedCount = 0;

void enableMotors()  { digitalWrite(ENA, HIGH); digitalWrite(ENB, HIGH); }
void disableMotors() { digitalWrite(ENA, LOW);  digitalWrite(ENB, LOW);  }

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  enableMotors();
  driving_forward = true;
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  enableMotors();
  driving_forward = false;
}

void left() {
  if (driving_forward) {
    digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);   // Stop left wheel
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // Right wheel forward
    enableMotors();
  }
}

void right() {
  if (driving_forward) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // Left wheel forward
    digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);   // Stop right wheel
    enableMotors();
  }
}

void stop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  disableMotors();
  driving_forward = false;
}

// RSSI (dBm) of the first station connected to our soft-AP; 0 if none
int clientRSSI() {
  wifi_sta_list_t list;
  if (esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0) {
    return list.sta[0].rssi;
  }
  return 0;
}

const char page[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SerBot Control + Data</title>
<style>
body{margin:0;padding:0;background:#191a1b;color:#ededed;font-family:'Segoe UI',Arial,sans-serif;}
.container{display:flex;flex-direction:column;align-items:center;padding:16px;}
h1{color:#bbb;letter-spacing:2px;font-size:32px;margin:12px 0 8px;}
h2{color:#bbb;font-size:18px;letter-spacing:1px;margin:22px 0 8px;border-bottom:1px solid #333;padding-bottom:4px;width:100%;}
.tiptext{color:#aaa;margin-bottom:16px;font-size:14px;text-align:center;}
.button-grid{display:grid;grid-template-columns:repeat(3,96px);grid-template-rows:repeat(3,96px);gap:14px;}
.btn{display:flex;align-items:center;justify-content:center;color:#ededed;background:#232325;border:2px solid #636363;border-radius:14px;font-size:22px;cursor:pointer;user-select:none;touch-action:none;}
.btn:active,.btn.pressed{background:#39393b;border-color:#bbb;}
.empty{pointer-events:none;}
.panel{width:100%;max-width:640px;}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px;}
.stat{background:#232325;border:1px solid #333;border-radius:8px;padding:8px;}
.stat b{display:block;font-size:20px;color:#fff;}
.stat span{font-size:12px;color:#999;}
label{font-size:13px;color:#aaa;display:block;margin:6px 0 2px;}
input,textarea{width:100%;box-sizing:border-box;background:#111;color:#eee;border:1px solid #444;border-radius:6px;padding:6px;font-family:monospace;}
button.act{background:#2d2f33;color:#eee;border:1px solid #666;border-radius:8px;padding:8px 12px;margin:6px 6px 0 0;cursor:pointer;}
table{width:100%;border-collapse:collapse;font-size:13px;margin-top:8px;}
td,th{border:1px solid #333;padding:4px 6px;text-align:right;}
th{color:#aaa;}
</style>
</head>
<body>
<div class="container">
  <h1>SERBOT</h1>
  <div class="tiptext">W/S = Forward/Back, A/D = Left/Right (while moving forward). Release to stop.</div>
  <div class="button-grid">
    <div class="empty"></div><button class="btn" data-c="F" id="btnF">FWD</button><div class="empty"></div>
    <button class="btn" data-c="L" id="btnL">LEFT</button><button class="btn" id="btnS">STOP</button><button class="btn" data-c="R" id="btnR">RIGHT</button>
    <div class="empty"></div><button class="btn" data-c="B" id="btnB">BACK</button><div class="empty"></div>
  </div>

  <div class="panel">
    <h2>1. Live link metrics</h2>
    <div class="grid" id="stats"></div>
    <label>Distance from car (m) — tag samples while walking away</label>
    <input id="dist" type="number" value="1" step="0.5">
    <button class="act" onclick="latencyTest()">Run latency test (50 safe STOP pings)</button>
    <button class="act" onclick="downloadCSV()">Download CSV</button>
    <button class="act" onclick="clearData()">Clear</button>

    <h2>2. Speed trials (straight line)</h2>
    <div class="grid">
      <div><label>Motor RPM (no-load spec)</label><input id="rpm" type="number" value="100"></div>
      <div><label>Wheel diameter (mm)</label><input id="dia" type="number" value="65"></div>
      <div><label>Track width b (mm)</label><input id="track" type="number" value="130"></div>
      <div><label>Trial distance (m)</label><input id="tdist" type="number" value="2" step="0.1"></div>
    </div>
    <label>Measured times for the trial distance (s), comma-separated</label>
    <textarea id="times" rows="2" placeholder="e.g. 6.1, 6.3, 5.9, 6.0, 6.2"></textarea>
    <label>Measured 90&deg; turn times (s), comma-separated</label>
    <textarea id="turns" rows="2" placeholder="e.g. 0.62, 0.58, 0.65"></textarea>
    <button class="act" onclick="calcTrials()">Calculate errors</button>
    <div id="trialOut"></div>
  </div>
</div>
<script>
let rtts = [];      // {t,cmd,rtt,proc,rssi,dist,ok}
let rssiLog = [];   // {t,rssi,dist,uptime,cmds,heap}
let lastRSSI = null;
const dist = () => parseFloat(document.getElementById('dist').value) || 0;

async function send(cmd){
  const t0 = performance.now();
  try{
    const ctrl = new AbortController();
    const to = setTimeout(()=>ctrl.abort(), 2000);
    const r = await fetch('/go?d='+cmd, {cache:'no-store', signal:ctrl.signal});
    clearTimeout(to);
    const proc = parseInt(await r.text());
    rtts.push({t:Date.now(), cmd, rtt:performance.now()-t0, proc, rssi:lastRSSI, dist:dist(), ok:1});
  }catch(e){
    rtts.push({t:Date.now(), cmd, rtt:null, proc:null, rssi:lastRSSI, dist:dist(), ok:0});
  }
  render();
}

// ---- Controls: pointer events (one event model for mouse + touch) ----
document.querySelectorAll('.btn[data-c]').forEach(b=>{
  b.addEventListener('pointerdown', e=>{e.preventDefault(); b.classList.add('pressed'); send(b.dataset.c);});
  const up = ()=>{ if(b.classList.contains('pressed')){ b.classList.remove('pressed'); send('S'); } };
  b.addEventListener('pointerup', up); b.addEventListener('pointerleave', up); b.addEventListener('pointercancel', up);
});
document.getElementById('btnS').addEventListener('click', ()=>send('S'));

const keyMap = {w:['F','btnF'], a:['L','btnL'], s:['B','btnB'], d:['R','btnR']};
const held = {};
document.addEventListener('keydown', e=>{
  if(e.repeat || e.target.tagName==='INPUT' || e.target.tagName==='TEXTAREA') return;
  const k = e.key.toLowerCase(); if(!keyMap[k]) return;
  held[k] = true; send(keyMap[k][0]); document.getElementById(keyMap[k][1]).classList.add('pressed');
});
document.addEventListener('keyup', e=>{
  const k = e.key.toLowerCase(); if(!keyMap[k] || !held[k]) return;
  held[k] = false; document.getElementById(keyMap[k][1]).classList.remove('pressed');
  if((k==='a' || k==='d') && held['w']) send('F');   // resume forward if W still held
  else send('S');
});

// ---- Statistics ----
const mean = a => a.reduce((s,x)=>s+x,0)/a.length;
const sd = a => { const m=mean(a); return Math.sqrt(a.reduce((s,x)=>s+(x-m)**2,0)/(a.length>1?a.length-1:1)); };
const pct = (a,p) => { const s=[...a].sort((x,y)=>x-y); return s[Math.min(s.length-1, Math.ceil(p/100*s.length)-1)]; };
function iqrFilter(a){
  if(a.length<4) return a;
  const q1=pct(a,25), q3=pct(a,75), i=q3-q1;
  return a.filter(x=>x>=q1-1.5*i && x<=q3+1.5*i);
}
function pearson(x,y){
  const n=x.length; if(n<3) return null;
  const mx=mean(x), my=mean(y); let sxy=0,sxx=0,syy=0;
  for(let i=0;i<n;i++){ sxy+=(x[i]-mx)*(y[i]-my); sxx+=(x[i]-mx)**2; syy+=(y[i]-my)**2; }
  return (sxx&&syy) ? sxy/Math.sqrt(sxx*syy) : null;
}
const f = (v,d=1) => (v===null||v===undefined||isNaN(v)) ? '—' : v.toFixed(d);

function render(){
  const ok = rtts.filter(r=>r.ok), a = ok.map(r=>r.rtt);
  const loss = rtts.length ? 100*(rtts.length-ok.length)/rtts.length : null;
  let jitter = null;
  if(a.length>1){ let s=0; for(let i=1;i<a.length;i++) s+=Math.abs(a[i]-a[i-1]); jitter=s/(a.length-1); }
  const paired = ok.filter(r=>r.rssi!==null && r.rssi!==0);
  const r = pearson(paired.map(p=>p.rssi), paired.map(p=>p.rtt));
  const procs = ok.map(r=>r.proc).filter(x=>!isNaN(x));
  const filt = iqrFilter(a);
  const cells = [
    ['Commands sent', rtts.length, 0], ['Mean RTT (ms)', a.length?mean(a):null, 1],
    ['SD RTT (ms)', a.length?sd(a):null, 1], ['Min / Max (ms)', a.length?(Math.min(...a).toFixed(0)+' / '+Math.max(...a).toFixed(0)):null],
    ['P95 RTT (ms)', a.length?pct(a,95):null, 1], ['Jitter (ms)', jitter, 1],
    ['Packet loss (%)', loss, 1], ['IQR-filtered mean (ms)', filt.length?mean(filt):null, 1],
    ['Outliers removed', a.length-filt.length, 0], ['ESP32 proc. time (\u00b5s)', procs.length?mean(procs):null, 0],
    ['RSSI now (dBm)', lastRSSI, 0], ['r(RSSI, RTT)', r, 2]
  ];
  document.getElementById('stats').innerHTML = cells.map(c=>
    `<div class="stat"><b>${typeof c[1]==='string'?c[1]:f(c[1],c[2])}</b><span>${c[0]}</span></div>`).join('');
}

async function pollStats(){
  try{
    const r = await fetch('/stats', {cache:'no-store'}); const j = await r.json();
    lastRSSI = j.rssi; rssiLog.push({t:Date.now(), rssi:j.rssi, dist:dist(), uptime:j.uptime, cmds:j.cmds, heap:j.heap});
    render();
  }catch(e){}
}
setInterval(pollStats, 1000);

async function latencyTest(){
  for(let i=0;i<50;i++){ await send('S'); await new Promise(r=>setTimeout(r,100)); }
}

function calcTrials(){
  const N=+document.getElementById('rpm').value, D=+document.getElementById('dia').value/1000;
  const b=+document.getElementById('track').value/1000, L=+document.getElementById('tdist').value;
  const vT = Math.PI*D*N/60;                       // theoretical wheel speed (m/s)
  const tTurnT = (Math.PI/2)*b/vT;                 // one wheel stopped -> pivot about it
  const parse = id => document.getElementById(id).value.split(',').map(s=>parseFloat(s)).filter(x=>x>0);
  const rows = (vals, theo, unit, conv) => {
    if(!vals.length) return '';
    const meas = vals.map(conv), err = meas.map(m=>m-theo);
    const mae = mean(err.map(Math.abs)), rmse = Math.sqrt(mean(err.map(e=>e*e)));
    return `<table><tr><th>Trial</th><th>Measured (${unit})</th><th>Theoretical</th><th>Abs error</th><th>% error</th></tr>`+
      meas.map((m,i)=>`<tr><td>${i+1}</td><td>${m.toFixed(3)}</td><td>${theo.toFixed(3)}</td><td>${Math.abs(err[i]).toFixed(3)}</td><td>${(100*Math.abs(err[i])/theo).toFixed(1)}</td></tr>`).join('')+
      `<tr><th>Mean</th><td>${mean(meas).toFixed(3)}</td><td></td><td>MAE ${mae.toFixed(3)}</td><td>${(100*mae/theo).toFixed(1)}</td></tr>`+
      `<tr><th>SD</th><td>${sd(meas).toFixed(3)}</td><td></td><td>RMSE ${rmse.toFixed(3)}</td><td></td></tr></table>`;
  };
  document.getElementById('trialOut').innerHTML =
    `<p style="font-size:13px;color:#aaa">Theoretical v = &pi;DN/60 = ${vT.toFixed(3)} m/s &nbsp;|&nbsp; 90&deg; turn t = (&pi;/2)&middot;b / v = ${tTurnT.toFixed(3)} s</p>`+
    rows(parse('times'), vT, 'm/s', t=>L/t) + rows(parse('turns'), tTurnT, 's', t=>t);
}

function downloadCSV(){
  let csv = 'type,timestamp_ms,command,rtt_ms,esp_proc_us,rssi_dbm,distance_m,ok,uptime_ms,cmds,free_heap\n';
  rtts.forEach(r=> csv += `cmd,${r.t},${r.cmd},${r.rtt===null?'':r.rtt.toFixed(2)},${r.proc===null?'':r.proc},${r.rssi??''},${r.dist},${r.ok},,,\n`);
  rssiLog.forEach(s=> csv += `rssi,${s.t},,,,${s.rssi},${s.dist},1,${s.uptime},${s.cmds},${s.heap}\n`);
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([csv], {type:'text/csv'}));
  a.download = 'serbot_data_'+Date.now()+'.csv'; a.click();
}
function clearData(){ rtts=[]; rssiLog=[]; render(); }
render();
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", page); }

void handleGo() {
  unsigned long t0 = micros();
  String d = server.hasArg("d") ? server.arg("d") : "";
  bool valid = true;
  if      (d == "F") forward();
  else if (d == "B") backward();
  else if (d == "L") left();
  else if (d == "R") right();
  else if (d == "S") stop();
  else valid = false;                       // reject anything not in the whitelist
  unsigned long procUs = micros() - t0;     // measured before Serial print

  if (valid) cmdCount++; else rejectedCount++;
  // CSV line on Serial: millis,command,proc_us,rssi
  Serial.printf("%lu,%s,%lu,%d\n", millis(), d.c_str(), procUs, clientRSSI());
  server.send(valid ? 200 : 400, "text/plain", String(procUs));
}

void handleStats() {
  String j = "{\"uptime\":" + String(millis()) +
             ",\"cmds\":" + String(cmdCount) +
             ",\"rejected\":" + String(rejectedCount) +
             ",\"rssi\":" + String(clientRSSI()) +
             ",\"heap\":" + String(ESP.getFreeHeap()) +
             ",\"clients\":" + String(WiFi.softAPgetStationNum()) + "}";
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== SerBot data-logging build ===");
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  stop();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("Connect to: "); Serial.println(ssid);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
  Serial.println("millis,command,proc_us,rssi");
  server.on("/", handleRoot);
  server.on("/go", handleGo);
  server.on("/stats", handleStats);
  server.begin();
  Serial.println("Ready!");
}

void loop() { server.handleClient(); }
