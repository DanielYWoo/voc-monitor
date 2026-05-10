#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

#include <Arduino.h>

// Vault-Tec style HTML page stored in PROGMEM
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VAULT-TEC TVOC MONITOR</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Share Tech Mono',monospace;background:#0a0a0a;color:#00ff41;padding:20px;
position:relative;overflow-x:hidden}
body::before{content:'';position:fixed;top:0;left:0;width:100%;height:100%;
background:repeating-linear-gradient(0deg,rgba(0,0,0,0.15),rgba(0,0,0,0.15) 1px,transparent 1px,transparent 2px);
pointer-events:none;z-index:1000}
body::after{content:'';position:fixed;top:0;left:0;width:100%;height:100%;
background:radial-gradient(ellipse at center,transparent 0%,rgba(0,0,0,0.4) 100%);
pointer-events:none;z-index:999}
h1{text-align:center;margin-bottom:20px;color:#00ff41;font-size:2em;
text-shadow:0 0 10px #00ff41,0 0 20px #00ff41;letter-spacing:4px;
border:3px solid #00ff41;padding:15px;position:relative}
h1::before{content:'☢';margin-right:15px}
h1::after{content:'☢';margin-left:15px}
.g{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:20px}
.c{background:#0d1a0d;border:2px solid #00ff41;border-radius:8px;padding:20px;
box-shadow:0 0 10px rgba(0,255,65,0.3),inset 0 0 30px rgba(0,255,65,0.05);position:relative}
.c::before{content:'';position:absolute;top:0;left:0;right:0;height:3px;
background:linear-gradient(90deg,transparent,#00ff41,transparent)}
.c h2{font-size:1em;color:#00ff41;margin-bottom:15px;text-transform:uppercase;
letter-spacing:3px;border-bottom:1px solid #00ff41;padding-bottom:10px;
text-shadow:0 0 5px #00ff41}
.v{font-size:2.2em;font-weight:700;text-shadow:0 0 10px currentColor}
.u{font-size:.45em;color:#00aa30;margin-left:5px}
.ch{color:#ff6b35}.rm{color:#00ff41}
.ch .u{color:#cc5528}.rm .u{color:#00aa30}
.env{margin-top:15px;padding-top:10px;border-top:1px dashed #00aa30;color:#00aa30;font-size:.9em}
.cc{background:#0d1a0d;border:2px solid #00ff41;border-radius:8px;padding:20px;margin-bottom:20px;
box-shadow:0 0 10px rgba(0,255,65,0.3),inset 0 0 30px rgba(0,255,65,0.05)}
.st{text-align:center;color:#00aa30;font-size:.9em;margin-top:20px;
border:1px solid #00aa30;padding:10px;border-radius:4px}
.st::before{content:'[';margin-right:5px}.st::after{content:']';margin-left:5px}
.warn{color:#ff6b35;text-shadow:0 0 5px #ff6b35}
.header-bar{display:flex;justify-content:space-between;align-items:center;
border:2px solid #00ff41;padding:10px 20px;margin-bottom:20px;background:#0d1a0d}
.header-bar span{color:#00aa30;font-size:.8em}
canvas{filter:drop-shadow(0 0 5px rgba(0,255,65,0.5))}
.vault-boy{display:flex;align-items:center;justify-content:center;gap:15px;
margin:20px 0;padding:15px;border:2px dashed #00aa30;border-radius:8px;background:#0d1a0d;
height:90px;overflow:hidden}
.vault-boy svg{width:50px;height:50px;flex-shrink:0;animation:bob 2s ease-in-out infinite}
@keyframes bob{0%,100%{transform:translateY(0)}50%{transform:translateY(-5px)}}
.vault-boy .quote{color:#00ff41;font-style:italic;font-size:.85em;max-width:450px;
text-align:center;line-height:1.3;overflow:hidden;display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical}
</style></head>
<body>
<div class="header-bar">
<span>VAULT-TEC INDUSTRIES</span>
<span>ENVIRONMENTAL MONITORING SYSTEM v2.77</span>
<span id="dt">--</span>
</div>
<h1>TVOC MONITOR</h1>
<div class="vault-boy">
<svg viewBox="0 0 100 100" fill="#00ff41">
<circle cx="50" cy="28" r="22" stroke="#00ff41" stroke-width="3" fill="none"/>
<circle cx="42" cy="24" r="3" fill="#00ff41"/>
<circle cx="58" cy="24" r="3" fill="#00ff41"/>
<path d="M40 34 Q50 42 60 34" stroke="#00ff41" stroke-width="3" fill="none" stroke-linecap="round"/>
<ellipse cx="50" cy="62" rx="18" ry="22" stroke="#00ff41" stroke-width="3" fill="none"/>
<line x1="32" y1="55" x2="15" y2="40" stroke="#00ff41" stroke-width="3" stroke-linecap="round"/>
<circle cx="12" cy="37" r="5" stroke="#00ff41" stroke-width="2" fill="none"/>
<line x1="10" y1="32" x2="18" y2="28" stroke="#00ff41" stroke-width="2" stroke-linecap="round"/>
<line x1="68" y1="55" x2="85" y2="40" stroke="#00ff41" stroke-width="3" stroke-linecap="round"/>
<circle cx="88" cy="37" r="5" stroke="#00ff41" stroke-width="2" fill="none"/>
<line x1="90" y1="32" x2="82" y2="28" stroke="#00ff41" stroke-width="2" stroke-linecap="round"/>
<line x1="40" y1="82" x2="35" y2="98" stroke="#00ff41" stroke-width="3" stroke-linecap="round"/>
<line x1="60" y1="82" x2="65" y2="98" stroke="#00ff41" stroke-width="3" stroke-linecap="round"/>
</svg>
<div class="quote" id="vbQuote">Remember: A clean vault is a happy vault!</div>
</div>
<div class="g">
<div class="c">
<h2>⚠ Chamber Readings</h2>
<div class="v ch"><span id="ct">--</span><span class="u">PPB TVOC</span></div>
<div class="v ch" style="margin-top:10px"><span id="cc">--</span><span class="u">PPM eCO₂</span></div>
<div class="env">TEMP: <span id="cT">--</span>°C | HUMIDITY: <span id="cR">--</span>%</div>
</div>
<div class="c">
<h2>✓ Room Baseline</h2>
<div class="v rm"><span id="rt">--</span><span class="u">PPB TVOC</span></div>
<div class="v rm" style="margin-top:10px"><span id="rc">--</span><span class="u">PPM eCO₂</span></div>
<div class="env">TEMP: <span id="rT">--</span>°C | HUMIDITY: <span id="rR">--</span>%</div>
</div>
</div>
<div class="cc">
<canvas id="mC" height="300"></canvas>
</div>
<div class="st">LAST SENSOR UPDATE: <span id="lu">--</span> | STATUS: <span style="color:#00ff41">NOMINAL</span></div>
<script>
const $=id=>document.getElementById(id),
fV=v=>(v>=65535||v<0)?null:v,
calcMax=(arr,min)=>{const vals=arr.filter(v=>v!==null);
const m=vals.length?Math.max(...vals):min;return Math.ceil(m*1.2)||min;},
vaultGreen='#00ff41',vaultGreenDim='#00aa30',vaultOrange='#ff6b35',vaultOrangeDim='#cc5528',
quotes=[
"Remember: A clean vault is a happy vault!",
"Vault-Tec: Preparing for the future!",
"Don't worry, that smell is probably nothing!",
"If you can smell it, it's already too late!",
"Fresh air is overrated anyway!",
"Warning: Breathing may contain atmosphere.",
"Your lungs called. They want a vacation.",
"Ah, the sweet smell of... wait, what IS that?",
"Vault-Tec tip: If readings go red, hold breath!",
"Fun fact: Plants also hate this air quality!",
"Questionable Life Choices™ brand air.",
"Remember: Panic is just cardio with steps!",
"When in doubt, blame the ventilation!",
"Pro tip: Can't smell danger if not breathing!",
"Forecast: Mostly breathable, chance of regret.",
"Inhale the good vibes... exhale the VOCs!",
"Masks are just face hugs for your lungs!"
],
mC=new Chart($('mC'),{type:'line',data:{labels:[],datasets:[
{label:'CHAMBER TVOC (PPB)',data:[],borderColor:vaultOrange,backgroundColor:'rgba(255,107,53,0.1)',tension:.3,fill:!1,yAxisID:'y',borderWidth:2,pointRadius:0,pointHoverRadius:4},
{label:'ROOM TVOC (PPB)',data:[],borderColor:vaultGreen,backgroundColor:'rgba(0,255,65,0.1)',tension:.3,fill:!1,yAxisID:'y',borderWidth:2,pointRadius:0,pointHoverRadius:4},
{label:'CHAMBER eCO2 (PPM)',data:[],borderColor:vaultOrangeDim,backgroundColor:'rgba(204,85,40,0.1)',tension:.3,fill:!1,borderDash:[5,5],yAxisID:'y1',borderWidth:2,pointRadius:0,pointHoverRadius:4},
{label:'ROOM eCO2 (PPM)',data:[],borderColor:vaultGreenDim,backgroundColor:'rgba(0,170,48,0.1)',tension:.3,fill:!1,borderDash:[5,5],yAxisID:'y1',borderWidth:2,pointRadius:0,pointHoverRadius:4}]},
options:{responsive:!0,maintainAspectRatio:!1,interaction:{mode:'index',intersect:!1},
scales:{x:{grid:{color:'#1a3a1a'},ticks:{color:vaultGreenDim,font:{family:'Share Tech Mono'}}},
y:{type:'linear',position:'left',grid:{color:'#1a3a1a'},ticks:{color:vaultOrange,font:{family:'Share Tech Mono'}},
title:{display:!0,text:'TVOC (PPB)',color:vaultOrange,font:{family:'Share Tech Mono',weight:'bold'}},beginAtZero:!0,min:0},
y1:{type:'linear',position:'right',grid:{drawOnChartArea:!1},ticks:{color:vaultGreenDim,font:{family:'Share Tech Mono'}},
title:{display:!0,text:'eCO2 (PPM)',color:vaultGreenDim,font:{family:'Share Tech Mono',weight:'bold'}},beginAtZero:!0,min:0}},
plugins:{legend:{labels:{color:vaultGreen,font:{family:'Share Tech Mono'},boxWidth:20,padding:15}},
title:{display:!0,text:'[ ATMOSPHERIC ANALYSIS - 30 MIN HISTORY ]',color:vaultGreen,font:{family:'Share Tech Mono',size:14,weight:'bold'},padding:15},
tooltip:{mode:'index',intersect:!1,backgroundColor:'rgba(13,26,13,0.95)',borderColor:vaultGreen,borderWidth:1,
titleFont:{family:'Share Tech Mono'},bodyFont:{family:'Share Tech Mono'},titleColor:vaultGreen,bodyColor:vaultGreen}}}});
function updateDateTime(){const now=new Date();$('dt').textContent=now.toLocaleDateString()+' '+now.toLocaleTimeString();}
function updateQuote(){$('vbQuote').textContent=quotes[Math.floor(Math.random()*quotes.length)];}
async function fC(){try{const r=await fetch('/api/current'),d=await r.json();
$('ct').textContent=d.ch_tvoc;$('cc').textContent=d.ch_eco2;
$('cT').textContent=d.ch_temp;$('cR').textContent=d.ch_rh;
$('rt').textContent=d.rm_tvoc;$('rc').textContent=d.rm_eco2;
$('rT').textContent=d.rm_temp;$('rR').textContent=d.rm_rh;
$('lu').textContent=new Date().toLocaleTimeString()}catch(e){}}
async function fH(){try{const r=await fetch('/api/data'),d=await r.json(),
l=d.map((_,i)=>`-${d.length-i}m`),
chT=d.map(x=>fV(x.ch_tvoc)),rmT=d.map(x=>fV(x.rm_tvoc)),
chE=d.map(x=>fV(x.ch_eco2)),rmE=d.map(x=>fV(x.rm_eco2));
mC.data.labels=l;
mC.data.datasets[0].data=chT;mC.data.datasets[1].data=rmT;
mC.data.datasets[2].data=chE;mC.data.datasets[3].data=rmE;
mC.options.scales.y.max=calcMax([...chT,...rmT],100);
mC.options.scales.y1.max=calcMax([...chE,...rmE],400);
mC.update()}catch(e){}}
updateDateTime();updateQuote();fC();fH();
setInterval(updateDateTime,1000);setInterval(fC,5000);setInterval(fH,30000);setInterval(updateQuote,30000)
</script></body></html>
)rawliteral";

#endif // WEB_CONTENT_H