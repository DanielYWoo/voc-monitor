#ifndef WEB_CONTENT_H
#define WEB_CONTENT_H

#include <Arduino.h>

// Minified HTML page stored in PROGMEM (~3KB)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>VOC Monitor</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#1a1a2e;color:#eee;padding:20px}
h1{text-align:center;margin-bottom:20px;color:#00d4ff}
.g{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));
gap:20px;margin-bottom:20px}
.c{background:#16213e;border-radius:12px;padding:20px;box-shadow:0 4px 6px rgba(0,0,0,.3)}
.c h2{font-size:1.1em;color:#888;margin-bottom:15px;text-transform:uppercase;letter-spacing:1px}
.v{font-size:2.5em;font-weight:700}.u{font-size:.5em;color:#888}
.ch{color:#ff6b6b}.rm{color:#4ecdc4}
.cc{background:#16213e;border-radius:12px;padding:20px;margin-bottom:20px}
.st{text-align:center;color:#666;font-size:.9em;margin-top:20px}</style></head>
<body><h1>🌬️ VOC Monitor</h1><div class="g"><div class="c"><h2>Chamber</h2>
<div class="v ch"><span id="ct">--</span><span class="u"> ppb TVOC</span></div>
<div class="v ch"><span id="cc">--</span><span class="u"> ppm eCO₂</span></div>
<div style="margin-top:10px;color:#888"><span id="cT">--</span>°C | <span id="cR">--</span>% RH</div>
</div><div class="c"><h2>Room</h2>
<div class="v rm"><span id="rt">--</span><span class="u"> ppb TVOC</span></div>
<div class="v rm"><span id="rc">--</span><span class="u"> ppm eCO₂</span></div>
<div style="margin-top:10px;color:#888"><span id="rT">--</span>°C | <span id="rR">--</span>% RH</div>
</div></div><div class="cc"><canvas id="tC" height="200"></canvas></div>
<div class="cc"><canvas id="eC" height="200"></canvas></div>
<div class="st">Last update: <span id="lu">--</span></div>
<script>
const $=id=>document.getElementById(id),
TVOC_MAX=2000,ECO2_MAX=4000,
fV=v=>(v>=65535||v<0)?null:v,
O={responsive:!0,maintainAspectRatio:!1,
scales:{x:{grid:{color:'#333'},ticks:{color:'#888'}},
y:{grid:{color:'#333'},ticks:{color:'#888'},beginAtZero:!0}},
plugins:{legend:{labels:{color:'#888'}}}},
tC=new Chart($('tC'),{type:'line',data:{labels:[],datasets:[
{label:'Chamber TVOC',data:[],borderColor:'#ff6b6b',tension:.3,fill:!1},
{label:'Room TVOC',data:[],borderColor:'#4ecdc4',tension:.3,fill:!1}]},
options:{...O,scales:{...O.scales,y:{...O.scales.y,max:TVOC_MAX}},
plugins:{...O.plugins,title:{display:!0,text:'TVOC (30 min)',color:'#888'}}}}),
eC=new Chart($('eC'),{type:'line',data:{labels:[],datasets:[
{label:'Chamber eCO2',data:[],borderColor:'#ff6b6b',tension:.3,fill:!1},
{label:'Room eCO2',data:[],borderColor:'#4ecdc4',tension:.3,fill:!1}]},
options:{...O,scales:{...O.scales,y:{...O.scales.y,max:ECO2_MAX}},
plugins:{...O.plugins,title:{display:!0,text:'eCO2 (30 min)',color:'#888'}}}});
async function fC(){try{const r=await fetch('/api/current'),d=await r.json();
$('ct').textContent=d.ch_tvoc;$('cc').textContent=d.ch_eco2;
$('cT').textContent=d.ch_temp;$('cR').textContent=d.ch_rh;
$('rt').textContent=d.rm_tvoc;$('rc').textContent=d.rm_eco2;
$('rT').textContent=d.rm_temp;$('rR').textContent=d.rm_rh;
$('lu').textContent=new Date().toLocaleTimeString()}catch(e){}}
async function fH(){try{const r=await fetch('/api/data'),d=await r.json(),
l=d.map((_,i)=>`-${d.length-i}m`);tC.data.labels=l;
tC.data.datasets[0].data=d.map(x=>fV(x.ch_tvoc));
tC.data.datasets[1].data=d.map(x=>fV(x.rm_tvoc));tC.update();eC.data.labels=l;
eC.data.datasets[0].data=d.map(x=>fV(x.ch_eco2));
eC.data.datasets[1].data=d.map(x=>fV(x.rm_eco2));eC.update()}catch(e){}}
fC();fH();setInterval(fC,5000);setInterval(fH,30000)
</script></body></html>
)rawliteral";

#endif // WEB_CONTENT_H