#pragma once
// Web control panel served by the ESP32 at http://192.168.4.1 (or http://eyes.local)
// Uses only system fonts so it works with no internet connection.

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Eye control</title>
<style>
:root{
  --bg:#1d2233; --panel:#262d43; --line:#343c57;
  --text:#eef0f6; --muted:#9aa3bd;
  --sclera:#f3efe6; --iris:#4a8fe0; --iris-in:#9fd0f5; --amber:#f2b441;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);
  font:16px/1.45 ui-rounded,"SF Pro Rounded","Segoe UI",system-ui,sans-serif}
header{display:flex;align-items:baseline;justify-content:space-between;gap:1rem;
  padding:1.1rem 1.4rem .4rem;max-width:980px;margin:auto}
h1{margin:0;font-size:1.7rem;font-weight:700;letter-spacing:-.01em}
#conn{font-size:.9rem;color:var(--muted)}
#conn.ok::before{content:"";display:inline-block;width:.55rem;height:.55rem;border-radius:50%;
  background:#6bd68a;margin-right:.4rem}
#conn.bad{color:#ff8a7a}
main{display:grid;grid-template-columns:minmax(260px,380px) 1fr;gap:1.6rem;
  max-width:980px;margin:auto;padding:1rem 1.4rem 2rem}
@media(max-width:760px){main{grid-template-columns:1fr}}

/* The look pad is drawn as an eyeball: drag the iris to aim the eyes */
.padwrap{display:flex;flex-direction:column;align-items:center;gap:.7rem}
#pad{position:relative;width:min(100%,360px);aspect-ratio:1;border-radius:50%;
  background:radial-gradient(circle at 50% 50%,var(--sclera) 62%,#cfc9bd 100%);
  touch-action:none;cursor:grab;box-shadow:inset 0 -10px 30px rgba(0,0,0,.25)}
#pad:active{cursor:grabbing}
#knob{position:absolute;left:50%;top:50%;width:34%;aspect-ratio:1;border-radius:50%;
  transform:translate(-50%,-50%);
  background:radial-gradient(circle,#05070d 0 32%,var(--iris-in) 34%,var(--iris) 78%,#1b3d6e 100%);
  box-shadow:0 0 0 3px rgba(20,40,80,.35);pointer-events:none}
#knob::after{content:"";position:absolute;left:24%;top:20%;width:16%;height:16%;
  border-radius:50%;background:#fff}
#gaze{position:absolute;left:50%;top:50%;width:12%;aspect-ratio:1;border-radius:50%;
  border:2px dashed var(--amber);transform:translate(-50%,-50%);pointer-events:none;opacity:.9}
.hint{color:var(--muted);font-size:.9rem;text-align:center;margin:0}

section{margin-bottom:1.4rem}
h2{font-size:1.05rem;font-weight:600;margin:0 0 .6rem}
.chips{display:flex;flex-wrap:wrap;gap:.5rem}
button{font:inherit;color:var(--text);background:var(--panel);border:1px solid var(--line);
  border-radius:999px;padding:.5rem 1rem;cursor:pointer}
button:hover{border-color:var(--muted)}
button:focus-visible,input:focus-visible{outline:3px solid var(--amber);outline-offset:2px}
button.on{background:var(--amber);border-color:var(--amber);color:#2a1d00;font-weight:600}
.row{display:flex;align-items:center;justify-content:space-between;gap:1rem;
  padding:.55rem 0;border-bottom:1px solid var(--line)}
.row:last-child{border-bottom:0}
input[type=range]{width:55%;accent-color:var(--amber)}
.switch{appearance:none;width:2.8rem;height:1.6rem;border-radius:1rem;background:var(--line);
  position:relative;cursor:pointer;transition:background .15s}
.switch::after{content:"";position:absolute;top:.2rem;left:.2rem;width:1.2rem;height:1.2rem;
  border-radius:50%;background:#fff;transition:left .15s}
.switch:checked{background:var(--amber)}
.switch:checked::after{left:1.4rem}
#stats{color:var(--muted);font-size:.85rem;display:grid;grid-template-columns:auto 1fr;gap:.15rem 1rem}
@media(prefers-reduced-motion:reduce){.switch,.switch::after{transition:none}}
</style>
</head>
<body>
<header>
  <h1>Eye control</h1>
  <span id="conn">Connecting…</span>
</header>

<main>
  <div class="padwrap">
    <div id="pad" role="application" aria-label="Drag to aim the eyes">
      <div id="gaze"></div><div id="knob"></div>
    </div>
    <p class="hint">Drag the iris to aim the eyes. The dashed ring shows where they are looking. Arrow keys and space bar work too.</p>
  </div>

  <div>
    <section>
      <h2>Eye style</h2>
      <div class="chips" id="styles"></div>
    </section>

    <section>
      <h2>Expression</h2>
      <div class="chips" id="exprs"></div>
    </section>

    <section>
      <h2>Actions</h2>
      <div class="chips">
        <button onclick="api('blink?eye=both')">Blink</button>
        <button onclick="api('blink?eye=left')">Wink left</button>
        <button onclick="api('blink?eye=right')">Wink right</button>
        <button onclick="api('release')">Look ahead</button>
      </div>
    </section>

    <section>
      <h2>Settings</h2>
      <div class="row"><label for="auto">Move eyes automatically</label>
        <input type="checkbox" class="switch" id="auto" onchange="api('auto?move='+(this.checked?1:0))"></div>
      <div class="row"><label for="ablink">Blink automatically</label>
        <input type="checkbox" class="switch" id="ablink" onchange="api('auto?blink='+(this.checked?1:0))"></div>
      <div class="row"><label for="snap">Return to auto when I let go</label>
        <input type="checkbox" class="switch" id="snap" checked></div>
      <div class="row"><label for="pupil">Pupil size <span id="pv"></span></label>
        <input type="range" id="pupil" min="30" max="200" value="100"></div>
      <div class="row"><span>Joystick drifting?</span>
        <button onclick="api('calibrate')">Re-centre joystick</button></div>
    </section>

    <section>
      <h2>Status</h2>
      <div id="stats"></div>
    </section>
  </div>
</main>

<script>
const $=id=>document.getElementById(id);
const pad=$('pad'), knob=$('knob'), gaze=$('gaze');
let dragging=false, lastSend=0, pend=null, tmr=null, last=[0,0], busy=false, pupilDrag=false, kx=0, ky=0;

function setConn(ok){const c=$('conn');c.className=ok?'ok':'bad';c.textContent=ok?'Connected':'No connection – check WiFi';}

function api(p){
  return fetch('/api/'+p).then(r=>{setConn(true);return r.status===200?r.json():null})
    .then(j=>{if(j)render(j)}).catch(()=>setConn(false));
}

function chips(el,list,active,path){
  if(el.childElementCount!==list.length){
    el.innerHTML='';
    list.forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;
      b.onclick=()=>api(path+'?i='+i);el.appendChild(b);});
  }
  [...el.children].forEach((b,i)=>{b.classList.toggle('on',i===active);b.setAttribute('aria-pressed',i===active)});
}

