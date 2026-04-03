// ============================================================
//   GeoNode-VZ  —  Monitor Sísmico IoT  · Triple Contingencia
//   Hardware  : ESP32 DevKit V1
//   Sensores  : Adafruit MPU6050, Potenciómetro
//   Periféricos: Buzzer PWM, LED Rojo PWM, LED Verde, BT Classic
//   Conectiv. : WiFi AP+STA, Bluetooth Serial, HTTP REST
//   Versión   : 3.1  (depurada)
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ─── PINES ───────────────────────────────────────────────────────
#define PIN_BZ   2
#define PIN_RED  4
#define PIN_GN  16
#define PIN_POT 34

// ─── CANALES PWM ─────────────────────────────────────────────────
#define CH_BZ  0
#define CH_RED 1

// ─── CREDENCIALES WiFi ───────────────────────────────────────────
const char* STA_SSID = "Wokwi-GUEST";
const char* STA_PASS = "";
const char* AP_SSID  = "GeoNode-VZ";
const char* AP_PASS  = "";

// ─── OBJETOS ─────────────────────────────────────────────────────
WebServer      server(80);
Adafruit_MPU6050 mpu;

// ─── ESTADO GLOBAL ───────────────────────────────────────────────
float    mag        = 0.0f;
float    umbral     = 4.5f;
float    ax = 0, ay = 0, az = 9.81f;
float    gx = 0, gy = 0, gz = 0;
float    tempC      = 25.0f;   // ← temperatura MPU6050 (único nombre)
String   statusIA   = "ESTRUCTURA SEGURA";
uint32_t eventCount = 0;
uint32_t uptime     = 0;

// ─── HISTORIAL EMA ───────────────────────────────────────────────
#define HIST_SIZE 60
float   history[HIST_SIZE];
uint8_t histIdx = 0;

// ──────────────────────────────────────────────────────────────────
//   HTML / CSS / JS  embebido  —  Triple contingencia cliente
// ──────────────────────────────────────────────────────────────────
const char PAGE_HTML[] PROGMEM = R"HTMLEOF(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GeoNode-VZ</title>
<script>window.__chartLoaded=false;</script>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"
  onload="window.__chartLoaded=true"
  onerror="console.warn('CDN no disponible')"></script>
<style>
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&family=Rajdhani:wght@300;400;600;700&display=swap');
:root{
  --void:#020508;--surface:#060d16;--card:#0a1626;--border:#0d2540;
  --accent:#00e5ff;--green:#00ff88;--red:#ff2244;--amber:#ffaa00;
  --text:#cce8ff;--muted:#3a6080;--dim:#1a3550;
}
*{margin:0;padding:0;box-sizing:border-box;}
body{background:var(--void);color:var(--text);font-family:'Rajdhani',sans-serif;min-height:100vh;overflow-x:hidden;}
body::before{content:'';position:fixed;inset:0;pointer-events:none;z-index:0;
  background:repeating-linear-gradient(0deg,transparent,transparent 39px,rgba(0,229,255,.03) 40px),
             repeating-linear-gradient(90deg,transparent,transparent 39px,rgba(0,229,255,.03) 40px);}
body::after{content:'';position:fixed;inset:0;pointer-events:none;z-index:0;
  background:repeating-linear-gradient(0deg,transparent,transparent 3px,rgba(0,0,0,.18) 3px,rgba(0,0,0,.18) 4px);
  opacity:.12;}
