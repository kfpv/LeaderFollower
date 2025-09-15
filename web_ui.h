// Auto-generated dynamic web UI (data-driven animation controls)
#pragma once
#include <Arduino.h>
#include "anim_schema.h"

// Served from PROGMEM to reduce RAM usage.
// Endpoint expectations on device:
//   GET /api/schema -> JSON { params:[{id,name,type,min,max,def,bits}], animations:[{index,name,params:[ids]}] }
//   POST /api/cfg2  -> accepts JSON { role:0|1, animIndex, params:[{id,value}], globals:[{id,value}] }
// (Adjust to your server implementation; this UI only issues fetch calls.)

// Generate JavaScript with embedded schema - we need to reconstruct the JSON in JavaScript
// since we can't directly embed C string literals into JS within raw string literals




// Generate the complete HTML with embedded schema
static const char INDEX_HTML_PREFIX[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Leader Controller</title>
<style>
html,body{margin:0;padding:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:#0d0f17;color:#e5e8ef}
h1{font-size:1.1rem;margin:0 0 .75rem;font-weight:600}
select,input[type=range]{width:100%}
label{font-size:.70rem;text-transform:uppercase;letter-spacing:.05em;opacity:.8;display:block;margin:0 0 .15rem}
.grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));}
.card{background:#141927;border:1px solid #222a3b;border-radius:10px;padding:14px;box-shadow:0 2px 4px rgba(0,0,0,.3)}
.row{margin-bottom:10px}
button{background:#2563eb;color:#fff;border:none;padding:10px 16px;border-radius:8px;font-size:.9rem;cursor:pointer;font-weight:500}
button:active{transform:translateY(1px)}
button.secondary{background:#374151}
.flex{display:flex;gap:12px;flex-wrap:wrap}
.param-row input[type=range]{margin:0}
.section-title{font-size:.75rem;font-weight:600;letter-spacing:.1em;opacity:.65;margin:20px 0 6px;text-transform:uppercase}
.inline{display:flex;align-items:center;gap:6px}
footer{margin-top:28px;font-size:.65rem;opacity:.55;text-align:center}
.tabs{display:flex;margin-bottom:16px;border-bottom:1px solid #1f2532}
.tabs button{flex:1;background:#141927;border-radius:0;border:0;border-bottom:2px solid transparent}
.tabs button.active{border-bottom-color:#2563eb;background:#192132}
.hide{display:none !important}
@media (max-width:700px){.grid{grid-template-columns:1fr}}
</style></head><body>
<div style="max-width:1100px;margin:0 auto;padding:18px">
  <h1>Leader / Follower Control</h1>
  <div class="tabs">
    <button id="tabLeader" class="active">Leader</button>
    <button id="tabFollower">Follower</button>
    <button id="tabGlobals">Globals</button>
  </div>
  <div id="panelLeader" class="panel">
    <div class="card"><div class="row"><label>Animation</label><select id="L_anim"></select></div><div id="L_paramContainer"></div></div>
  </div>
  <div id="panelFollower" class="panel hide">
    <div class="card"><div class="row"><label>Animation</label><select id="F_anim"></select></div><div id="F_paramContainer"></div></div>
  </div>
  <div id="panelGlobals" class="panel hide">
    <div class="card"><div class="row" id="G_paramContainer"></div></div>
  </div>
  <div class="flex" style="margin-top:16px">
    <button id="applyLeader">Send Leader</button>
    <button id="applyFollower">Send Follower</button>
    <button id="applyBoth" class="secondary">Send Both</button>
  </div>
  <footer>Dynamic UI prototype – parameters generated from schema.</footer>
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

  // Only touch data.params[*].min/max/def
  if (Array.isArray(data.params)) {
    data.params.forEach(p => {
      if (p) { // skip the final null
        p.min = toFloat(p.min);
        p.max = toFloat(p.max);
        p.def = toFloat(p.def);
      }
    });
  }

  return data;
}

const SCHEMA = parseSchemaAndFix(SCHEMA_JSON);

function $(id){return document.getElementById(id);} // small helper

const tabs=[{btn:'tabLeader',panel:'panelLeader'},{btn:'tabFollower',panel:'panelFollower'},{btn:'tabGlobals',panel:'panelGlobals'}];
tabs.forEach(t=>{ $(t.btn).onclick=()=>{ tabs.forEach(x=>{ $(x.btn).classList.toggle('active',x.btn===t.btn); $(x.panel).classList.toggle('hide',x.btn!==t.btn); }); }; });

let schema=null; // local schema
const paramMap=new Map();

function normalizeNumber(v){ return typeof v==='number'? v : parseFloat(v); }
async function buildSchema(){
  if (schema) return schema;
  
  // Schema is now directly embedded, no need to fetch
  schema = SCHEMA;
  // Normalize numbers
  schema.params.forEach(p=>{ 
    if (p) {
      p.id = +p.id; 
      p.type = +p.type; 
      p.min = normalizeNumber(p.min); 
      p.max = normalizeNumber(p.max); 
      p.def = normalizeNumber(p.def); 
      p.bits = +p.bits;
    }
  });
  schema.animations.forEach(a=>{ 
    if (a) {
      a.index = +a.index; 
      a.params = a.params.map(x=>+x); 
    }
  });
  schema.params.forEach(p=>{if (p) paramMap.set(p.id,p);});
  return schema;
}

function createParamControl(pd, prefix){
  const wrap=document.createElement('div'); wrap.className='row param-row';
  const label=document.createElement('label'); label.textContent=pd.name; wrap.appendChild(label);
  let input;
  if (pd.type===0){ // bool
    input=document.createElement('input'); input.type='checkbox'; input.checked=!!pd.def;
  } else if (pd.type===3){ // enum (not defined yet) placeholder as select
    input=document.createElement('select');
  } else { // range/int
    input=document.createElement('input'); input.type='range';
    input.min=pd.min; input.max=pd.max; input.value=pd.def;
    input.step=(pd.type===1)? (pd.bits<=8? '0.01':'0.001') : '1';
  }
  input.dataset.pid=pd.id; input.id=prefix+'p'+pd.id;
  // live value bubble
  if (input.type==='range') {
    const span=document.createElement('div'); span.style.fontSize='.65rem'; span.style.opacity='.7'; span.textContent=input.value;
    input.addEventListener('input',()=>{ span.textContent=input.value; });
    wrap.appendChild(input); wrap.appendChild(span);
  } else {
    wrap.appendChild(input);
  }
  return wrap;
}

function buildAnimSelect(selectEl){
  selectEl.innerHTML='';
  schema.animations.forEach(a=>{ const o=document.createElement('option'); o.value=a.index; o.textContent=a.name; selectEl.appendChild(o); });
}

function buildControlsFor(prefix){
  const selectEl = $(prefix+'_anim');
  const container = $(prefix+'_paramContainer');
  function rebuild(){
    container.innerHTML='';
    const anim = schema.animations.find(a=>a.index==selectEl.value);
    if (!anim) return;
    anim.params.forEach(pid=>{ const pd=paramMap.get(pid); if(pd) container.appendChild(createParamControl(pd,prefix)); });
  }
  selectEl.onchange=rebuild; rebuild();
}

function buildGlobals(){
  const gc=$('G_paramContainer'); gc.innerHTML='';
  if (!schema) return;
  // Build a set of param IDs used by any animation
  const used=new Set();
  schema.animations.forEach(a=>{ (a.params||[]).forEach(pid=>used.add(pid)); });
  // Globals are params not referenced by any animation
  const globals = schema.params.filter(p=>!used.has(p.id));
  // Stable sort by name for nicer UI
  globals.sort((a,b)=>String(a.name).localeCompare(String(b.name)));
  globals.forEach(pd=>{ gc.appendChild(createParamControl(pd,'G_')); });
}

function gather(prefix){
  const selectEl = $(prefix+'_anim');
  const container = $(prefix+'_paramContainer');
  const animIndex = selectEl? parseInt(selectEl.value,10):0;
  const params=[];
  container.querySelectorAll('[data-pid]').forEach(el=>{
    const pid=parseInt(el.dataset.pid,10); let v;
    if (el.type==='checkbox') v = el.checked?1:0; else v=parseFloat(el.value);
    params.push({id:pid,value:v});
  });
  return {animIndex, params};
}

function gatherGlobals(){
  const params=[]; document.querySelectorAll('#G_paramContainer [data-pid]').forEach(el=>{
    const pid=parseInt(el.dataset.pid,10); let v=parseFloat(el.value); if(el.type==='checkbox') v=el.checked?1:0; params.push({id:pid,value:v});
  }); return params; }

async function send(role, animIndex, params, globals){
  try {
    const body = JSON.stringify({role, animIndex, params, globals});
    await fetch('/api/cfg2',{method:'POST',headers:{'Content-Type':'application/json'},body});
  } catch(e){ console.error('send failed',e); }
}

async function init(){
  await buildSchema();
  buildAnimSelect($('L_anim')); buildAnimSelect($('F_anim'));
  buildControlsFor('L'); buildControlsFor('F'); buildGlobals();
  $('applyLeader').onclick=()=>{ const g=gatherGlobals(); const L=gather('L'); send(0,L.animIndex,L.params,g); };
  $('applyFollower').onclick=()=>{ const g=gatherGlobals(); const F=gather('F'); send(1,F.animIndex,F.params,g); };
  $('applyBoth').onclick=()=>{ const g=gatherGlobals(); const L=gather('L'); const F=gather('F'); send(0,L.animIndex,L.params,g); setTimeout(()=>send(1,F.animIndex,F.params,g),40); };
}
init();
</script>
</body></html>
)HTML";