function place(el,x,y){el.style.left=(50+x*33)+'%';el.style.top=(50+y*33)+'%';}

function render(s){
  chips($('styles'),s.styles,s.style,'style');
  chips($('exprs'),s.exprs,s.expr,'expr');
  $('auto').checked=s.auto; $('ablink').checked=s.autoBlink;
  if(!pupilDrag){$('pupil').value=Math.round(s.pupil*100);$('pv').textContent=Math.round(s.pupil*100)+'%';}
  place(gaze,s.x,s.y);
  if(!dragging) place(knob,s.x,s.y);
  const src={auto:'Automatic',hold:'Holding still',joystick:'Joystick',web:'This page',serial:'Serial monitor'}[s.src]||s.src;
  $('stats').innerHTML=
    `<span>Controlled by</span><span>${src}</span>`+
    `<span>Frame rate</span><span>${s.fps} fps</span>`+
    `<span>Joystick</span><span>${s.joy[0]}, ${s.joy[1]}</span>`+
    `<span>Access point</span><span>http://${s.ap}</span>`+
    (s.sta?`<span>Home WiFi</span><span>http://${s.sta}</span>`:'')+
    `<span>Free memory</span><span>${Math.round(s.heap/1024)} KB</span>`;
}

/* ---- look pad ---- */
function flush(){
  tmr=null; if(!pend) return;
  lastSend=performance.now(); const [x,y]=pend; pend=null;
  fetch(`/api/look?x=${x.toFixed(3)}&y=${y.toFixed(3)}`).catch(()=>setConn(false));
}
function queueLook(x,y){
  pend=[x,y]; last=[x,y];
  const wait=70-(performance.now()-lastSend);
  if(wait<=0) flush(); else if(!tmr) tmr=setTimeout(flush,wait);
}
function fromEvent(e){
  const r=pad.getBoundingClientRect();
  let x=((e.clientX-r.left)/r.width*2-1)/0.66, y=((e.clientY-r.top)/r.height*2-1)/0.66;
  const m=Math.hypot(x,y); if(m>1){x/=m;y/=m;}
  place(knob,x,y); queueLook(x,y);
}
pad.addEventListener('pointerdown',e=>{dragging=true;pad.setPointerCapture(e.pointerId);fromEvent(e)});
pad.addEventListener('pointermove',e=>{if(dragging)fromEvent(e)});
['pointerup','pointercancel'].forEach(ev=>pad.addEventListener(ev,()=>{
  if(!dragging) return; dragging=false;
  if($('snap').checked){place(knob,0,0);api('release');}
}));
setInterval(()=>{if(dragging)queueLook(...last)},1000);   // keep control while holding still

/* ---- keyboard ---- */
document.addEventListener('keydown',e=>{
  if(e.target.tagName==='INPUT'||e.target.tagName==='BUTTON') return;
  const step=0.25, k=e.key;
  if(k===' '){api('blink?eye=both');e.preventDefault();return;}
  if(k==='ArrowLeft')kx-=step; else if(k==='ArrowRight')kx+=step;
  else if(k==='ArrowUp')ky-=step; else if(k==='ArrowDown')ky+=step; else return;
  e.preventDefault();
  kx=Math.max(-1,Math.min(1,kx)); ky=Math.max(-1,Math.min(1,ky));
  place(knob,kx,ky); queueLook(kx,ky);
});

/* ---- pupil slider ---- */
const ps=$('pupil');
ps.addEventListener('input',()=>{pupilDrag=true;$('pv').textContent=ps.value+'%';});
ps.addEventListener('change',()=>{pupilDrag=false;api('pupil?v='+(ps.value/100));});

/* ---- status polling ---- */
function poll(){ if(busy) return; busy=true; api('status').finally(()=>busy=false); }
poll(); setInterval(poll,400);
</script>
</body>
</html>
)rawliteral";
