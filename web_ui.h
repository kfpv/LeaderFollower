// Auto-generated responsive web UI for Leader node
#pragma once
#include <Arduino.h>

// Mobile friendly, leader/follower tabs on small screens, flat button style
// Served from PROGMEM to reduce RAM usage.
static const char INDEX_HTML[] PROGMEM = R"HTML(
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
    /* Range styling (webkit) */
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
  <header><h1>Leader Controls</h1></header>
  <div class="container">
    <div class="card">
      <div class="row"><label>Global speed</label><input id="globalSpeed" type="range" min="0" max="4" step="0.01"><input id="globalSpeed_n" type="number" min="0" max="4" step="0.01"></div>
      <div class="row"><label>Global min</label><input id="globalMin" type="range" min="0" max="1" step="0.01"><input id="globalMin_n" type="number" min="0" max="1" step="0.01"></div>
      <div class="row"><label>Global max</label><input id="globalMax" type="range" min="0" max="1" step="0.01"><input id="globalMax_n" type="number" min="0" max="1" step="0.01"></div>
    </div>
    <div class="tabs">
      <button class="tab active" data-target="leaderCard">Leader</button>
      <button class="tab" data-target="followerCard">Follower</button>
    </div>
    <div class="twocol">
      <div class="card active" id="leaderCard">
        <div class="pill">Leader animation</div>
        <div class="row"><label>Type</label><select id="L_anim"><option value="0">Static</option><option value="1">Wave</option><option value="2">Pulse</option><option value="3">Chase</option></select><span></span></div>
        <div class="row"><label>Speed</label><input id="L_speed" type="range" min="0" max="12" step="0.01"><input id="L_speed_n" type="number" min="0" max="12" step="0.01"></div>
        <div class="row"><label>Phase</label><input id="L_phase" type="range" min="-6.283" max="6.283" step="0.001"><input id="L_phase_n" type="number" min="-6.283" max="6.283" step="0.001"></div>
        <div class="row"><label>Width</label><input id="L_width" type="range" min="1" max="8" step="1"><input id="L_width_n" type="number" min="1" max="8" step="1"></div>
        <div class="row"><label>Branch mode</label><div class="switch"><input id="L_branch" type="checkbox"><span>per-branch</span></div><span></span></div>
        <div class="row"><label>Invert</label><div class="switch"><input id="L_invert" type="checkbox"><span>reverse</span></div><span></span></div>
      </div>
      <div class="card" id="followerCard">
        <div class="pill">Follower animation</div>
        <div class="row"><label>Type</label><select id="F_anim"><option value="0">Static</option><option value="1">Wave</option><option value="2">Pulse</option><option value="3">Chase</option></select><span></span></div>
        <div class="row"><label>Speed</label><input id="F_speed" type="range" min="0" max="12" step="0.01"><input id="F_speed_n" type="number" min="0" max="12" step="0.01"></div>
        <div class="row"><label>Phase</label><input id="F_phase" type="range" min="-6.283" max="6.283" step="0.001"><input id="F_phase_n" type="number" min="-6.283" max="6.283" step="0.001"></div>
        <div class="row"><label>Width</label><input id="F_width" type="range" min="1" max="8" step="1"><input id="F_width_n" type="number" min="1" max="8" step="1"></div>
        <div class="row"><label>Branch mode</label><div class="switch"><input id="F_branch" type="checkbox"><span>per-branch</span></div><span></span></div>
        <div class="row"><label>Invert</label><div class="switch"><input id="F_invert" type="checkbox"><span>reverse</span></div><span></span></div>
      </div>
    </div>
    <div class="actions"><button class="btn" id="apply">Write Settings</button><span id="status" class="pill">&nbsp;</span></div>
  </div>
  <script>
    const $=id=>document.getElementById(id);const link=(a,b)=>{a.addEventListener('input',()=>b.value=a.value);b.addEventListener('input',()=>a.value=b.value)};
    [['globalSpeed','globalSpeed_n'],['globalMin','globalMin_n'],['globalMax','globalMax_n'],['L_speed','L_speed_n'],['L_phase','L_phase_n'],['L_width','L_width_n'],['F_speed','F_speed_n'],['F_phase','F_phase_n'],['F_width','F_width_n']].forEach(([a,b])=>link($(a),$(b)));
    function initTabs(){const tabs=[...document.querySelectorAll('.tab')];const leader=$('leaderCard'), follower=$('followerCard');tabs.forEach(t=>t.addEventListener('click',()=>{tabs.forEach(x=>x.classList.remove('active'));t.classList.add('active');[leader,follower].forEach(c=>c.classList.remove('active'));const target=$(t.dataset.target);target.classList.add('active')}));}
    async function load(){try{const r=await fetch('/api/state');const s=await r.json();$('globalSpeed').value=$('globalSpeed_n').value=s.globalSpeed;$('globalMin').value=$('globalMin_n').value=s.globalMin;$('globalMax').value=$('globalMax_n').value=s.globalMax;$('L_anim').value=s.leader.animIndex;$('L_speed').value=$('L_speed_n').value=s.leader.speed;$('L_phase').value=$('L_phase_n').value=s.leader.phase;$('L_width').value=$('L_width_n').value=s.leader.width;$('L_branch').checked=s.leader.branchMode;$('L_invert').checked=s.leader.invert;$('F_anim').value=s.follower.animIndex;$('F_speed').value=$('F_speed_n').value=s.follower.speed;$('F_phase').value=$('F_phase_n').value=s.follower.phase;$('F_width').value=$('F_width_n').value=s.follower.width;$('F_branch').checked=s.follower.branchMode;$('F_invert').checked=s.follower.invert;}catch(e){console.error(e);}}
    async function apply(){const f=new URLSearchParams();f.set('globalSpeed',$('globalSpeed_n').value);f.set('globalMin',$('globalMin_n').value);f.set('globalMax',$('globalMax_n').value);f.set('L_anim',$('L_anim').value);f.set('L_speed',$('L_speed_n').value);f.set('L_phase',$('L_phase_n').value);f.set('L_width',$('L_width_n').value);f.set('L_branch',$('L_branch').checked?'1':'0');f.set('L_invert',$('L_invert').checked?'1':'0');f.set('F_anim',$('F_anim').value);f.set('F_speed',$('F_speed_n').value);f.set('F_phase',$('F_phase_n').value);f.set('F_width',$('F_width_n').value);f.set('F_branch',$('F_branch').checked?'1':'0');f.set('F_invert',$('F_invert').checked?'1':'0');const r=await fetch('/api/apply',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:String(f)});$('status').textContent=await r.text()||'applied';setTimeout(()=>$('status').textContent='\u00a0',1600);} 
    $('apply')?.addEventListener('click',apply);initTabs();load();
  </script>
</body>
</html>
)HTML";
