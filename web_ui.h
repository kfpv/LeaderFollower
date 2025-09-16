// Auto-generated dynamic web UI (data-driven animation controls) — Vivid Controls layout
#pragma once
#if defined(__EMSCRIPTEN__) || defined(_LP64)
  #include "web-sim2/Arduino.h"
#else
  #include <Arduino.h>
#endif
#include "anim_schema.h"

// Served from PROGMEM to reduce RAM usage.
// Endpoints:
//   GET /api/state -> legacy state for convenience (leader/follower subset + globals)
//   POST /api/cfg2  -> JSON { role:0|1, animIndex, params:[{id,value}], globals:[{id,value}] }

// Generate the complete HTML with embedded schema
static const char INDEX_HTML_PREFIX[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1" />
  <title>Leader Controls</title>
  <style>
    :root{--bg:#0f1116;--panel:#151826;--text:#e6e6ef;--muted:#9aa4b2;--accent1:#7c3aed;--accent1-h:#8b5cf6;--accent2:#06b6d4;--outline:#23283b;--focus:#06b6d4;--danger:#ef4444}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
    body{margin:0;font:14px/1.4 system-ui,Segoe UI,Roboto,Helvetica,Arial;background:var(--bg);color:var(--text);-webkit-font-smoothing:antialiased}
    header{padding:14px 18px;border-bottom:1px solid var(--outline);display:flex;align-items:center;gap:12px;background:linear-gradient(180deg,#121525,#0f1116)}
    h1{margin:0;font-size:17px;font-weight:600;letter-spacing:.5px}
    .container{max-width:960px;margin:0 auto;padding:18px;display:grid;gap:18px}
    .card{background:var(--panel);border:1px solid var(--outline);border-radius:14px;padding:16px;box-shadow:0 8px 24px rgba(0,0,0,.28);position:relative}
    .row{display:grid;grid-template-columns:140px 1fr 82px;gap:10px;align-items:center;margin:10px 0}
    .row label{color:var(--muted);font-size:12px;letter-spacing:.5px;text-transform:uppercase}
    input[type=range]{width:100%;margin:0;height:28px;background:transparent}
    input[type=number],select{width:100%;background:#0c0f18;border:1px solid var(--outline);border-radius:9px;color:var(--text);padding:8px 9px;font:13px system-ui}
    input[type=number]:focus,select:focus{outline:2px solid var(--focus);outline-offset:0}
    select{appearance:none;background-image:linear-gradient(45deg,transparent 49%,var(--muted) 50%),linear-gradient(135deg,var(--muted) 49%,transparent 50%);background-position:calc(100% - 14px) 54%,calc(100% - 9px) 54%;background-size:6px 6px; background-repeat:no-repeat;padding-right:30px}
    .twocol{display:grid;grid-template-columns:1fr 1fr;gap:18px}
    .tabs{display:none}
    .tab{flex:1;appearance:none;background:#1b2030;border:1px solid var(--outline);color:var(--muted);padding:10px 6px;font:600 13px system-ui;border-radius:10px;cursor:pointer;transition:.18s background,color}
    .tab.active{background:var(--accent1);color:#fff;border-color:var(--accent1)}
    .btn{appearance:none;border:1px solid var(--accent1);background:var(--accent1);color:#fff;font:600 14px system-ui;padding:11px 18px;border-radius:12px;cursor:pointer;letter-spacing:.5px;min-width:150px;box-shadow:0 4px 14px -2px #000,inset 0 0 0 0 var(--accent1-h);transition:.22s background, .22s transform, .22s box-shadow}
    .btn:hover{background:var(--accent1-h)}
    .btn:active{transform:translateY(1px);box-shadow:0 2px 8px -2px #000}
    .pill{padding:4px 9px;border-radius:999px;font-size:11px;border:1px solid var(--outline);color:var(--muted);display:inline-block;margin-bottom:4px;text-transform:uppercase;letter-spacing:.7px}
    .switch{display:flex;gap:12px;align-items:center}
    .switch input{transform:scale(1.1)}
    #status{min-width:70px;text-align:center}
    .actions{display:flex;gap:14px;align-items:center;flex-wrap:wrap}
    /* Navbar */
    .nav{display:flex;align-items:center;gap:14px;width:100%}
    .navlinks{margin-left:auto;display:flex;gap:8px}
    .navlink{color:var(--muted);text-decoration:none;padding:8px 10px;border-radius:8px;border:1px solid var(--outline);background:#1b2030;font:600 13px system-ui}
    .navlink.active{background:var(--accent1);border-color:var(--accent1);color:#fff}
    .navlink.disabled{opacity:.5;pointer-events:none}
    .hamburger{display:none;background:#1b2030;border:1px solid var(--outline);color:var(--text);border-radius:8px;padding:8px 10px;font-size:16px}
    .toolbar{display:flex;gap:8px;margin-left:8px}
    @media (max-width:700px){
      .hamburger{display:block;margin-left:0}
      .toolbar{margin-left:auto}
      .navlinks{position:absolute;top:54px;right:12px;flex-direction:column;gap:10px;background:var(--panel);border:1px solid var(--outline);border-radius:12px;padding:10px;display:none;z-index:9999}
      .navlinks.open{display:flex}
    }
    /* LED grid */
    .led-grid{display:grid;grid-template-columns:repeat(7,42px);gap:10px;justify-content:center;padding:10px}
    .dot{width:42px;height:42px;border-radius:50%;border:1px solid var(--outline);background:#1a1f2e;color:var(--muted);cursor:pointer;font:600 12px system-ui}
    .dot.on{background:var(--accent2);color:#001018;border-color:var(--accent2)}
    @media (max-width:860px){
      .row{grid-template-columns:120px 1fr 70px}
    }
    @media (max-width:700px){
      .twocol{display:block}
      .tabs{display:flex;gap:10px;margin-top:-4px}
      .twocol .card{display:none;margin-top:14px}
      .twocol .card.active{display:block;animation:fade .25s}
      header{padding:12px 14px}
      .container{padding:14px}
      h1{font-size:16px}
    }
    @keyframes fade{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:translateY(0)}}
    /* Range styling */
    input[type=range]{-webkit-appearance:none}
    input[type=range]::-webkit-slider-runnable-track{height:6px;background:#222a3d;border-radius:4px;border:1px solid #293146}
    input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;margin-top:-6px;height:18px;width:18px;border-radius:50%;background:var(--accent2);border:2px solid #0f1116;box-shadow:0 0 0 3px #0f1116,0 0 0 4px var(--accent2);cursor:pointer;transition:.2s box-shadow,.2s background}
    input[type=range]:focus::-webkit-slider-thumb{background:var(--accent1);box-shadow:0 0 0 3px #0f1116,0 0 0 4px var(--accent1)}
    input[type=range]::-moz-range-track{height:6px;background:#222a3d;border-radius:4px;border:1px solid #293146}
    input[type=range]::-moz-range-thumb{height:18px;width:18px;border-radius:50%;background:var(--accent2);border:2px solid #0f1116;box-shadow:0 0 0 3px #0f1116,0 0 0 4px var(--accent2);cursor:pointer;transition:.2s background,.2s box-shadow}
    input[type=range]:focus::-moz-range-thumb{background:var(--accent1);box-shadow:0 0 0 3px #0f1116,0 0 0 4px var(--accent1)}
  </style>
</head>
<body>
  <header>
    <div class="nav">
      <h1>Vivid Controls</h1>
      <nav class="navlinks">
        <a href="#" data-mode="normal" class="navlink active">Normal</a>
        <a href="#" data-mode="sequential" class="navlink">Sequential</a>
        <a href="#" data-mode="mapping" class="navlink disabled" title="Not implemented">Mapping</a>
      </nav>
  <div class="toolbar">
        <button id="btnImport" title="Import config" class="navlink" aria-label="Import">
          <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-import-icon lucide-import"><path d="M12 3v12"/><path d="m8 11 4 4 4-4"/><path d="M8 5H4a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2h-4"/></svg>
        </button>
        <button id="btnExport" title="Export config" class="navlink" aria-label="Export">
          <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-clipboard-copy-icon lucide-clipboard-copy"><rect width="8" height="4" x="8" y="2" rx="1" ry="1"/><path d="M8 4H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/><path d="M16 4h2a2 2 0 0 1 2 2v4"/><path d="M21 14H11"/><path d="m15 10-4 4 4 4"/></svg>
        </button>
      </div>
      <button class="hamburger" aria-label="menu" aria-expanded="false">☰</button>
    </div>
  </header>
  <div class="container">
    <div class="card" id="globalsCard">
      <div class="pill">Globals</div>
      <div id="G_paramContainer"></div>
    </div>
    <div class="tabs">
      <button class="tab active" data-target="leaderCard">Leader</button>
      <button class="tab" data-target="followerCard">Follower</button>
    </div>
    <div class="twocol">
      <div class="card active" id="leaderCard">
        <div class="pill">Leader</div>
        <div class="mode-normal">
          <div class="row"><label>Animation</label><select id="L_anim"></select><span></span></div>
          <div id="L_paramContainer"></div>
        </div>
        <div class="mode-seq" hidden>
          <div class="led-grid" id="L_ledGrid"></div>
        </div>
      </div>
      <div class="card" id="followerCard">
        <div class="pill">Follower</div>
        <div class="mode-normal">
          <div class="row"><label>Animation</label><select id="F_anim"></select><span></span></div>
          <div id="F_paramContainer"></div>
        </div>
        <div class="mode-seq" hidden>
          <div class="led-grid" id="F_ledGrid"></div>
        </div>
      </div>
    </div>
    <div class="actions"><button class="btn" id="apply">Write Settings</button><span id="status" class="pill">&nbsp;</span></div>
  </div>
  <script>
// Schema generated dynamically from anim_schema.h using X-macros
const SCHEMA_JSON = ')HTML";

// Generate the JSON schema string
static const char ANIM_SCHEMA_JSON[] PROGMEM =
  "{\"params\":["
    PARAM_LIST(PARAM_JSON_COMMA)
    "null],\"animations\":["
    ANIM_ITEMS(ANIM_JSON_COMMA)
    "null]}";

static const char INDEX_HTML_SUFFIX[] PROGMEM = R"HTML(';


function parseSchemaAndFix(jsonText) {
  const data = JSON.parse(jsonText);

  // helper to strip a trailing f/F and convert to number
  const toFloat = v =>
    (typeof v === 'string' && /^-?\d+(?:\.\d+)?f$/i.test(v))
      ? parseFloat(v)
      : v;

  if (Array.isArray(data.params)) {
    data.params.forEach(p => {
      if (p) {
        p.min = toFloat(p.min);
        p.max = toFloat(p.max);
        p.def = toFloat(p.def);
      }
    });
  }
  return data;
}

const SCHEMA = parseSchemaAndFix(SCHEMA_JSON);
const $ = (id)=>document.getElementById(id);

// Navbar mode switching (only links with data-mode act as tabs)
const modeLinks=[...document.querySelectorAll('.navlink[data-mode]')];
const hamburger=document.querySelector('.hamburger');
function setMode(mode){
  modeLinks.forEach(a=>a.classList.toggle('active',a.dataset.mode===mode));
  document.querySelectorAll('.mode-normal').forEach(el=>el.hidden=(mode!=='normal'));
  document.querySelectorAll('.mode-seq').forEach(el=>el.hidden=(mode!=='sequential'));
  const nl=document.querySelector('.navlinks'); nl.classList.remove('open'); hamburger.setAttribute('aria-expanded','false');
}
modeLinks.forEach(a=>!a.classList.contains('disabled')&&a.addEventListener('click',e=>{e.preventDefault();setMode(a.dataset.mode);}));
hamburger.addEventListener('click',()=>{const nl=document.querySelector('.navlinks');const isOpen=nl.classList.toggle('open');hamburger.setAttribute('aria-expanded',String(isOpen));});

// Tabs for narrow screens
function initTabs(){const tabs=[...document.querySelectorAll('.tab')];const leader=$('leaderCard'), follower=$('followerCard');tabs.forEach(t=>t.addEventListener('click',()=>{tabs.forEach(x=>x.classList.remove('active'));t.classList.add('active');[leader,follower].forEach(c=>c.classList.remove('active'));const target=$(t.dataset.target);target.classList.add('active')}));}

let schema=null; const paramMap=new Map();
function normalizeNumber(v){ return typeof v==='number'? v : parseFloat(v); }

function typeKind(t){
  if (typeof t==='number'){
    // 0=bool,1=float,2=int,3=enum (convention)
    return (t===0)?'bool':(t===2?'int':(t===3?'enum':'float'));
  }
  if (typeof t==='string'){
    switch(t){
      case 'PT_BOOL': return 'bool';
      case 'PT_INT': return 'int';
      case 'PT_FLOAT': return 'float';
      case 'PT_ENUM': return 'enum';
      default: return 'float';
    }
  }
  return 'float';
}

async function buildSchema(){
  if (schema) return schema;
  schema = SCHEMA;
  (schema.params||[]).forEach(p=>{ if(!p) return; p.id=+p.id; p.min=normalizeNumber(p.min); p.max=normalizeNumber(p.max); p.def=normalizeNumber(p.def); p._kind=typeKind(p.type); });
  (schema.animations||[]).forEach(a=>{ if(!a) return; a.index=+a.index; a.params=(a.params||[]).map(x=>+x); });
  (schema.params||[]).forEach(p=>{ if(p) paramMap.set(p.id,p); });
  return schema;
}

function linkInputs(rangeEl, numEl){
  if (!rangeEl||!numEl) return;
  const sync=(src,dst)=>{ dst.value=src.value; };
  rangeEl.addEventListener('input',()=>sync(rangeEl,numEl));
  numEl.addEventListener('input',()=>sync(numEl,rangeEl));
}

function createParamControl(pd, prefix){
  const wrap=document.createElement('div'); wrap.className='row param-row'; wrap.dataset.pid=pd.id;
  const label=document.createElement('label'); label.textContent=pd.name; wrap.appendChild(label);
  const kind = pd._kind || typeKind(pd.type);
  if (kind==='bool'){
    const boxWrap=document.createElement('div'); boxWrap.className='switch';
    const input=document.createElement('input'); input.type='checkbox'; input.checked=!!pd.def; input.dataset.pid=pd.id; input.dataset.role='value-input'; input.id=prefix+'p'+pd.id;
    const span=document.createElement('span'); span.textContent='toggle';
    boxWrap.appendChild(input); boxWrap.appendChild(span); wrap.appendChild(boxWrap);
    const pad=document.createElement('span'); pad.textContent=''; wrap.appendChild(pad);
  } else if (kind==='enum'){
    const sel=document.createElement('select'); sel.dataset.pid=pd.id; sel.dataset.role='value-input'; sel.id=prefix+'p'+pd.id; wrap.appendChild(sel);
    const pad=document.createElement('span'); pad.textContent=''; wrap.appendChild(pad);
  } else {
    const range=document.createElement('input'); range.type='range'; range.min=pd.min; range.max=pd.max; range.value=pd.def; range.step=(kind==='int')?'1':(pd.bits&&pd.bits<=8?'0.01':'0.001'); range.dataset.pid=pd.id; range.id=prefix+'p'+pd.id;
    const num=document.createElement('input'); num.type='number'; num.min=pd.min; num.max=pd.max; num.value=pd.def; num.step=range.step; num.dataset.pid=pd.id; num.dataset.role='value-input'; num.id=prefix+'pn'+pd.id;
    linkInputs(range,num);
    wrap.appendChild(range); wrap.appendChild(num);
  }
  return wrap;
}

function buildAnimSelect(selectEl){
  selectEl.innerHTML='';
  (schema.animations||[]).forEach(a=>{ if (!a) return; const o=document.createElement('option'); o.value=String(a.index); o.textContent=a.name; selectEl.appendChild(o); });
}

function buildControlsFor(prefix){
  const sel=$(prefix+'_anim'); const container=$(prefix+'_paramContainer');
  function rebuild(){
    container.innerHTML='';
    const idx=parseInt(sel.value,10);
    const anim=(schema.animations||[]).find(a=>a&&a.index===idx);
    if (!anim) return;
    (anim.params||[]).forEach(pid=>{ const pd=paramMap.get(pid); if(pd) container.appendChild(createParamControl(pd,prefix)); });
  }
  sel.onchange=rebuild; rebuild();
}

function buildGlobals(){
  const gc=$('G_paramContainer'); if(!gc) return; gc.innerHTML=''; if(!schema) return;
  // Only these three are globals, in this order
  const order=["globalspeed","globalmin","globalmax"];
  const byName=new Map();
  (schema.params||[]).forEach(p=>{ if(!p) return; byName.set(String(p.name).toLowerCase(), p); });
  order.forEach(key=>{ const pd=byName.get(key); if(pd) gc.appendChild(createParamControl(pd,'G_')); });
}

function gather(prefix){
  const selectEl=$(prefix+'_anim'); const container=$(prefix+'_paramContainer');
  const animIndex=selectEl?parseInt(selectEl.value,10):0; const params=[];
  container.querySelectorAll('[data-role="value-input"]').forEach(el=>{
    const pid=parseInt(el.dataset.pid,10); let v;
    if (el.type==='checkbox') v=el.checked?1:0; else v=parseFloat(el.value);
    params.push({id:pid,value:v});
  });
  return {animIndex, params};
}

function gatherGlobals(){
  const params=[]; document.querySelectorAll('#G_paramContainer [data-role="value-input"]').forEach(el=>{
    const pid=parseInt(el.dataset.pid,10); let v=(el.type==='checkbox')?(el.checked?1:0):parseFloat(el.value); if(!Number.isNaN(v)) params.push({id:pid,value:v});
  }); return params; }

async function send(role, animIndex, params, globals){
  try{
    const body=JSON.stringify({role,animIndex,params,globals});
    await fetch('/api/cfg2',{method:'POST',headers:{'Content-Type':'application/json'},body});
  }catch(e){console.error('send failed',e);} }

// ----- Import / Export -----
function buildExportConfig(){
  const L=gather('L'); const F=gather('F');
  // Only include globalSpeed from globals
  let globalSpeedVal=null;
  // Find element in globals with name globalSpeed
  const gsRow=[...document.querySelectorAll('#G_paramContainer .param-row')].find(r=>{
    const pid=parseInt(r.dataset.pid||'-1',10); const pd=paramMap.get(pid); return pd && String(pd.name).toLowerCase()==='globalspeed';
  });
  if (gsRow){
    const input=gsRow.querySelector('[data-role="value-input"]');
    if (input) globalSpeedVal = input.type==='checkbox'? (input.checked?1:0) : parseFloat(input.value);
  }
  const cfg={ v:1, globals:{ globalSpeed: (globalSpeedVal??1.0) }, leader:{ animIndex:L.animIndex, params:L.params }, follower:{ animIndex:F.animIndex, params:F.params } };
  return cfg;
}

function validateConfig(cfg){
  if(!cfg || typeof cfg!=='object') return 'not an object';
  if(!cfg.leader || !cfg.follower) return 'missing leader/follower';
  const checkSide=(s)=> typeof s.animIndex==='number' && Array.isArray(s.params) && s.params.every(p=>p&&typeof p.id==='number'&&typeof p.value==='number');
  if(!checkSide(cfg.leader) || !checkSide(cfg.follower)) return 'bad side payload';
  if(cfg.globals && typeof cfg.globals.globalSpeed!=='number') return 'bad globals';
  return null;
}

async function copyToClipboard(text){
  try{ await navigator.clipboard.writeText(text); return true; }catch{ return false; }
}
async function readFromClipboard(){
  try{ const t=await navigator.clipboard.readText(); return t; }catch{ return null; }
}

function applyImportedConfig(cfg){
  // Set globals (only globalSpeed)
  if (cfg.globals && typeof cfg.globals.globalSpeed==='number'){
    const gsRow=[...document.querySelectorAll('#G_paramContainer .param-row')].find(r=>{ const pid=parseInt(r.dataset.pid||'-1',10); const pd=paramMap.get(pid); return pd && String(pd.name).toLowerCase()==='globalspeed'; });
    if (gsRow){ const inp=gsRow.querySelector('[data-role="value-input"]'); if(inp){ inp.value=String(cfg.globals.globalSpeed); const range=gsRow.querySelector('input[type=range]'); if(range) range.value=inp.value; } }
  }
  // Leader/follower
  const sides=[['L', cfg.leader], ['F', cfg.follower]];
  sides.forEach(([prefix, side])=>{
    const sel=$(prefix+'_anim'); if(!sel) return; sel.value=String(side.animIndex); buildControlsFor(prefix);
    const cont=$(prefix+'_paramContainer');
    side.params.forEach(p=>{ const row=[...cont.querySelectorAll('.param-row')].find(r=>parseInt(r.dataset.pid,10)===p.id); if(row){ const inp=row.querySelector('[data-role="value-input"]'); if(inp){ if(inp.type==='checkbox') inp.checked = !!p.value; else inp.value=String(p.value); const range=row.querySelector('input[type=range]'); if(range && inp.type==='number') range.value=inp.value; } } });
  });
}

async function doExport(){
  const cfg=buildExportConfig(); const text=JSON.stringify(cfg);
  const ok=await copyToClipboard(text);
  if(!ok){ window.prompt('Copy config JSON:', text); }
  const st=$('status'); if(st){ st.textContent='exported'; setTimeout(()=>st.textContent='\u00a0',1200); }
}

async function doImport(){
  let text=await readFromClipboard();
  if(!text){ text = window.prompt('Paste config JSON:') || ''; }
  if(!text) return;
  let cfg=null; try{ cfg=JSON.parse(text); }catch{ alert('Invalid JSON'); return; }
  const err=validateConfig(cfg); if(err){ alert('Invalid config: '+err); return; }
  applyImportedConfig(cfg);
  const st=$('status'); if(st){ st.textContent='imported'; setTimeout(()=>st.textContent='\u00a0',1200); }
}

function findSingleAnimIndex(){
  const byName=(schema.animations||[]).find(a=>a&&String(a.name).toLowerCase().includes('single'));
  if (byName) return byName.index;
  const idx4=(schema.animations||[]).find(a=>a&&a.index===4);
  return idx4?idx4.index:(schema.animations&&schema.animations[0]?schema.animations[0].index:0);
}

function setParamValueInContainer(containerId, predicate, value){
  const cont=$(containerId); if(!cont) return;
  const rows=[...cont.querySelectorAll('.param-row')];
  for(const r of rows){
    const pid=parseInt(r.dataset.pid,10); const pd=paramMap.get(pid);
    if (pd && predicate(pd)){
      const input=r.querySelector('[data-role="value-input"]')||r.querySelector('input[type=number]')||r.querySelector('input[type=checkbox]');
      if (!input) continue; if (input.type==='checkbox') input.checked=!!value; else input.value=String(value);
      const range=r.querySelector('input[type=range]'); if (range && input.type==='number') range.value=input.value;
      break;
    }
  }
}

function applySequentialSelection(prefix, selectedIndex){
  const sel=$(prefix+'_anim'); const singleIdx=findSingleAnimIndex(); sel.value=String(singleIdx);
  // rebuild params for single anim and set the likely index param
  buildControlsFor(prefix);
  const containerId=prefix+'_paramContainer';
  // Heuristic: pick an INT param whose name mentions 'single' or 'index', else first INT
  const idx=parseInt(sel.value,10); const anim=(schema.animations||[]).find(a=>a&&a.index===idx);
  let targetPid=null;
  if (anim){
    const intPids=(anim.params||[]).filter(pid=>{const pd=paramMap.get(pid); return pd && (pd._kind==='int' || typeKind(pd.type)==='int');});
    targetPid = intPids.find(pid=>{const n=String(paramMap.get(pid).name||'').toLowerCase(); return n.includes('single')||n.includes('index');}) || intPids[0];
  }
  if (targetPid!=null){
    setParamValueInContainer(containerId, pd=>pd.id===targetPid, selectedIndex);
    // Immediately send CFG2 for this side with current globals
    const globals=gatherGlobals();
    const params=[]; const cont=$(containerId);
    const row=[...cont.querySelectorAll('.param-row')].find(r=>parseInt(r.dataset.pid,10)===targetPid);
    if (row){ const inp=row.querySelector('[data-role="value-input"]'); if (inp){ const val=(inp.type==='checkbox')?(inp.checked?1:0):parseFloat(inp.value); params.push({id:targetPid,value:val}); } }
    // Also include other params in the form (best-effort)
    cont.querySelectorAll('[data-role="value-input"]').forEach(el=>{
      const pid=parseInt(el.dataset.pid,10); if(pid===targetPid) return; let v=(el.type==='checkbox')?(el.checked?1:0):parseFloat(el.value); if(!Number.isNaN(v)) params.push({id:pid,value:v});
    });
    // Determine role: prefix 'L' -> 0, 'F' -> 1
    const role = (prefix==='L')?0:1;
    send(role, singleIdx, params, globals);
  }
}

function buildGrids(){
  function buildGrid(id,onClick){const grid=$(id);grid.innerHTML='';for(let i=0;i<28;i++){const b=document.createElement('button');b.type='button';b.className='dot';b.textContent=String(i);b.dataset.idx=String(i);b.addEventListener('click',()=>onClick(i,b));grid.appendChild(b);} }
  function selectExclusive(gridId, idx){const grid=$(gridId);[...grid.querySelectorAll('.dot')].forEach(el=>{const on=Number(el.dataset.idx)===idx;el.classList.toggle('on',on);});}
  let Lsel=-1, Fsel=-1;
  buildGrid('L_ledGrid',(i)=>{Lsel=(Lsel===i)?-1:i; selectExclusive('L_ledGrid',Lsel); if(Lsel>=0){ applySequentialSelection('L', Lsel);} });
  buildGrid('F_ledGrid',(i)=>{Fsel=(Fsel===i)?-1:i; selectExclusive('F_ledGrid',Fsel); if(Fsel>=0){ applySequentialSelection('F', Fsel);} });
}

async function loadState(){
  try{
    const r=await fetch('/api/state'); if(!r.ok) return; const s=await r.json();
    // Globals by name if possible
    const setGlobalByName=(name, val)=>{
      setParamValueInContainer('G_paramContainer', pd=>String(pd.name).toLowerCase().includes(name), val);
    };
    setGlobalByName('global speed', s.globalSpeed);
    setGlobalByName('global min', s.globalMin);
    setGlobalByName('global max', s.globalMax);
    // Leader
    $('L_anim').value=String(s.leader.animIndex); buildControlsFor('L');
    setParamValueInContainer('L_paramContainer', pd=>/speed/i.test(pd.name), s.leader.speed);
    setParamValueInContainer('L_paramContainer', pd=>/phase/i.test(pd.name), s.leader.phase);
    setParamValueInContainer('L_paramContainer', pd=>/width|single/i.test(pd.name), s.leader.width);
    setParamValueInContainer('L_paramContainer', pd=>/branch/i.test(pd.name), s.leader.branchMode?1:0);
    setParamValueInContainer('L_paramContainer', pd=>/invert/i.test(pd.name), s.leader.invert?1:0);
    // Follower
    $('F_anim').value=String(s.follower.animIndex); buildControlsFor('F');
    setParamValueInContainer('F_paramContainer', pd=>/speed/i.test(pd.name), s.follower.speed);
    setParamValueInContainer('F_paramContainer', pd=>/phase/i.test(pd.name), s.follower.phase);
    setParamValueInContainer('F_paramContainer', pd=>/width|single/i.test(pd.name), s.follower.width);
    setParamValueInContainer('F_paramContainer', pd=>/branch/i.test(pd.name), s.follower.branchMode?1:0);
    setParamValueInContainer('F_paramContainer', pd=>/invert/i.test(pd.name), s.follower.invert?1:0);
  }catch(e){console.warn('state load failed',e);}
}

async function init(){
  await buildSchema();
  buildAnimSelect($('L_anim')); buildAnimSelect($('F_anim'));
  buildControlsFor('L'); buildControlsFor('F'); buildGlobals();
  initTabs(); buildGrids(); setMode('normal');
  // Wire Import / Export action buttons
  const btnImport=$('btnImport'); if(btnImport){ btnImport.addEventListener('click', (e)=>{ e.preventDefault(); doImport(); }); }
  const btnExport=$('btnExport'); if(btnExport){ btnExport.addEventListener('click', (e)=>{ e.preventDefault(); doExport(); }); }
  $('apply').addEventListener('click', async ()=>{
    try {
      const g=gatherGlobals(); const L=gather('L'); const F=gather('F');
      console.log('Applying CFG2', {globals:g, leader:L, follower:F});
      // Send leader then follower
      await send(0,L.animIndex,L.params,g);
      await new Promise(r=>setTimeout(r,40));
      await send(1,F.animIndex,F.params,g);
      const st=$('status'); st.textContent='applied'; setTimeout(()=>st.textContent='\u00a0',1600);
    } catch(err){ console.error(err); const st=$('status'); st.textContent='error'; st.style.background='#ef4444'; }
  });
  loadState();
}
init();
  </script>
</body>
</html>
)HTML";