.wrap{max-width:1280px;margin:0 auto;padding:16px 20px;position:relative;z-index:1;}
header{display:flex;justify-content:space-between;align-items:center;padding-bottom:16px;margin-bottom:20px;border-bottom:1px solid var(--border);position:relative;}
header::after{content:'';position:absolute;bottom:-1px;left:0;width:180px;height:1px;background:var(--accent);box-shadow:0 0 14px var(--accent);}
.logo{font-family:'Orbitron',monospace;font-size:clamp(1.3rem,3.5vw,1.9rem);font-weight:900;letter-spacing:.1em;color:var(--accent);text-shadow:0 0 24px rgba(0,229,255,.5),0 0 60px rgba(0,229,255,.15);}
.logo sub{font-size:.55rem;letter-spacing:.2em;color:var(--muted);vertical-align:middle;margin-left:10px;border:1px solid var(--border);padding:2px 7px;border-radius:2px;}
.header-r{display:flex;flex-direction:column;align-items:flex-end;gap:6px;}
.clock{font-family:'Share Tech Mono',monospace;font-size:.68rem;color:var(--muted);}
#conn-badge{font-family:'Share Tech Mono',monospace;font-size:.6rem;letter-spacing:.18em;padding:3px 10px;border-radius:2px;display:flex;align-items:center;gap:6px;border:1px solid;transition:all .4s;}
#conn-badge.internet{color:var(--green);border-color:rgba(0,255,136,.35);background:rgba(0,255,136,.05);}
#conn-badge.intranet{color:var(--amber);border-color:rgba(255,170,0,.35);background:rgba(255,170,0,.05);}
#conn-badge.serial{color:var(--accent);border-color:rgba(0,229,255,.35);background:rgba(0,229,255,.05);}
#conn-badge.offline{color:var(--red);border-color:rgba(255,34,68,.35);background:rgba(255,34,68,.05);}
.dot{width:6px;height:6px;border-radius:50%;background:currentColor;animation:blink 1.4s ease-in-out infinite;}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.contingency-bar{display:flex;gap:8px;margin-bottom:18px;font-family:'Share Tech Mono',monospace;font-size:.6rem;}
.ct-item{flex:1;padding:8px 10px;border-radius:3px;border:1px solid var(--border);display:flex;align-items:center;gap:8px;transition:all .4s;}
.ct-item .ct-dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .4s;}
.ct-item.active{border-color:rgba(0,255,136,.4);background:rgba(0,255,136,.04);}
.ct-item.active .ct-dot{background:var(--green);box-shadow:0 0 8px var(--green);animation:blink 1.2s infinite;}
.ct-item.standby{border-color:rgba(255,170,0,.3);background:rgba(255,170,0,.03);}
.ct-item.standby .ct-dot{background:var(--amber);}
.ct-item.inactive{opacity:.4;}
.ct-label{color:var(--text);letter-spacing:.12em;}
.ct-sub{color:var(--muted);font-size:.55rem;margin-left:auto;}
.metrics{display:grid;grid-template-columns:1.4fr 1fr 1fr;gap:14px;margin-bottom:14px;}
@media(max-width:700px){.metrics{grid-template-columns:1fr;}}
.panel{background:var(--card);border:1px solid var(--border);border-radius:4px;padding:18px 20px;position:relative;overflow:hidden;transition:border-color .3s;}
.panel::before{content:'';position:absolute;top:0;left:0;width:2px;height:100%;background:var(--accent);box-shadow:0 0 8px var(--accent);opacity:.35;}
.panel::after{content:'';position:absolute;top:0;right:0;width:16px;height:16px;border-top:1px solid var(--accent);border-right:1px solid var(--accent);opacity:.3;}
.panel-label{font-family:'Share Tech Mono',monospace;font-size:.58rem;color:var(--muted);letter-spacing:.22em;text-transform:uppercase;margin-bottom:10px;}
.pga-val{font-family:'Orbitron',monospace;font-size:clamp(2.8rem,7vw,4.2rem);font-weight:900;color:#fff;line-height:1;text-shadow:0 0 40px rgba(255,255,255,.2);transition:color .4s,text-shadow .4s;}
.pga-unit{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--muted);margin-top:4px;}
.pga-val.alerta{color:var(--red);text-shadow:0 0 30px rgba(255,34,68,.7),0 0 60px rgba(255,34,68,.3);animation:pulse-red .5s ease-in-out infinite alternate;}
@keyframes pulse-red{from{opacity:1}to{opacity:.7}}
#status-panel.danger{border-color:rgba(255,34,68,.5);}
#status-panel.danger::before{background:var(--red);box-shadow:0 0 8px var(--red);}
.status-icon{font-size:1.6rem;margin-bottom:6px;}
#status-text{font-family:'Orbitron',monospace;font-size:.9rem;font-weight:700;letter-spacing:.08em;color:var(--green);transition:color .4s;}
#status-text.danger{color:var(--red);}
#ia-desc{font-size:.75rem;color:var(--muted);margin-top:8px;line-height:1.5;}
.umbral-val{font-family:'Orbitron',monospace;font-size:2rem;font-weight:700;color:var(--amber);}
.prog-track{height:4px;background:var(--border);border-radius:2px;margin:10px 0 4px;overflow:hidden;}
.prog-bar{height:100%;width:45%;border-radius:2px;background:linear-gradient(90deg,var(--amber),#ff6600);transition:width .5s ease;}
.prog-labels{display:flex;justify-content:space-between;font-family:'Share Tech Mono',monospace;font-size:.55rem;color:var(--dim);}
.intensity-grid{display:grid;grid-template-columns:repeat(12,1fr);gap:3px;margin-top:10px;}
.i-seg{height:8px;border-radius:1px;background:var(--border);transition:background .15s,box-shadow .15s;}
.chart-panel{background:var(--card);border:1px solid var(--border);border-radius:4px;padding:16px;margin-bottom:14px;}
.chart-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;}
.chart-title{font-family:'Share Tech Mono',monospace;font-size:.6rem;color:var(--muted);letter-spacing:.18em;}
.chart-live{font-family:'Share Tech Mono',monospace;font-size:.55rem;color:var(--green);border:1px solid rgba(0,255,136,.35);padding:2px 8px;border-radius:2px;animation:blink 1s ease-in-out infinite;}
#serial-panel{display:none;background:var(--surface);border:1px solid rgba(0,229,255,.2);border-radius:4px;padding:14px;margin-bottom:14px;}
#serial-panel.visible{display:block;}
.serial-log{font-family:'Share Tech Mono',monospace;font-size:.65rem;color:var(--muted);height:80px;overflow-y:auto;border:1px solid var(--border);padding:8px;border-radius:3px;background:var(--void);}
#btn-serial{font-family:'Share Tech Mono',monospace;font-size:.65rem;background:rgba(0,229,255,.08);border:1px solid rgba(0,229,255,.35);color:var(--accent);padding:6px 16px;border-radius:3px;cursor:pointer;letter-spacing:.15em;transition:all .2s;margin-right:8px;}
#btn-serial:hover{background:rgba(0,229,255,.15);}
.status-bar{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;background:var(--surface);border:1px solid var(--border);border-radius:4px;padding:12px 16px;margin-bottom:14px;}
@media(max-width:600px){.status-bar{grid-template-columns:repeat(2,1fr);}}
.sb-label{font-family:'Share Tech Mono',monospace;font-size:.55rem;color:var(--dim);letter-spacing:.15em;margin-bottom:4px;}
.sb-val{font-family:'Share Tech Mono',monospace;font-size:.7rem;color:var(--text);}
.sb-val.ok{color:var(--green);}.sb-val.warn{color:var(--amber);}.sb-val.err{color:var(--red);}
footer{text-align:center;padding-top:12px;font-family:'Share Tech Mono',monospace;font-size:.6rem;color:var(--dim);letter-spacing:.2em;border-top:1px solid var(--border);}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div>
      <div class="logo">GEONODE<span style="color:var(--muted)">-</span>VZ<sub>IoT · v3.1</sub></div>
      <div style="font-size:.7rem;color:var(--muted);letter-spacing:.2em;margin-top:6px;">MONITOR SÍSMICO DE ESTRUCTURA · TRIPLE CONTINGENCIA</div>
    </div>
    <div class="header-r">
      <div class="clock" id="sys-clock">--:--:-- UTC</div>
      <div id="conn-badge" class="offline"><span class="dot"></span><span id="conn-mode">CONECTANDO</span></div>
    </div>
  </header>
  <div class="contingency-bar">
    <div class="ct-item inactive" id="ct-internet"><span class="ct-dot"></span><span class="ct-label">INTERNET</span><span class="ct-sub" id="ct-internet-ip">—</span></div>
    <div class="ct-item inactive" id="ct-intranet"><span class="ct-dot"></span><span class="ct-label">INTRANET AP</span><span class="ct-sub">192.168.4.1</span></div>
    <div class="ct-item inactive" id="ct-serial"><span class="ct-dot"></span><span class="ct-label">BLUETOOTH / SERIAL</span><span class="ct-sub">Web Serial API</span></div>
  </div>
  <div class="metrics">
    <div class="panel">
      <div class="panel-label">⬡ Aceleración PGA</div>
      <div class="pga-val" id="pga-val">0.00</div>
      <div class="pga-unit">m/s²  ·  EMA α=0.2</div>
      <div class="intensity-grid" id="intensity-grid"></div>
    </div>
    <div class="panel" id="status-panel">
      <div class="panel-label">⬡ Diagnóstico IA</div>
      <div class="status-icon" id="status-icon">🟢</div>
      <div id="status-text">ESTRUCTURA SEGURA</div>
      <div id="ia-desc">Parámetros en rango nominal. Sistema operando con normalidad.</div>
    </div>
    <div class="panel">
      <div class="panel-label">⬡ Umbral Potenciómetro</div>
      <div class="umbral-val" id="umbral-val">4.5 <span style="font-size:.9rem;color:var(--muted)">m/s²</span></div>
      <div class="prog-track"><div class="prog-bar" id="umbral-bar"></div></div>
      <div class="prog-labels"><span>1.0</span><span>5.5</span><span>10.0</span></div>
    </div>
  </div>
  <div class="chart-panel">
    <div class="chart-header">
      <div class="chart-title">SISMÓGRAFO · TIEMPO REAL · EJE XYZ COMPUESTO</div>
      <div class="chart-live">● LIVE</div>
    </div>
    <canvas id="sismoChart" height="75"></canvas>
  </div>
  <div id="serial-panel">
    <div style="display:flex;align-items:center;gap:10px;margin-bottom:10px;">
      <button id="btn-serial" onclick="connectSerial()">⚡ CONECTAR SERIAL / BT</button>
      <span style="font-family:'Share Tech Mono',monospace;font-size:.6rem;color:var(--muted);">Web Serial API — Requiere Chrome/Edge</span>
    </div>
    <div class="serial-log" id="serial-log">Esperando conexión serial...</div>
  </div>
  <div class="status-bar">
    <div><div class="sb-label">Modo Activo</div><div class="sb-val" id="sb-mode">—</div></div>
    <div><div class="sb-label">Eventos Detectados</div><div class="sb-val" id="sb-events">0</div></div>
    <div><div class="sb-label">Temperatura MPU6050</div><div class="sb-val" id="sb-temp">—</div></div>
    <div><div class="sb-label">Uptime ESP32</div><div class="sb-val" id="sb-uptime">—</div></div>
  </div>
  <footer>GEONODE-VZ · TRIPLE CONTINGENCIA · INTERNET · INTRANET · BLUETOOTH SERIAL · 2025</footer>
</div>
<script>
// ═══════════════════════════════════════════════
//   Motor de triple contingencia
//   Internet STA → Intranet AP → Bluetooth/Serial
// ═══════════════════════════════════════════════
const ENDPOINTS     = { internet: null, intranet: 'http://192.168.4.1' };
const FETCH_INTERVAL = 120;
const TIMEOUT_MS     = 1500;
const FAILOVER_TRIES = 3;

let activeMode = 'none', failCount = 0, eventCount = 0;
let wasAlert = false, serialPort = null, serialReader = null, serialBuf = '';

// ── Chart ──────────────────────────────────────
const POINTS = 80;
let chart;
function initChart(){
  if(typeof Chart==='undefined'){ document.querySelector('.chart-live').textContent='● DEMO'; return; }
  const ctx=document.getElementById('sismoChart').getContext('2d');
  chart=new Chart(ctx,{
    type:'line',
    data:{ labels:Array(POINTS).fill(''),
      datasets:[{ label:'PGA', data:Array(POINTS).fill(0),
        borderColor:'#00ff88', borderWidth:1.5, tension:0.3, fill:true,
        backgroundColor:(c)=>{ const g=c.chart.ctx.createLinearGradient(0,0,0,180);
          g.addColorStop(0,'rgba(0,255,136,0.2)'); g.addColorStop(1,'rgba(0,255,136,0)'); return g; },
        pointRadius:0 }]
    },
    options:{ responsive:true, animation:false,
      scales:{
        y:{ min:0, max:12, grid:{color:'rgba(0,229,255,0.04)'},
          ticks:{color:'#1a3550',font:{family:"'Share Tech Mono'",size:9},maxTicksLimit:6} },
        x:{display:false}
      },
      plugins:{legend:{display:false}}, interaction:{intersect:false}
    }
  });
}

// ── Intensidad ─────────────────────────────────
const SEGS=12;
(function buildIntensity(){
  const g=document.getElementById('intensity-grid');
  for(let i=0;i<SEGS;i++){ const d=document.createElement('div'); d.className='i-seg'; g.appendChild(d); }
})();
function updateIntensity(mag,umbral){
  const segs=document.querySelectorAll('.i-seg');
  const ratio=Math.min(mag/12,1);
  segs.forEach((s,i)=>{
    const t=(i+1)/SEGS;
    if(t<=ratio){
      if(mag>umbral){ s.style.background='#ff2244'; s.style.boxShadow='0 0 5px #ff2244'; }
      else if(ratio>.65){ s.style.background='#ffaa00'; s.style.boxShadow='0 0 5px #ffaa00'; }
      else { s.style.background='#00ff88'; s.style.boxShadow='0 0 5px #00ff88'; }
    } else { s.style.background='var(--border)'; s.style.boxShadow='none'; }
  });
}

// ── Reloj ──────────────────────────────────────
setInterval(()=>{
  const n=new Date();
  document.getElementById('sys-clock').textContent=n.toUTCString().slice(17,25)+' UTC';
},1000);

// ── Aplicar datos ──────────────────────────────
function applyData(d){
  const mag   =isFinite(d.mag)    ? +d.mag    : 0;
  const umbral=isFinite(d.umbral) ? +d.umbral : 4.5;
  const status=d.status||'ESTRUCTURA SEGURA';
  const pgaEl=document.getElementById('pga-val');
  pgaEl.textContent=mag.toFixed(2);
  pgaEl.classList.toggle('alerta',mag>umbral);
  const sp=document.getElementById('status-panel');
  const st=document.getElementById('status-text');
  const icon=document.getElementById('status-icon');
  const desc=document.getElementById('ia-desc');
  if(mag>umbral){
    sp.classList.add('danger'); st.textContent=mag>7?'⚠ RIESGO CRÍTICO':'⚠ SISMO DETECTADO';
    st.className='danger'; icon.textContent='🔴';
    desc.textContent='Vibración supera umbral. Evaluando integridad estructural...';
    if(chart) chart.data.datasets[0].borderColor='#ff2244';
    if(!wasAlert){ eventCount++; document.getElementById('sb-events').textContent=eventCount; }
    wasAlert=true;
  } else {
    sp.classList.remove('danger'); st.textContent=status; st.className='';
    icon.textContent='🟢';
    desc.textContent='Análisis en parámetros nominales. Sistema operando con normalidad.';
    if(chart) chart.data.datasets[0].borderColor='#00ff88';
    wasAlert=false;
  }
  const pct=Math.min((umbral/10)*100,100);
  document.getElementById('umbral-val').innerHTML=umbral.toFixed(1)+' <span style="font-size:.9rem;color:var(--muted)">m/s²</span>';
  document.getElementById('umbral-bar').style.width=pct+'%';
  updateIntensity(mag,umbral);
  if(chart){ chart.data.datasets[0].data.push(mag); chart.data.datasets[0].data.shift(); chart.update(); }
  if(d.temp!=null)   document.getElementById('sb-temp').textContent=(+d.temp).toFixed(1)+'°C';
  if(d.uptime!=null){
    const s=+d.uptime, h=Math.floor(s/3600), m=Math.floor((s%3600)/60), sec=s%60;
    document.getElementById('sb-uptime').textContent=
      `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
  }
}

// ── UI helpers ─────────────────────────────────
function setMode(mode,ip){
  activeMode=mode; failCount=0;
  const badge=document.getElementById('conn-badge');
  const modeEl=document.getElementById('conn-mode');
  const sbMode=document.getElementById('sb-mode');
  badge.className='offline';
  ['ct-internet','ct-intranet','ct-serial'].forEach(id=>{document.getElementById(id).className='ct-item inactive';});
  if(mode==='internet'){
    badge.className='internet'; modeEl.textContent='INTERNET';
    sbMode.innerHTML='<span class="ok">INTERNET STA</span>';
    document.getElementById('ct-internet').className='ct-item active';
    document.getElementById('ct-intranet').className='ct-item standby';
    document.getElementById('ct-serial').className='ct-item standby';
    if(ip) document.getElementById('ct-internet-ip').textContent=ip;
    document.getElementById('serial-panel').classList.remove('visible');
  } else if(mode==='intranet'){
    badge.className='intranet'; modeEl.textContent='INTRANET AP';
    sbMode.innerHTML='<span class="warn">INTRANET AP</span>';
    document.getElementById('ct-intranet').className='ct-item active';
    document.getElementById('ct-serial').className='ct-item standby';
    document.getElementById('serial-panel').classList.remove('visible');
  } else if(mode==='serial'){
    badge.className='serial'; modeEl.textContent='BT / SERIAL';
    sbMode.innerHTML='<span class="ok">BLUETOOTH SERIAL</span>';
    document.getElementById('ct-serial').className='ct-item active';
    document.getElementById('serial-panel').classList.add('visible');
  } else {
    modeEl.textContent='SIN SEÑAL'; sbMode.innerHTML='<span class="err">OFFLINE</span>';
  }
}

function onFail(){
  failCount++;
  if(failCount<FAILOVER_TRIES) return;
  if(activeMode==='internet'){ console.warn('[GeoNode] Internet → Intranet'); setMode('intranet'); }
  else if(activeMode==='intranet'){ console.warn('[GeoNode] Intranet → Serial'); setMode('serial'); stopFetchLoop(); }
}

// ── Fetch ──────────────────────────────────────
async function fetchWithTimeout(url){
  const ctrl=new AbortController();
  const tid=setTimeout(()=>ctrl.abort(),TIMEOUT_MS);
  try{
    const r=await fetch(url+'/data',{signal:ctrl.signal,cache:'no-store',headers:{'Accept':'application/json'}});
    clearTimeout(tid);
    if(!r.ok) throw new Error('HTTP '+r.status);
    return await r.json();
  } finally { clearTimeout(tid); }
}
let fetchTimer=null;
function startFetchLoop(){ if(fetchTimer) return; fetchTimer=setInterval(doFetch,FETCH_INTERVAL); doFetch(); }
function stopFetchLoop(){ clearInterval(fetchTimer); fetchTimer=null; }
async function doFetch(){
  const url=activeMode==='internet'?ENDPOINTS.internet:ENDPOINTS.intranet;
  if(!url){ onFail(); return; }
  try{ const data=await fetchWithTimeout(url); applyData(data); failCount=0; }
  catch(e){ console.warn('[GeoNode] fetch error:',e.message); onFail(); }
}

// ── Web Serial / Bluetooth ─────────────────────
async function connectSerial(){
  if(!('serial' in navigator)){ logSerial('Web Serial API no disponible. Usa Chrome o Edge.'); return; }
  try{
    serialPort=await navigator.serial.requestPort();
    await serialPort.open({baudRate:115200});
    logSerial('Puerto serial abierto — esperando datos...');
    const decoder=new TextDecoderStream();
    serialPort.readable.pipeTo(decoder.writable);
    serialReader=decoder.readable.getReader();
    serialBuf='';
    readSerial();
  } catch(e){ logSerial('Error: '+e.message); }
}
async function readSerial(){
  while(true){
    try{
      const {value,done}=await serialReader.read();
      if(done) break;
      serialBuf+=value;
      let nl;
      while((nl=serialBuf.indexOf('\n'))>-1){
        const line=serialBuf.slice(0,nl).trim();
        serialBuf=serialBuf.slice(nl+1);
        if(line.startsWith('{')){
          try{ const d=JSON.parse(line); applyData(d); logSerial('RX: mag='+d.mag+' | '+d.status); }
          catch(e){ logSerial('JSON err: '+line.slice(0,40)); }
        }
      }
    } catch(e){ break; }
  }
}
function logSerial(msg){ const el=document.getElementById('serial-log'); el.textContent+='\n'+msg; el.scrollTop=el.scrollHeight; }

// ── Auto-detección de canal ────────────────────
async function detectBestChannel(){
  const origin=window.location.origin;
  const isESP=/192\.168\.\d+\.\d+/.test(origin)||origin.includes('geonode');
  if(isESP){
    if(origin.includes('192.168.4.')){ ENDPOINTS.intranet=origin; setMode('intranet'); }
    else { ENDPOINTS.internet=origin; setMode('internet'); document.getElementById('ct-internet-ip').textContent=origin.replace('http://',''); }
    startFetchLoop(); return;
  }
  try{ await fetchWithTimeout('http://192.168.1.100'); ENDPOINTS.internet='http://192.168.1.100'; setMode('internet','192.168.1.100'); startFetchLoop(); return; } catch(e){}
  try{ await fetchWithTimeout('http://192.168.4.1'); setMode('intranet'); startFetchLoop(); return; } catch(e){}
  console.warn('[GeoNode] HTTP no disponible → Serial en espera');
  setMode('serial');
}

initChart();
detectBestChannel();
</script>
</body>
</html>
)HTMLEOF";

// ──────────────────────────────────────────────
//   SETUP
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  SerialBT.begin("GeoNode-VZ_BT");
  Serial.println("[BT] Bluetooth listo: GeoNode-VZ_BT");

  // ── PWM ────────────────────────────────────
  ledcSetup(CH_BZ,  2000, 8);
  ledcAttachPin(PIN_BZ,  CH_BZ);
  ledcSetup(CH_RED, 5000, 8);
  ledcAttachPin(PIN_RED, CH_RED);
  pinMode(PIN_GN, OUTPUT);
  digitalWrite(PIN_GN, HIGH);

  // ── Historial ──────────────────────────────
  memset(history, 0, sizeof(history));

  // ── WiFi AP + STA ──────────────────────────
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("[WiFi] AP activo: " + String(AP_SSID));
  Serial.println("[WiFi] AP IP: "     + WiFi.softAPIP().toString());

  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("[WiFi] Conectando STA");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(400);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("[WiFi] STA conectado: " + WiFi.localIP().toString());
  else
    Serial.println("[WiFi] STA sin conexión — solo AP disponible");

  // ── I2C + MPU6050 ──────────────────────────
  Wire.begin(21, 22);
  if (!mpu.begin()) {
    Serial.println("[ERROR] MPU6050 no encontrado");
    while (1) { delay(500); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("[MPU6050] OK");

  // ── Rutas HTTP ─────────────────────────────

  // GET / → Página principal
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send_P(200, "text/html; charset=UTF-8", PAGE_HTML);
  });

  // GET /data → JSON completo
  server.on("/data", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Cache-Control", "no-cache, no-store");

    float safeMag = (isnan(mag)    || isinf(mag))    ? 0.0f : mag;
    float safeUmb = (isnan(umbral) || isinf(umbral)) ? 4.5f : umbral;
    float safeT   = (isnan(tempC)  || isinf(tempC))  ? 0.0f : tempC;

    // ── Buffers de conversión float→string ──
    char bMag[12], bUmb[10], bAx[10], bAy[10], bAz[10], bTemp[10];
    dtostrf(safeMag, 6, 3, bMag);
    dtostrf(safeUmb, 5, 2, bUmb);
    dtostrf(ax,      6, 3, bAx);
    dtostrf(ay,      6, 3, bAy);
    dtostrf(az,      6, 3, bAz);
    dtostrf(safeT,   5, 1, bTemp);   // ← tempC → bTemp (corregido)

    String staIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";

    String json = "{";
    json += "\"mag\":"     + String(bMag)  + ",";
    json += "\"umbral\":"  + String(bUmb)  + ",";
    json += "\"status\":\"" + statusIA    + "\",";
    json += "\"ax\":"      + String(bAx)  + ",";
    json += "\"ay\":"      + String(bAy)  + ",";
    json += "\"az\":"      + String(bAz)  + ",";
    json += "\"temp\":"    + String(bTemp) + ",";
    json += "\"uptime\":"  + String(uptime) + ",";
    json += "\"events\":"  + String(eventCount) + ",";
    json += "\"sta_ip\":\"" + staIP       + "\",";
    json += "\"ap_ip\":\"192.168.4.1\"";
    json += "}";

    server.send(200, "application/json", json);
  });

  // OPTIONS preflight CORS
  server.on("/data", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.send(204);
  });

  // GET /status → Resumen de red
  server.on("/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    String ap  = WiFi.softAPIP().toString();
    String sta = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "N/A";
    String json = "{\"ap\":\"" + ap + "\",\"sta\":\"" + sta + "\",\"uptime\":" + String(uptime) + "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("[HTTP] Servidor activo en puerto 80");
  Serial.println("[Sistema] GeoNode-VZ listo — Triple Contingencia activada");
  Serial.println("[URLs] AP:  http://192.168.4.1");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("[URLs] STA: http://" + WiFi.localIP().toString());
}

// ──────────────────────────────────────────────
//   LOOP
// ──────────────────────────────────────────────
uint32_t lastRead   = 0;
uint32_t lastSerial = 0;
uint32_t lastUptime = 0;
bool     prevAlert  = false;

void loop() {
  server.handleClient();

  uint32_t now = millis();

  // ── Uptime (1 segundo) ─────────────────────
  if (now - lastUptime >= 1000) {
    uptime++;
    lastUptime = now;
  }

  // ── Lectura MPU6050 (cada 20 ms) ──────────
  if (now - lastRead >= 20) {
    lastRead = now;

    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    ax    = a.acceleration.x;
    ay    = a.acceleration.y;
    az    = a.acceleration.z;
    gx    = g.gyro.x;
    gy    = g.gyro.y;
    gz    = g.gyro.z;
    tempC = t.temperature;   // ← única asignación de temperatura

    // Potenciómetro → umbral 1.0 – 10.0 m/s²
    umbral = map(analogRead(PIN_POT), 0, 4095, 10, 100) / 10.0f;

    // Magnitud EMA (sin gravedad en Z)
    float raw = sqrtf(sq(ax) + sq(ay) + sq(az - 9.81f));
    if (isnan(mag)) mag = 0.0f;
    mag = (0.2f * raw) + (0.8f * mag);

    // Historial circular
    history[histIdx] = mag;
    histIdx = (histIdx + 1) % HIST_SIZE;

    // ── Lógica de alerta ────────────────────
    bool alert = (mag > umbral);

    if (alert) {
      statusIA = (mag > 7.0f) ? "RIESGO CRITICO" : "SISMO DETECTADO";
      int intens = (int)map(constrain((int)(mag * 10), 10, 100), 10, 100, 40, 255);
      ledcWrite(CH_BZ,  intens);
      ledcWrite(CH_RED, intens);
      digitalWrite(PIN_GN, LOW);
      if (!prevAlert) eventCount++;
    } else {
      statusIA = "ESTRUCTURA SEGURA";
      ledcWrite(CH_BZ,  0);
      ledcWrite(CH_RED, 0);
      digitalWrite(PIN_GN, HIGH);
    }
    prevAlert = alert;
  }

  // ── JSON Serial + Bluetooth (cada 100 ms) ─
  if (now - lastSerial >= 100) {
    lastSerial = now;

    char bMag[10], bUmb[8], bTemp[8];
    dtostrf(mag,    4, 3, bMag);
    dtostrf(umbral, 4, 1, bUmb);
    dtostrf(tempC,  4, 1, bTemp);   // ← tempC → bTemp (corregido)

    // Canal 1: USB Serial
    Serial.print("{\"mag\":");        Serial.print(bMag);
    Serial.print(",\"status\":\"");   Serial.print(statusIA);
    Serial.print("\",\"umbral\":");   Serial.print(bUmb);
    Serial.print(",\"temp\":");       Serial.print(bTemp);
    Serial.print(",\"uptime\":");     Serial.print(now / 1000);
    Serial.println("}");

    // Canal 3: Bluetooth Classic
    SerialBT.print("{\"mag\":");      SerialBT.print(bMag);
    SerialBT.print(",\"status\":\""); SerialBT.print(statusIA);
    SerialBT.print("\",\"umbral\":"); SerialBT.print(bUmb);
    SerialBT.print(",\"temp\":");     SerialBT.print(bTemp);
    SerialBT.print(",\"uptime\":");   SerialBT.print(now / 1000);
    SerialBT.println("}");
  }
}
