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
    /* Favorites modal */
    .modal{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,.45);z-index:10000}
    .modal[hidden]{display:none}
    .modal-panel{background:var(--panel);border:1px solid var(--outline);border-radius:12px;min-width:280px;max-width:560px;width:92%;max-height:70vh;overflow:auto;box-shadow:0 10px 28px rgba(0,0,0,.5);padding:14px}
    .modal-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
    .fav-list{display:flex;flex-direction:column;gap:8px}
    .fav-row{display:flex;align-items:center;justify-content:space-between;border:1px solid var(--outline);background:#1b2030;border-radius:10px;padding:10px}
    .fav-name{font:600 13px system-ui;color:var(--text)}
    .fav-details{border:1px dashed var(--outline);border-radius:10px;padding:10px;margin:6px 0 12px;background:#12172a}
    .fav-actions{display:flex;gap:8px;margin-top:8px}
    /* Loading overlay */
    #loadingOverlay{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,.45);z-index:20000}
    #loadingOverlay .box{display:flex;flex-direction:column;align-items:center;gap:10px}
    .spinner{width:40px;height:40px;border-radius:50%;border:3px solid #2a3147;border-top-color:var(--accent2);animation:spin 1s linear infinite}
    .loading-text{color:var(--muted)}
    @keyframes spin{to{transform:rotate(360deg)}}
  </style>
</head>
<body>
  <header>
    <div id="loadingOverlay"><div class="box"><div class="spinner"></div><div class="loading-text">Loading…</div></div></div>
    <div class="nav">
      <h1>Vivid Controls</h1>
       <nav class="navlinks">
         <a href="#" data-mode="normal" class="navlink active">Normal</a>
         <a href="#" data-mode="sequential" class="navlink">Sequential</a>
          <a href="#" data-mode="auto" class="navlink">Auto</a>
       </nav>
   <div class="toolbar">
         <button id="btnImport" title="Import config" class="navlink" aria-label="Import">
           <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-import-icon lucide-import"><path d="M12 3v12"/><path d="m8 11 4 4 4-4"/><path d="M8 5H4a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V7a2 2 0 0 0-2-2h-4"/></svg>
         </button>
         <button id="btnExport" title="Export config" class="navlink" aria-label="Export">
           <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-clipboard-copy-icon lucide-clipboard-copy"><rect width="8" height="4" x="8" y="2" rx="1" ry="1"/><path d="M8 4H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/><path d="M16 4h2a2 2 0 0 1 2 2v4"/><path d="M21 14H11"/><path d="m15 10-4 4 4 4"/></svg>
         </button>
         <button id="btnFavAdd" title="Add favorite" class="navlink" aria-label="Add favorite" disabled>
           <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-star-icon lucide-star"><path d="M11.525 2.295a.53.53 0 0 1 .95 0l2.31 4.679a2.123 2.123 0 0 0 1.595 1.16l5.166.756a.53.53 0 0 1 .294.904l-3.736 3.638a2.123 2.123 0 0 0-.611 1.878l.882 5.14a.53.53 0 0 1-.771.56l-4.618-2.428a2.122 2.122 0 0 0-1.973 0L6.396 21.01a.53.53 0 0 1-.77-.56l.881-5.139a2.122 2.122 0 0 0-.611-1.879L2.16 9.795a.53.53 0 0 1 .294-.906l5.165-.755a2.122 2.122 0 0 0 1.597-1.16z"/></svg>
         </button>
         <button id="btnFavOpen" title="Open favorites" class="navlink" aria-label="Open favorites" disabled>
           <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-user-star-icon lucide-user-star"><path d="M16.051 12.616a1 1 0 0 1 1.909.024l.737 1.452a1 1 0 0 0 .737.535l1.634.256a1 1 0 0 1 .588 1.806l-1.172 1.168a1 1 0 0 0-.282.866l.259 1.613a1 1 0 0 1-1.541 1.134l-1.465-.75a1 1 0 0 0-.912 0l-1.465.75a1 1 0 0 1-1.539-1.133l.258-1.613a1 1 0 0 0-.282-.866l-1.156-1.153a1 1 0 0 1 .572-1.822l1.633-.256a1 1 0 0 0 .737-.535z"/><path d="M8 15H7a4 4 0 0 0-4 4v2"/><circle cx="10" cy="7" r="4"/></svg>
         </button>
       </div>
      <button class="hamburger" aria-label="menu" aria-expanded="false">☰</button>
    </div>
  </header>
   <div class="container">
    <div id="favModal" class="modal" hidden>
      <div class="modal-panel">
        <div class="modal-header"><div class="pill">Favorites</div><button id="favClose" class="navlink">Close</button></div>
        <div id="favList" class="fav-list"></div>
      </div>
    </div>
    <div class="card" id="globalsCard">
      <div class="pill">Globals</div>
      <div id="G_paramContainer"></div>
    </div>
     <div class="tabs">
       <button class="tab active" data-target="leaderCard">Leader</button>
       <button class="tab" data-target="followerCard">Follower</button>
     </div>
      <div class="card mode-auto" id="autoCard" hidden style="grid-column:1 / -1">
        <div class="pill">Auto</div>
        <div class="row"><label>Interval (min)</label><input id="autoInterval" type="number" min="1" step="1" value="1"/><span></span></div>
        <div class="row"><label>Randomize</label><div class="switch"><input id="autoRandomize" type="checkbox"/><span>shuffle</span></div><span></span></div>
        <div class="autoFavSection" style="display:flex;flex-direction:column;gap:8px;margin:10px 0">
          <div class="pill">Favorites</div>
          <label style="display:flex;align-items:center;gap:8px">
            <input id="autoSelAll" type="checkbox"/> Select All
          </label>
          <div id="autoFavList" class="fav-list"></div>
        </div>
        <div class="actions">
          <button class="btn" id="autoStart">Start</button>
          <button class="navlink" id="autoStop">Stop</button>
          <span id="autoStatus" class="pill">&nbsp;</span>
        </div>
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
  const autoCard=document.getElementById('autoCard'); if (autoCard) autoCard.hidden = (mode!=='auto');
  // Hide/show leader/follower/tabs based on mode
  const twoCol=document.querySelector('.twocol'); if(twoCol) twoCol.style.display = (mode==='auto') ? 'none' : '';
  const tabsEl=document.querySelector('.tabs'); if(tabsEl) tabsEl.style.display = (mode==='auto') ? 'none' : '';
  // Toolbar: Import/Export/Add visible only in normal; Favorites list visible always
  const hideTools=['btnImport','btnExport','btnFavAdd'].map(id=>document.getElementById(id)).filter(Boolean);
  hideTools.forEach(el=>{ el.style.display = (mode==='normal') ? '' : 'none'; });
  const favOpen=document.getElementById('btnFavOpen'); if(favOpen){ favOpen.style.display=''; }
  // Hide Write Settings button in Auto mode
  const applyBtn=document.getElementById('apply'); if(applyBtn){ applyBtn.style.display = (mode==='auto') ? 'none' : ''; }
  // When entering auto mode, refresh config/status
  if (mode==='auto') {
    ensureFavoritesLoaded().then(()=>loadAutoConfig()).then(()=>startAutoStatusPolling()).catch(()=>{});
  } else {
    stopAutoStatusPolling();
  }
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

// ----- Favorites -----
let FAVS=[]; let FAVS_LOADED=false;
function getAnimName(index){ const a=(SCHEMA.animations||[]).find(x=>x&&x.index===Number(index)); return a? a.name : ('#'+String(index)); }
function getParamNameById(pid){ const pd=paramMap.get(Number(pid)); return pd? pd.name : ('pid '+String(pid)); }
function fmtVal(v){ if(typeof v==='number'){ const s=v.toFixed(3); return s.replace(/\.0+$/,'').replace(/(\.[1-9]*)0+$/,'$1'); } return String(v); }
function renderFavDetails(cfg, item, readOnly){ const wrap=document.createElement('div'); wrap.className='fav-details';
  const secG=document.createElement('div'); secG.innerHTML='<div class="pill">Globals</div>';
  const ulG=document.createElement('ul'); ulG.style.margin='8px 0'; ulG.style.paddingLeft='18px'; ulG.style.color='var(--muted)';
  if(cfg && cfg.globals && typeof cfg.globals.globalSpeed==='number'){ const li=document.createElement('li'); li.textContent='globalSpeed: '+fmtVal(cfg.globals.globalSpeed); ulG.appendChild(li); }
  secG.appendChild(ulG); wrap.appendChild(secG);
  const addSide=(label, side)=>{ const sec=document.createElement('div'); sec.innerHTML='<div class="pill">'+label+'</div>'; const meta=document.createElement('div'); meta.style.margin='6px 0'; meta.style.color='var(--muted)'; meta.textContent='Anim: '+getAnimName(side&&side.animIndex)+' (index '+(side&&side.animIndex)+')'; sec.appendChild(meta); const ul=document.createElement('ul'); ul.style.margin='4px 0 10px'; ul.style.paddingLeft='18px'; ul.style.color='var(--muted)';
    const params=side&&Array.isArray(side.params)? side.params : []; params.forEach(p=>{ if(p&&typeof p.id==='number'){ const li=document.createElement('li'); li.textContent=getParamNameById(p.id)+': '+fmtVal(p.value); ul.appendChild(li);} });
    if(!params.length){ const li=document.createElement('li'); li.textContent='(no params)'; ul.appendChild(li); }
    sec.appendChild(ul); wrap.appendChild(sec); };
  addSide('Leader', cfg&&cfg.leader?cfg.leader:{});
  addSide('Follower', cfg&&cfg.follower?cfg.follower:{});
  if (!readOnly){
    const actions=document.createElement('div'); actions.className='fav-actions';
    const btnLoad=document.createElement('button'); btnLoad.type='button'; btnLoad.className='navlink'; btnLoad.innerHTML='<span style="display:inline-flex;gap:6px;align-items:center"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-arrow-up-from-line-icon lucide-arrow-up-from-line"><path d="m18 9-6-6-6 6"/><path d="M12 3v14"/><path d="M5 21h14"/></svg>Load</span>';
    btnLoad.addEventListener('click', async ()=>{ if(!item||typeof item.id!== 'number') return; const ok=window.confirm('Load this favorite? This will overwrite current settings.'); if(!ok) return; try{ btnLoad.disabled=true; btnLoad.textContent='Loading…'; await loadFavorite(item); } finally { btnLoad.disabled=false; btnLoad.innerHTML='<span style="display:inline-flex;gap:6px;align-items:center"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-arrow-up-from-line-icon lucide-arrow-up-from-line"><path d="m18 9-6-6-6 6"/><path d="M12 3v14"/><path d="M5 21h14"/></svg>Load</span>'; } });
    actions.appendChild(btnLoad);
    const btnDelete=document.createElement('button'); btnDelete.type='button'; btnDelete.className='navlink'; btnDelete.style.borderColor='var(--danger)'; btnDelete.style.color='var(--danger)'; btnDelete.innerHTML='<span style="display:inline-flex;gap:6px;align-items:center"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-trash2-icon lucide-trash-2"><path d="M10 11v6"/><path d="M14 11v6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"/><path d="M3 6h18"/><path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>Delete</span>';
    btnDelete.addEventListener('click', async ()=>{ if(!item||typeof item.id!== 'number') return; const ok=window.confirm('Delete this favorite?'); if(!ok) return; try{ btnDelete.disabled=true; btnDelete.textContent='Deleting…'; await deleteFavorite(item.id); } finally { btnDelete.disabled=false; btnDelete.innerHTML='<span style="display:inline-flex;gap:6px;align-items:center"><svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-trash2-icon lucide-trash-2"><path d="M10 11v6"/><path d="M14 11v6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"/><path d="M3 6h18"/><path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>Delete</span>'; } });
    actions.appendChild(btnDelete);
    wrap.appendChild(actions);
  }
  return wrap; }
function renderFavList(){ const list=$('favList'); if(!list) return; list.innerHTML=''; if(!Array.isArray(FAVS)||FAVS.length===0){ const d=document.createElement('div'); d.className='fav-row'; d.textContent='No favorites saved yet'; list.appendChild(d); return; }
  FAVS.forEach(it=>{ const row=document.createElement('div'); row.className='fav-row';
    const left=document.createElement('div'); left.style.display='flex'; left.style.flexDirection='column';
    const title=document.createElement('div'); title.className='fav-name'; title.textContent=it&&it.name?String(it.name):'favorite'; left.appendChild(title);
    const subtitle=document.createElement('div'); subtitle.style.color='var(--muted)'; subtitle.style.fontSize='12px';
    const cfg = it && it.cfg ? it.cfg : null; const leaderName = cfg? getAnimName(cfg.leader&&cfg.leader.animIndex) : ''; const followerName = cfg? getAnimName(cfg.follower&&cfg.follower.animIndex) : '';
    subtitle.textContent = 'Leader: '+leaderName+'  •  Follower: '+followerName; left.appendChild(subtitle);
    const actions=document.createElement('div'); actions.style.display='flex'; actions.style.gap='8px';
    const btnDetails=document.createElement('button'); btnDetails.type='button'; btnDetails.className='navlink'; btnDetails.textContent='Details';
    const details = renderFavDetails(cfg||{}, it, false); details.style.display='none'; details.style.marginTop='10px';
    btnDetails.addEventListener('click',()=>{ const open = details.style.display==='none'; details.style.display=open?'block':'none'; btnDetails.textContent=open?'Hide':'Details'; });
    actions.appendChild(btnDetails);
    row.appendChild(left); row.appendChild(actions); list.appendChild(row); list.appendChild(details);
  }); }

// ----- Auto mode (UI+API) -----
let AUTO_CFG={ on:false, interval:10, random:false, selections:[], current:{name:'',id:-1,remaining:0}};
let AUTO_STATUS_TIMER=null;
let AUTO_LOCAL_REMAIN=0;

function renderAutoFavList(){ const list=$('autoFavList'); if(!list) return; list.innerHTML='';
  if(!Array.isArray(FAVS)||FAVS.length===0){ const d=document.createElement('div'); d.className='fav-row'; d.textContent='No favorites saved yet'; list.appendChild(d); return; }
  FAVS.forEach((it)=>{ const row=document.createElement('div'); row.className='fav-row';
    const left=document.createElement('div'); left.style.display='flex'; left.style.flexDirection='column';
    const title=document.createElement('div'); title.className='fav-name'; title.textContent=it&&it.name?String(it.name):'favorite'; left.appendChild(title);
    const subtitle=document.createElement('div'); subtitle.style.color='var(--muted)'; subtitle.style.fontSize='12px';
    const cfg = it && it.cfg ? it.cfg : null; const leaderName = cfg? getAnimName(cfg.leader&&cfg.leader.animIndex) : ''; const followerName = cfg? getAnimName(cfg.follower&&cfg.follower.animIndex) : '';
    subtitle.textContent = 'Leader: '+leaderName+'  •  Follower: '+followerName; left.appendChild(subtitle);
    const right=document.createElement('div'); right.style.display='flex'; right.style.alignItems='center'; right.style.gap='10px';
    const cb=document.createElement('input'); cb.type='checkbox'; cb.checked = AUTO_CFG.selections.includes(it.id);
    cb.addEventListener('change', async ()=>{ const id=it.id; const idx=AUTO_CFG.selections.indexOf(id); if(cb.checked){ if(idx<0) AUTO_CFG.selections.push(id); } else { if(idx>=0) AUTO_CFG.selections.splice(idx,1); } updateSelectAllIndeterminate(); await saveAutoSettings(); if (AUTO_CFG.on){ AUTO_LOCAL_REMAIN = Math.max(1, parseInt(String(AUTO_CFG.interval||1),10)) * 60; updateAutoStatus(); } });
    right.appendChild(cb);
    row.appendChild(left); row.appendChild(right);
    // read-only: Details without load/delete
    const details = renderFavDetails(cfg||{}, it, true); details.style.display='none'; details.style.marginTop='10px';
    const btnDetails=document.createElement('button'); btnDetails.type='button'; btnDetails.className='navlink'; btnDetails.textContent='Details'; btnDetails.style.marginTop='8px'; btnDetails.addEventListener('click',()=>{ const open = details.style.display==='none'; details.style.display=open?'block':'none'; btnDetails.textContent=open?'Hide':'Details'; });
    list.appendChild(row); list.appendChild(btnDetails); list.appendChild(details);
  });
  updateSelectAllIndeterminate();
}

function updateSelectAllIndeterminate(){ const all=$('autoSelAll'); if(!all) return; const total=Array.isArray(FAVS)?FAVS.length:0; const sel=AUTO_CFG.selections.length; all.indeterminate = sel>0 && sel<total; all.checked = total>0 && sel===total; const startBtn=$('autoStart'); if(startBtn){ startBtn.disabled = (sel===0); } }

function readAutoFormIntoCfg(){ const iv=$('autoInterval'); const rand=$('autoRandomize'); AUTO_CFG.interval = Math.max(1, parseInt(iv.value||'1',10)); AUTO_CFG.random = !!rand.checked; }

async function loadAutoConfig(){ try{ const r=await fetch('/api/auto/config'); if(!r.ok) throw new Error('auto cfg get failed'); const data=await r.json();
  AUTO_CFG.on=!!data.on; AUTO_CFG.interval = Math.max(1, parseInt(data.interval||'1',10)); AUTO_CFG.random=!!data.random; AUTO_CFG.selections = Array.isArray(data.selections)? data.selections.slice():[]; AUTO_CFG.current = data.current || {name:'',id:-1,remaining:0};
  AUTO_LOCAL_REMAIN = Math.max(0, parseInt(AUTO_CFG.current.remaining||0,10));
  // Default to all favorites if none selected
  if (AUTO_CFG.selections.length===0 && Array.isArray(FAVS) && FAVS.length>0){
    AUTO_CFG.selections = FAVS.map(it=>it.id);
    await saveAutoSettings();
  }
  $('autoInterval').value=String(AUTO_CFG.interval); $('autoRandomize').checked=AUTO_CFG.random;
  renderAutoFavList(); updateAutoStatus();
 } catch(e){ console.warn('loadAutoConfig failed', e); }
}

async function saveAutoSettings(){ readAutoFormIntoCfg(); const body=JSON.stringify({ interval:AUTO_CFG.interval, random:AUTO_CFG.random, selections:AUTO_CFG.selections });
  try{ const r=await fetch('/api/auto/settings',{method:'POST',headers:{'Content-Type':'application/json'},body}); if(!r.ok) throw new Error('auto save failed'); } catch(e){ console.warn('saveAutoSettings failed', e); }
}

async function startAuto(){ await saveAutoSettings(); try{ const r=await fetch('/api/auto/start',{method:'POST'}); if(!r.ok) throw new Error('auto start failed'); AUTO_CFG.on=true; startAutoStatusPolling(true); } catch(e){ console.warn('startAuto failed', e); } }
async function stopAuto(){ try{ const r=await fetch('/api/auto/stop',{method:'POST'}); if(!r.ok) throw new Error('auto stop failed'); AUTO_CFG.on=false; updateAutoStatus(); stopAutoStatusPolling(); } catch(e){ console.warn('stopAuto failed', e); } }

function updateAutoStatus(){ const pill=$('autoStatus'); if(!pill) return; const startBtn=$('autoStart'); if(startBtn){ const totalFavs = Array.isArray(FAVS)?FAVS.length:0; startBtn.disabled = (totalFavs===0 || AUTO_CFG.selections.length===0); }
  if(!AUTO_CFG.on){ pill.textContent='stopped'; pill.style.borderColor='var(--outline)'; pill.style.color='var(--muted)'; return; } const nm=AUTO_CFG.current&&AUTO_CFG.current.name?AUTO_CFG.current.name:''; const rem=(typeof AUTO_LOCAL_REMAIN==='number')?Math.max(0,Math.floor(AUTO_LOCAL_REMAIN)):0; pill.textContent = (nm? nm+' • ' : '') + rem + 's left'; pill.style.borderColor='var(--accent2)'; pill.style.color='var(--accent2)'; }

async function localAutoTick(){ if(!AUTO_CFG.on){ updateAutoStatus(); return; } if (AUTO_LOCAL_REMAIN>0){ AUTO_LOCAL_REMAIN -= 1; updateAutoStatus(); return; } try{ await loadAutoConfig(); updateAutoStatus(); } catch(_){} }

function startAutoStatusPolling(){ stopAutoStatusPolling(); // seed remaining from current if present
  if (typeof AUTO_LOCAL_REMAIN !== 'number' || AUTO_LOCAL_REMAIN<=0){ AUTO_LOCAL_REMAIN = Math.max(0, parseInt(AUTO_CFG.current&&AUTO_CFG.current.remaining||0,10)); }
  updateAutoStatus(); AUTO_STATUS_TIMER = setInterval(localAutoTick, 1000); }
function stopAutoStatusPolling(){ if (AUTO_STATUS_TIMER){ clearInterval(AUTO_STATUS_TIMER); AUTO_STATUS_TIMER=null; } }


// Wire Auto controls
(function(){ const selAll=$('autoSelAll'); if(selAll){ selAll.addEventListener('change', async ()=>{ if(!Array.isArray(FAVS)) return; if(selAll.checked){ AUTO_CFG.selections = FAVS.map(it=>it.id); } else { AUTO_CFG.selections = []; } renderAutoFavList(); await saveAutoSettings(); if (AUTO_CFG.on){ AUTO_LOCAL_REMAIN = Math.max(1, parseInt(String(AUTO_CFG.interval||1),10)) * 60; updateAutoStatus(); } }); }
  const startBtn=$('autoStart'); if(startBtn) startBtn.addEventListener('click', startAuto);
  const stopBtn=$('autoStop'); if(stopBtn) stopBtn.addEventListener('click', stopAuto);
  const iv=$('autoInterval'); if(iv) iv.addEventListener('change', async ()=>{ await saveAutoSettings(); if (AUTO_CFG.on){ AUTO_LOCAL_REMAIN = Math.max(1, parseInt(String(AUTO_CFG.interval||1),10)) * 60; updateAutoStatus(); } });
  const rand=$('autoRandomize'); if(rand) rand.addEventListener('change', async ()=>{ await saveAutoSettings(); if (AUTO_CFG.on){ AUTO_LOCAL_REMAIN = Math.max(1, parseInt(String(AUTO_CFG.interval||1),10)) * 60; updateAutoStatus(); } });
})();
async function fetchFavorites(){ try{ const r=await fetch('/api/favorites'); if(!r.ok) throw new Error('fav get failed'); const data=await r.json(); FAVS = Array.isArray(data.items)? data.items : []; FAVS_LOADED=true; }catch(e){ console.warn('favorites load failed', e); FAVS=[]; FAVS_LOADED=false; } finally { const addBtn=$('btnFavAdd'), openBtn=$('btnFavOpen'); if(addBtn) addBtn.disabled=!FAVS_LOADED; if(openBtn) openBtn.disabled=!FAVS_LOADED; } }
async function ensureFavoritesLoaded(){ if (FAVS_LOADED) return; await fetchFavorites(); }
async function addFavorite(){ if(!FAVS_LOADED) return; const name=window.prompt('Name this favorite:'); if(!name) return; const cfg=buildExportConfig(); const body={ name, ...cfg }; try{ const r=await fetch('/api/favorites/add',{ method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)}); if(!r.ok) throw new Error('fav add failed'); const js=await r.json(); const st=$('status'); if(st){ st.textContent='saved'; setTimeout(()=>st.textContent='\u00a0', 1200); } await fetchFavorites();
  // Auto-select newly added favorite if in Auto mode or no selections yet
  const newId = (js && typeof js.id === 'number') ? js.id : (FAVS.length? FAVS.length-1 : -1);
  if (newId>=0 && (document.querySelector('.navlink[data-mode].active')?.dataset.mode==='auto' || AUTO_CFG.selections.length===0)){
    if (!AUTO_CFG.selections.includes(newId)) AUTO_CFG.selections.push(newId);
    renderAutoFavList(); updateSelectAllIndeterminate();
  }
 }catch(e){ console.error('addFavorite failed', e); const st=$('status'); if(st){ st.textContent='error'; st.style.background='#ef4444'; } } }
async function deleteFavorite(id){ try{ const r=await fetch('/api/favorites/delete',{ method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({id})}); if(!r.ok) throw new Error('fav delete failed'); await fetchFavorites(); renderFavList(); // refresh auto UI too
  renderAutoFavList(); updateSelectAllIndeterminate(); if(document.querySelector('.navlink[data-mode].active')?.dataset.mode==='auto'){ await loadAutoConfig(); }
} catch(e){ console.error('deleteFavorite failed', e); const st=$('status'); if(st){ st.textContent='error'; st.style.background='#ef4444'; } } }
function openFavorites(){ renderFavList(); const m=$('favModal'); if(m) m.hidden=false; }
function closeFavorites(){ const m=$('favModal'); if(m) m.hidden=true; }

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

async function loadFavorite(item){
  try{
    if(!item||!item.cfg) throw new Error('missing cfg');
    // Apply into form
    applyImportedConfig(item.cfg);
    // Auto-submit to device (leader then follower)
    const g=gatherGlobals(); const L=gather('L'); const F=gather('F');
    await send(0,L.animIndex,L.params,g);
    await new Promise(r=>setTimeout(r,40));
    await send(1,F.animIndex,F.params,g);
    const st=$('status'); if(st){ st.textContent='loaded'; setTimeout(()=>st.textContent='\u00a0', 1400); }
    // Close modal if open
    closeFavorites();
  }catch(e){ console.error('loadFavorite failed', e); const st=$('status'); if(st){ st.textContent='error'; st.style.background='#ef4444'; } }
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

function setParamValueByPid(containerId, pid, value){
  setParamValueInContainer(containerId, pd=>pd.id===pid, value);
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
    const r=await fetch('/api/state'); if(!r.ok) throw new Error('state get failed'); const s=await r.json();
    // Leader
    if (s.leader){
      $('L_anim').value=String(s.leader.animIndex); buildControlsFor('L');
      const lp = Array.isArray(s.leader.params) ? s.leader.params : [];
      lp.forEach(p=>{ if (p && typeof p.id==='number' && typeof p.value==='number') setParamValueByPid('L_paramContainer', p.id, p.value); });
    }
    // Follower
    if (s.follower){
      $('F_anim').value=String(s.follower.animIndex); buildControlsFor('F');
      const fp = Array.isArray(s.follower.params) ? s.follower.params : [];
      fp.forEach(p=>{ if (p && typeof p.id==='number' && typeof p.value==='number') setParamValueByPid('F_paramContainer', p.id, p.value); });
    }
    // Globals from leader params by PIDs
    const pids = { speed: 20, min: 21, max: 22 };
    const lp = (s.leader && Array.isArray(s.leader.params)) ? s.leader.params : [];
    const findVal=(pid)=>{ const it=lp.find(x=>x&&x.id===pid); return it? it.value : null; };
    const gs=findVal(pids.speed); if (typeof gs==='number') setParamValueByPid('G_paramContainer', pids.speed, gs);
    const gmin=findVal(pids.min); if (typeof gmin==='number') setParamValueByPid('G_paramContainer', pids.min, gmin);
    const gmax=findVal(pids.max); if (typeof gmax==='number') setParamValueByPid('G_paramContainer', pids.max, gmax);
    return true;
  }catch(e){console.warn('state load failed',e); return false;}
}

async function init(){
  const overlay=document.getElementById('loadingOverlay'); const hideOverlay=()=>{ if(overlay) overlay.style.display='none'; };
  await buildSchema();
  buildAnimSelect($('L_anim')); buildAnimSelect($('F_anim'));
  buildControlsFor('L'); buildControlsFor('F'); buildGlobals();
  initTabs(); buildGrids(); setMode('normal');
  // Wire Import / Export action buttons
  const btnImport=$('btnImport'); if(btnImport){ btnImport.addEventListener('click', (e)=>{ e.preventDefault(); doImport(); }); }
  const btnExport=$('btnExport'); if(btnExport){ btnExport.addEventListener('click', (e)=>{ e.preventDefault(); doExport(); }); }
  // Wire Favorites buttons
  const btnFavAdd=$('btnFavAdd'); if(btnFavAdd){ btnFavAdd.addEventListener('click', (e)=>{ e.preventDefault(); addFavorite(); }); }
  const btnFavOpen=$('btnFavOpen'); if(btnFavOpen){ btnFavOpen.addEventListener('click', (e)=>{ e.preventDefault(); openFavorites(); }); }
  const favClose=$('favClose'); if(favClose){ favClose.addEventListener('click', (e)=>{ e.preventDefault(); closeFavorites(); }); }
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
  // First-load: wait for state, favorites, and auto config
  let stateOk=false, favOk=false, cfgOk=false;
  try{ stateOk = await loadState(); }catch{ stateOk=false; }
  try{ await fetchFavorites(); favOk=true; }catch{ favOk=false; }
  try { const r = await fetch('/api/auto/config'); if (r.ok) { const data = await r.json(); if (data && data.on) { setMode('auto'); } cfgOk=true; } else { cfgOk=false; } } catch(_) { cfgOk=false; }
  hideOverlay();
}
init();
  </script>
</body>
</html>
)HTML";

