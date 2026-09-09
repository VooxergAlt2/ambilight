#include "network/WebUiService.h"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

#include <lwip/inet.h>
#include <lwip/sockets.h>

#include "config/FirmwareInfo.h"

namespace ambilight {
namespace {

constexpr char kIndexResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "Content-Security-Policy: default-src 'self'; "
    "connect-src 'self'; img-src 'none'; "
    "style-src 'unsafe-inline'; script-src 'unsafe-inline'\r\n"
    "Connection: close\r\n"
    "\r\n"
R"HTML(<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ambilight C6</title>
<style>
:root{color-scheme:light dark;--bg:#f4f5f7;--card:#fff;--text:#17181a;--muted:#737780;--line:#d9dce2;--accent:#4263eb;--primary:#4263eb;--danger:#c92a2a;--ok:#2b8a3e;--soft:#eef1ff}
@media(prefers-color-scheme:dark){:root{--bg:#111315;--card:#1a1d20;--text:#f3f4f5;--muted:#9aa0a8;--line:#30343a;--accent:#91a7ff;--primary:#3b5bdb;--danger:#ff6b6b;--ok:#69db7c;--soft:#252b45}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:960px;margin:auto;padding:22px 16px 64px}header{display:flex;justify-content:space-between;gap:16px;align-items:flex-end;margin-bottom:16px}
h1{font-size:22px;margin:0}h2{font-size:15px;margin:0 0 12px}h3{font-size:13px;margin:14px 0 8px}.muted{color:var(--muted)}.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
.cards,.cols{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.cols{grid-template-columns:repeat(2,1fr)}
.card,details,.panel{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}.card b{display:block;font-size:18px;margin-top:4px}
.nav{display:flex;gap:6px;overflow:auto;margin:14px 0 8px;padding:4px;background:var(--card);border:1px solid var(--line);border-radius:12px;position:sticky;top:6px;z-index:5}
.nav button{white-space:nowrap;border-color:transparent;background:transparent}.nav button.active{background:var(--soft);color:var(--accent);font-weight:650}
.page{display:none}.page.active{display:block}.page>details:first-child,.page>.panel:first-child{margin-top:10px}
details{margin-top:10px}summary{cursor:pointer;font-weight:650;user-select:none}.section{padding-top:14px}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
.segmented{display:inline-flex;gap:4px;padding:4px;border:1px solid var(--line);border-radius:10px}.segmented button{border-color:transparent}
label{color:var(--muted);font-size:12px}input,select,textarea,button{font:inherit;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);padding:8px 10px}
input[type=number]{width:105px}input[type=range]{padding:0;width:min(420px,70vw)}input[type=text],input[type=password]{min-width:220px;flex:1}
textarea{width:100%;min-height:62px;resize:vertical;font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
button{cursor:pointer}button.primary{background:var(--primary);color:#fff;border-color:transparent}button.danger{color:var(--danger)}button:disabled{opacity:.45;cursor:not-allowed}
.seg{display:grid;grid-template-columns:85px 90px 105px minmax(100px,1fr);gap:8px;align-items:center;margin:6px 0}.spatial{display:grid;grid-template-columns:repeat(4,minmax(115px,1fr));gap:8px}.field{display:flex;flex-direction:column;gap:4px}
#action{min-height:20px;margin:9px 2px;position:sticky;top:58px;z-index:6}#action:not(:empty){background:var(--card);border:1px solid var(--line);border-radius:8px;padding:8px 10px;box-shadow:0 4px 18px #0002}.ok{color:var(--ok)}.bad{color:var(--danger)}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:var(--bg);padding:10px;border-radius:8px;border:1px solid var(--line)}
.tvwrap{display:grid;grid-template-columns:minmax(220px,360px) 1fr;gap:18px;align-items:center;margin:4px 0 18px}.tv{position:relative;aspect-ratio:16/9;border:8px solid #20242a;border-radius:10px;background:#0d0f12;box-shadow:0 8px 28px #0002}.tvside{position:absolute;font:12px/1.2 ui-monospace,SFMono-Regular,Consolas,monospace;background:var(--card);border:1px solid var(--line);border-radius:7px;padding:4px 6px;white-space:nowrap}.tvtop{top:-38px;left:50%;transform:translateX(-50%)}.tvbottom{bottom:-38px;left:50%;transform:translateX(-50%)}.tvleft{left:-86px;top:50%;transform:translateY(-50%)}.tvright{right:-92px;top:50%;transform:translateY(-50%)}
.callout{background:var(--soft);border-radius:10px;padding:10px 12px}.source{font-size:12px;color:var(--muted)}
.tofgrid{display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:4px}.tofcell{padding:6px 3px;min-width:0;min-height:48px;overflow:hidden;font:11px/1.15 ui-monospace,SFMono-Regular,Consolas,monospace}.tofcell strong{display:block;font-size:12px}.tofcell.usable{border-color:var(--ok)}.tofcell.weak{border-style:dashed}.tofcell.rejected{opacity:.55}.tofcell.selected{outline:2px solid var(--accent);outline-offset:1px}
@media(max-width:760px){.cards{grid-template-columns:repeat(2,1fr)}.cols,.spatial,.tvwrap{grid-template-columns:1fr}.tvwrap{padding:30px 62px 34px}.tvleft{left:-58px}.tvright{right:-58px}.seg{grid-template-columns:65px 78px minmax(82px,1fr) minmax(92px,1fr)}header{align-items:flex-start;flex-direction:column}.nav{top:4px}}
@media(max-width:430px){main{padding-left:10px;padding-right:10px}.cards{gap:7px}.card{padding:11px}.seg{grid-template-columns:1fr 1fr}.seg>b{grid-column:1/-1}.tvwrap{padding-left:0;padding-right:0}.tvleft{left:4px}.tvright{right:4px}.tofgrid{gap:2px}.tofcell{font-size:0;padding:3px 1px;min-height:36px}.tofcell strong{font-size:10px}.spatial{grid-template-columns:1fr 1fr}input[type=number]{width:100%}}
</style>
</head>
<body>
<main>
<header><div><h1>Ambilight C6</h1><div id="fw" class="muted mono">connecting…</div></div><div class="muted">LAN control · HTTP/80</div></header>

<section class="cards">
<div class="card"><span class="muted">Коррекция ToF</span><b id="stCorr">—</b></div>
<div class="card"><span class="muted">Яркость</span><b id="stBright">—</b></div>
<div class="card"><span class="muted">Сигнал ПК</span><b id="stDdp">—</b></div>
<div class="card"><span class="muted">Датчик ToF</span><b id="stTof">—</b></div>
</section>
<div id="action" class="muted"></div>

<nav class="nav" aria-label="Разделы">
<button id="navHome" class="active" onclick="showPage('home')">Главная</button>
<button id="navLed" onclick="showPage('led')">LED</button>
<button id="navTof" onclick="showPage('tof')">ToF</button>
<button id="navDiag" onclick="showPage('diag')">Диагностика</button>
<button id="navSystem" onclick="showPage('system')">Система</button>
</nav>

<section id="pageHome" class="page active">
<div class="panel">
<h2>Подсветка и коррекция</h2>
<div class="section">
<div class="muted">Коррекция по расстоянию</div>
<div class="row segmented">
<button id="corr0" onclick="post('/api/correction','0')">Выкл.</button>
<button id="corr1" onclick="post('/api/correction','1')">Наблюдение</button>
<button id="corr2" onclick="post('/api/correction','2')">Включена</button>
</div>
<div class="row">
<label>Яркость</label>
<input id="brightness" type="range" min="0" max="255" value="32">
<span id="brightnessValue" class="mono">32</span>
</div>
<div class="muted">Изменение яркости сохраняется при отпускании ползунка. Режим «Наблюдение» рассчитывает коррекцию, но не применяет её к LED.</div>
<div id="homeStatus" class="callout" style="margin-top:12px">Ожидание данных…</div>
</div>
</div>
</section>

<section id="pageLed" class="page">
<details open>
<summary>Настройка LED</summary>
<div class="section">
<h2>Топология телевизора</h2>
<div class="tvwrap">
<div class="tv">
<div id="tvTop" class="tvside tvtop">TOP</div>
<div id="tvRight" class="tvside tvright">RIGHT</div>
<div id="tvBottom" class="tvside tvbottom">BOTTOM</div>
<div id="tvLeft" class="tvside tvleft">LEFT</div>
</div>
<div>
<div class="callout">Сначала проверьте, какой GPIO соответствует каждой физической стороне, затем направление и количество LED. Изменение топологии выполняется через автоматическое защитное гашение.</div>
<div id="tvSummary" class="muted mono" style="margin-top:10px"></div>
</div>
</div>
<h2>Параметры сторон</h2>
<div id="mapping"></div>
<div id="topologyInfo" class="muted mono"></div>
<div class="row">
<button id="mapApply" class="primary" onclick="applyMap()">Сохранить топологию</button>
<button id="mapReset" onclick="resetMap()">Вернуть измеренный профиль</button>
</div>
<div class="muted">Количество 1..230. GPIO только 18/19/20/21, каждый выход используется один раз. Флаг «развернуть» (REV) меняет только физическую адресацию LED на проводе (с какого конца лента пронумерована 0); видимое направление стороны на схеме телевизора при этом не меняется.</div>

<div class="cols" style="margin-top:16px">
<div>
<h2>Проверка логической стороны</h2>
<div class="row">
<select id="rangeSide"><option value="0">TOP</option><option value="1">RIGHT</option><option value="2">BOTTOM</option><option value="3">LEFT</option></select>
<input id="rangeStart" type="number" min="0" value="0" step="1">
<input id="rangeCount" type="number" min="1" value="1" step="1">
</div>
<div class="row">
<button id="runLogical" onclick="runLogicalRange()">Проверить диапазон</button>
<button id="runWhole" onclick="runWholeSide()">Вся сторона</button>
</div>
<div class="muted">Проверяет применённую топологию, направление и маску отключённого пикселя.</div>
</div>
<div>
<h2>Определение физического GPIO</h2>
<div class="row">
<select id="rawGpio"><option>18</option><option>19</option><option>20</option><option>21</option></select>
<input id="rawStart" type="number" min="0" max="229" value="0" step="1">
<input id="rawCount" type="number" min="1" max="230" value="1" step="1">
</div>
<div class="row">
<button id="runRaw" onclick="runRawRange()">Зажечь GPIO</button>
<button id="testSegments" onclick="post('/api/test','1')">Все стороны</button>
<button id="testDirection" onclick="post('/api/test','2')">Маркеры направления</button>
<button onclick="post('/api/test','0')">Стоп</button>
</div>
<div class="muted">Этот тест обходит логическое сопоставление и нужен только для определения физически подключённой линии.</div>
</div>
</div>
<div id="testState" class="muted"></div>

<h2 style="margin-top:16px">Отключённый пиксель</h2>
<div id="pixelMask"></div>
<div class="row">
<button class="primary" onclick="applyPixelMask()">Сохранить маску</button>
<button onclick="resetPixelMask()">Очистить</button>
<span id="maskSource" class="muted"></span>
</div>
<div class="muted">По одному пикселю на сторону. Пусто = не отключать. Индекс считается от логического начала стороны.</div>
</div>
</details>
</section>

<section id="pageTof" class="page">
<details open>
<summary>Настройка ToF</summary>
<div class="section">
<h2>Матрица расстояний 8×8</h2>
<div class="row">
<button id="tofDebugStart" class="primary" onclick="post('/api/tof-debug','start')">Live-режим 60 с</button>
<button id="tofDebugStop" onclick="post('/api/tof-debug','stop')">Стоп</button>
<span id="tofDebugState" class="muted"></span>
</div>
<div class="muted">Матрица уже ориентирована относительно телевизора. Верх сетки соответствует TOP. Зелёная рамка означает пригодную зону, пунктирная — пониженную уверенность.</div>
<div class="cols" style="margin-top:12px">
<div>
<div class="muted" style="text-align:center">TOP ↑</div>
<div id="tofGrid" class="tofgrid"></div>
<div class="muted">← LEFT · RIGHT →</div>
</div>
<div>
<h2>Выбранная зона</h2>
<pre id="tofZoneDetail">Выберите ячейку</pre>
<h2>Плоскость стены</h2>
<pre id="tofDetail">—</pre>
</div>
</div>

<h2 style="margin-top:16px">Геометрия установки</h2>
<div class="callout" style="margin-bottom:10px">Размеры задаются в плоскости телевизора. X/Y — смещение датчика от центра экрана, Z — смещение плоскости LED относительно датчика.</div>
<div class="spatial">
<div class="field"><label>Ширина ТВ, мм</label><input id="spW" type="number" min="100" max="5000" step=".1"></div>
<div class="field"><label>Высота ТВ, мм</label><input id="spH" type="number" min="100" max="5000" step=".1"></div>
<div class="field"><label>Датчик X, мм</label><input id="spX" type="number" min="-2000" max="2000" step=".1"></div>
<div class="field"><label>Датчик Y, мм</label><input id="spY" type="number" min="-2000" max="2000" step=".1"></div>
<div class="field"><label>Плоскость LED Z, мм</label><input id="spZ" type="number" min="-1000" max="1000" step=".1"></div>
<div class="field"><label>Поворот датчика</label><select id="spR"><option value="0">0°</option><option value="1">90°</option><option value="2">180°</option><option value="3">270°</option></select></div>
<div class="field"><label>Отразить горизонтально</label><select id="spM"><option value="0">Нет</option><option value="1">Да</option></select></div>
<div class="field"><label>Порог изменения плоскости, мм</label><input id="spD" type="number" min="1" max="500" step=".1"></div>
</div>
<div class="row">
<button id="spApply" class="primary" onclick="applySpatial()">Сохранить геометрию</button>
<button id="spReset" onclick="resetSpatial()">По умолчанию</button>
<span id="spSource" class="source"></span>
</div>

<h2>Яркость в зависимости от расстояния</h2>
<div class="muted">Формат: <span class="mono">расстояние_мм:яркость_%</span>. От 2 до 8 точек; расстояние строго возрастает, яркость не уменьшается. Q12 преобразуется автоматически.</div>
<textarea id="curve" spellcheck="false" placeholder="50:50,500:75,4000:100"></textarea>
<div class="row">
<button id="curveApply" class="primary" onclick="applyCurve()">Сохранить кривую</button>
<button id="curveReset" onclick="resetCurve()">Нейтральная 100%</button>
<span id="curveSource" class="source"></span>
</div>

<div class="cols">
<div>
<h2>Калибровка</h2>
<div class="row">
<button id="calStart" onclick="post('/api/calibration','start')">Снять данные 60 с</button>
<button id="probeStart" onclick="post('/api/shadow-probe','start')">Тест модели 10 с</button>
</div>
<pre id="calDetail">Калибровка ещё не выполнялась.</pre>
</div>
</div>
<div class="muted">Изменения геометрии и кривой запрещены в режиме «Включена». Во время 60-секундной калибровки также блокируются геометрия и LED-топология, чтобы итог не смешивал разные конфигурации. Калибровка не изменяет физический вывод.</div>
</div>
</details>
</section>

<section id="pageSystem" class="page">
<details open>
<summary>Сеть</summary>
<div class="section">
<div id="wifiState" class="muted"></div>
<div class="row"><input id="ssid" type="text" maxlength="32" placeholder="SSID"><input id="wifiPass" type="password" maxlength="63" placeholder="Новый пароль"></div>
<div class="row"><label><input id="wifiOpen" type="checkbox"> Открытая сеть, без пароля</label></div>
<div class="row">
<button class="primary" onclick="applyWifi()">Сохранить и подключиться</button>
<button onclick="forgetWifi()">Забыть сохранённую сеть</button>
</div>
<div class="muted">Пароль никогда не возвращается браузеру. Пустое поле не означает «оставить прежний пароль»: для защищённой сети введите пароль заново; для сети без пароля явно отметьте «Открытая сеть». После смены сети эта страница может потерять соединение.</div>
</div>
</details>
<div class="panel" style="margin-top:10px">
<h2>Сброс конфигурации</h2>
<div class="row">
<button id="factory" class="danger" onclick="factoryReset()">Заводской сброс</button>
</div>
<div class="muted">Сброс доступен только при яркости 0. Будут удалены Wi-Fi и все runtime-настройки Ambilight.</div>
</div>
</section>

<section id="pageDiag" class="page">
<details open>
<summary>Диагностика</summary>
<div class="section">
<pre id="diag">—</pre>
<div class="muted">Низкоуровневые счётчики предназначены для поиска проблем DDP, памяти и web-сервиса. Web UI не имеет отдельной аутентификации: не публикуйте TCP/80 наружу.</div>
</div>
</details>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let pendingActionId=0,pendingActionStartedMs=0,pendingDirtyIds=[],pendingFieldState={},posting=false,refreshing=false,offlineBanner=false,tofDebugUiActive=false,selectedTofZone=27,activeMap=null;
const corrNames=['ВЫКЛ.','НАБЛЮДЕНИЕ','ВКЛЮЧЕНА'];
const mapNames=['TOP','RIGHT','BOTTOM','LEFT'];
const pages=['home','led','tof','diag','system'];
function showPage(name){
  if(!pages.includes(name))name='home';
  pages.forEach(p=>{$('page'+p[0].toUpperCase()+p.slice(1)).classList.toggle('active',p===name);$('nav'+p[0].toUpperCase()+p.slice(1)).classList.toggle('active',p===name)});
}
function sourceLabel(v){if(v==='CUSTOM_NVS')return 'Сохранено';if(v==='CUSTOM_RUNTIME')return 'Временно';if(v==='DEFAULT')return 'По умолчанию';return v||'—'}
function brightnessLabel(v){const n=Number(v)||0;return Math.round(n*100/255)+'% · '+n+'/255'}
function gainPercent(q){return (Number(q)*100/4096).toFixed(3).replace(/\.0+$/,'').replace(/(\.\d*?)0+$/,'$1')}
function sideArrow(i){return ['→','↓','←','↑'][i]}
function txt(id,v){$(id).textContent=v}
function setv(id,v){const e=$(id);if(!e.dataset.dirty)e.value=v}
function clean(ids){ids.forEach(id=>{const e=$(id);if(e)delete e.dataset.dirty})}
function fieldState(id){const e=$(id);if(!e)return '';return e.type==='checkbox'?(e.checked?'1':'0'):String(e.value)}
function cleanPendingIfUnchanged(){pendingDirtyIds.forEach(id=>{if(fieldState(id)===pendingFieldState[id])clean([id])})}
function markDirty(){document.querySelectorAll('input,select,textarea').forEach(e=>{if(e.dataset.bound)return;e.dataset.bound='1';e.addEventListener('input',()=>e.dataset.dirty='1')})}
function actionError(message){txt('action',message);$('action').className='bad';return false}
function syncActionLock(){$('brightness').disabled=posting||!!pendingActionId}
function rollbackAutosaveDrafts(){if(pendingDirtyIds.includes('brightness'))clean(['brightness'])}
function clearPendingAction(){pendingActionId=0;pendingActionStartedMs=0;pendingDirtyIds=[];pendingFieldState={};syncActionLock()}
function actionSequenceAfter(current,expected){
  const a=Number(current)>>>0,b=Number(expected)>>>0;
  if(!a||!b||a===b)return false;
  return ((a-b)>>>0)<0x80000000;
}
async function post(path,body,dirtyIds=[]){
  if(posting||pendingActionId)return actionError('Дождитесь завершения предыдущего действия.');
  posting=true;syncActionLock();txt('action','Отправляем…');$('action').className='muted';
  try{
    const r=await fetch(path,{method:'POST',headers:{'X-Ambilight-Control':'1'},body:String(body)});
    const j=await r.json();
    if(!r.ok)throw new Error(j.error||('HTTP '+r.status));
    const queued=Number(j.queued||0);
    if(!queued)throw new Error('controller did not return action id');
    pendingActionId=queued;pendingActionStartedMs=Date.now();pendingDirtyIds=[...dirtyIds];pendingFieldState={};pendingDirtyIds.forEach(id=>pendingFieldState[id]=fieldState(id));
    txt('action','Применяем…');$('action').className='muted';
    setTimeout(refresh,120);
    return true;
  }catch(e){actionError(e.message);return false}
  finally{posting=false;syncActionLock()}
}
function mapFieldIds(){return [...Array(4)].flatMap((_,i)=>['count'+i,'gpio'+i,'rev'+i])}
function validateMap(){
  const gpios=[],p=[];
  for(let i=0;i<4;i++){
    const count=Number($('count'+i).value),gpio=Number($('gpio'+i).value);
    if(!Number.isInteger(count)||count<1||count>230)return actionError(mapNames[i]+': количество LED должно быть 1..230.');
    if(![18,19,20,21].includes(gpio))return actionError(mapNames[i]+': допустимы только GPIO18/19/20/21.');
    if(gpios.includes(gpio))return actionError('GPIO'+gpio+' назначен более чем одной стороне.');
    gpios.push(gpio);p.push(count+':'+gpio+':'+($('rev'+i).checked?'1':'0'));
  }
  return p;
}
function applyMap(){const p=validateMap();if(!p)return;post('/api/led-map',p.join(','),mapFieldIds())}
function resetMap(){post('/api/led-map','reset',mapFieldIds())}
function runLogicalRange(){post('/api/test','side:'+$('rangeSide').value+':'+$('rangeStart').value+':'+$('rangeCount').value)}
function runWholeSide(){
  const i=Number($('rangeSide').value);
  if(!activeMap||!activeMap.segments||!activeMap.segments[i])return actionError('Активная topology ещё не загружена.');
  $('rangeStart').value=0;$('rangeCount').value=activeMap.segments[i][0];runLogicalRange()
}
function runRawRange(){post('/api/test','gpio:'+$('rawGpio').value+':'+$('rawStart').value+':'+$('rawCount').value)}
function maskFieldIds(){return [0,1,2,3].map(i=>'mask'+i)}
function applyPixelMask(){
  const ids=maskFieldIds(),p=[];
  for(let i=0;i<ids.length;i++){
    const raw=$(ids[i]).value.trim();
    if(raw===''){p.push('-');continue}
    const value=Number(raw),max=activeMap&&activeMap.segments?activeMap.segments[i][0]-1:Number($(ids[i]).max);
    if(!Number.isInteger(value)||value<0||value>max)return actionError(mapNames[i]+': индекс отключённого LED должен быть 0..'+max+'.');
    p.push(String(value));
  }
  post('/api/pixel-mask',p.join(','),ids)
}
function resetPixelMask(){post('/api/pixel-mask','reset',maskFieldIds())}
function spatialFieldIds(){return ['spW','spH','spX','spY','spZ','spR','spM','spD']}
function spatialNumber(id,label,min,max){
  const raw=$(id).value.trim();
  if(raw===''){actionError(label+': заполните поле.');return null}
  const n=Number(raw);
  if(!Number.isFinite(n)||n<min||n>max){actionError(label+': допустимо '+min+'..'+max+' мм.');return null}
  return n.toFixed(1)
}
function applySpatial(){
  const ids=spatialFieldIds();
  const w=spatialNumber('spW','Ширина ТВ',100,5000);if(w===null)return;
  const h=spatialNumber('spH','Высота ТВ',100,5000);if(h===null)return;
  const x=spatialNumber('spX','Датчик X',-2000,2000);if(x===null)return;
  const y=spatialNumber('spY','Датчик Y',-2000,2000);if(y===null)return;
  const z=spatialNumber('spZ','Плоскость LED Z',-1000,1000);if(z===null)return;
  const d=spatialNumber('spD','Deadband',1,500);if(d===null)return;
  const rot=Number($('spR').value),mirror=Number($('spM').value);
  if(!Number.isInteger(rot)||rot<0||rot>3)return actionError('Поворот датчика должен быть 0°, 90°, 180° или 270°.');
  if(mirror!==0&&mirror!==1)return actionError('Отражение должно быть «Нет» или «Да».');
  post('/api/spatial',[w,h,x,y,z,rot,mirror,d].join(','),ids)
}
function resetSpatial(){post('/api/spatial','reset',spatialFieldIds())}
function applyCurve(){
  const raw=$('curve').value.trim(),items=raw?raw.split(','):[];
  if(items.length<2||items.length>8)return actionError('Кривая должна содержать от 2 до 8 точек.');
  let prevD=-1,prevP=-1;const out=[];
  for(const item of items){
    const parts=item.split(':');
    if(parts.length!==2)return actionError('Используйте формат расстояние_мм:яркость_%.');
    const d=Number(parts[0].trim()),pct=Number(parts[1].trim());
    if(!Number.isInteger(d)||d<0||d>65535)return actionError('Расстояние должно быть целым числом 0..65535 мм.');
    if(!Number.isFinite(pct)||pct<0||pct>100)return actionError('Яркость должна быть в диапазоне 0..100%.');
    if(d<=prevD)return actionError('Расстояния должны строго возрастать.');
    if(pct<prevP)return actionError('Яркость не должна уменьшаться с ростом расстояния.');
    out.push(d+':'+Math.round(pct*4096/100));prevD=d;prevP=pct;
  }
  post('/api/curve',out.join(','),['curve'])
}
function resetCurve(){post('/api/curve','reset',['curve'])}
async function applyWifi(){
  const ssid=$('ssid').value.trim(),open=$('wifiOpen').checked,p=$('wifiPass').value;
  if(!ssid)return actionError('Введите SSID.');
  if(!open&&p==='')return actionError('Введите пароль защищённой сети или явно отметьте «Открытая сеть».');
  const ok=await post('/api/wifi',ssid+'|'+(open?'':p),['ssid']);
  if(ok){
    $('wifiPass').value='';delete $('wifiPass').dataset.dirty;
    $('wifiOpen').checked=false;delete $('wifiOpen').dataset.dirty;
    $('wifiPass').disabled=false;
  }
}
function forgetWifi(){if(confirm('Забыть сохранённую Wi-Fi сеть? После этого web-интерфейс может стать недоступен.'))post('/api/wifi','clear')}
function factoryReset(){if(confirm('Стереть всю конфигурацию Ambilight и перезагрузить контроллер?'))post('/api/factory','reset')}
function renderMap(m){
  activeMap=m;
  if(!$('mapping').children.length){
    mapNames.forEach((n,i)=>{$('mapping').insertAdjacentHTML('beforeend',`<div class="seg"><b>${n}</b><input id="count${i}" title="Количество LED" type="number" min="1" max="230" step="1"><select id="gpio${i}" title="GPIO"><option>18</option><option>19</option><option>20</option><option>21</option></select><label><input id="rev${i}" type="checkbox"> развернуть</label></div>`)});
    markDirty();
  }
  m.segments.forEach((x,i)=>{setv('count'+i,x[0]);setv('gpio'+i,x[1]);const e=$('rev'+i);if(!e.dataset.dirty)e.checked=!!x[2]});
  const total=m.segments.reduce((a,x)=>a+x[0],0);
  const tvIds=['tvTop','tvRight','tvBottom','tvLeft'];
  m.segments.forEach((x,i)=>txt(tvIds[i],mapNames[i]+' · '+x[0]+' · GPIO'+x[1]+' · '+sideArrow(i)+' · '+(x[2]?'REV':'FWD')));
  txt('tvSummary','Активно: '+total+' LED · DDP '+(total*3)+' байт · '+sourceLabel(m.source));
  txt('topologyInfo','Всего '+total+' LED · DDP '+(total*3)+' байт · '+sourceLabel(m.source));
  const side=Number($('rangeSide').value||0),len=m.segments[side][0];
  $('rangeStart').max=Math.max(0,len-1);$('rangeCount').max=len;
}
function renderPixelMask(m,map){
  if(!$('pixelMask').children.length){
    mapNames.forEach((n,i)=>{$('pixelMask').insertAdjacentHTML('beforeend',`<div class="seg"><b>${n}</b><input id="mask${i}" type="number" min="0" step="1" placeholder="none"><span class="muted" id="maskLimit${i}"></span><span></span></div>`)});
    markDirty();
  }
  m.offsets.forEach((v,i)=>{const max=map.segments[i][0]-1;$('mask'+i).max=max;txt('maskLimit'+i,'0..'+max);setv('mask'+i,v<0?'':v)});
  txt('maskSource',sourceLabel(m.source));
}
function tofStatusText(s){
  if(s===5)return '5 · полная уверенность';
  if(s===6||s===9)return s+' · пригодна с пониженным весом';
  return s+' · исключена из обычной обработки';
}
function selectTofZone(i){selectedTofZone=i;document.querySelectorAll('.tofcell').forEach((e,n)=>e.classList.toggle('selected',n===i))}
function renderTofGrid(t,sp){
  const g=$('tofGrid');
  if(!g.children.length){
    for(let i=0;i<64;i++){
      const b=document.createElement('button');b.type='button';b.className='tofcell';b.onclick=()=>{selectTofZone(i);renderTofGrid(lastTofSnapshot,lastSpatialSnapshot)};g.appendChild(b)
    }
  }
  lastTofSnapshot=t;lastSpatialSnapshot=sp;
  const live=t.available&&t.state==='ranging';
  (t.grid||[]).forEach((x,i)=>{
    const e=g.children[i],d=x[0],st=x[1],raw=x[2];
    const usable=st===5||st===6||st===9;
    e.className='tofcell '+(!live?'rejected':(st===5?'usable':(usable?'usable weak':'rejected')))+(i===selectedTofZone?' selected':'');
    e.innerHTML='<strong>'+(d>0?d:'—')+'</strong>r'+raw+' s'+st;
    e.title=(live?'':'НЕАКТУАЛЬНЫЕ ДАННЫЕ · ')+'normalized '+Math.floor(i/8)+','+(i%8)+' · raw '+raw+' · '+d+' mm · '+tofStatusText(st);
  });
  const x=(t.grid||[])[selectedTofZone];
  if(x){
    txt('tofZoneDetail',(live?'Данные актуальны':'Данные неактуальны · '+t.state)+'\nСтрока '+Math.floor(selectedTofZone/8)+', столбец '+(selectedTofZone%8)+'\nRaw index '+x[2]+'\nРасстояние '+x[0]+' мм\nСтатус '+tofStatusText(x[1])+'\nПоворот '+(sp.rot*90)+'° · отражение '+(sp.mirror?'да':'нет'));
  }
  tofDebugUiActive=!!t.debug_active;
  txt('tofDebugState',t.debug_active?('LIVE · осталось '+Math.ceil(t.debug_remaining_ms/1000)+' с · '+(sp.rot*90)+'° · отражение '+(sp.mirror?'да':'нет')):('Обычный режим · '+(sp.rot*90)+'° · отражение '+(sp.mirror?'да':'нет')));
}
let lastTofSnapshot={grid:[]},lastSpatialSnapshot={rot:0,mirror:0};
function calibrationText(c){
  if(c.active)return 'Сбор данных · кадров '+c.samples;
  if(!c.has_summary)return 'Калибровка ещё не выполнялась.';
  const s=c.summary;
  let t='Кадров: '+s.total+'\nПлоскость: '+(s.plane_valid?'определена':'не определена');
  if(s.plane_valid)t+='\nНаклон: yaw '+(s.yaw[1]/100).toFixed(2)+'° · pitch '+(s.pitch[1]/100).toFixed(2)+'°\nРасстояние до плоскости: '+s.z0[1]+' мм';
  if(s.spatial_valid)t+='\nДиапазон стены: '+s.min[1]+'..'+s.max[1]+' мм\nСтороны: '+s.segments.map((x,i)=>mapNames[i]+' '+x[0]+'→'+x[1]+' мм').join(' · ');
  return t;
}
function tofOperationalStatus(t){
  if(!t.available)return 'НЕТ';
  if(t.state==='error')return 'ОШИБКА';
  if(t.state==='initializing'||t.state==='not-started')return 'ЗАПУСК';
  if(t.gain_fail_open)return 'БЕЗ КОРР.';
  return t.state==='ranging'?'НОРМА':String(t.state||'?').toUpperCase();
}
function commissioningText(c){
  if(!c.pattern)return 'Тест не запущен';
  const left=' · '+Math.ceil(c.remaining_ms/1000)+' с';
  if(c.pattern===1)return 'Все стороны'+left;
  if(c.pattern===2)return 'Проверка направления'+left;
  if(c.pattern===3)return (mapNames[c.side]||('side '+c.side))+' · LED '+c.start+'..'+(c.start+c.count-1)+left;
  if(c.pattern===4)return 'GPIO'+c.gpio+' · LED '+c.start+'..'+(c.start+c.count-1)+left;
  return 'Тест '+c.pattern+left;
}
function render(s){
  if(pendingActionId){
    if(s.action.id===pendingActionId){
      if(s.action.ok)cleanPendingIfUnchanged();else rollbackAutosaveDrafts();
      txt('action',s.action.msg||(s.action.ok?'ok':'failed'));$('action').className=s.action.ok?'ok':'bad';
      clearPendingAction();
    }else if(actionSequenceAfter(s.action.id,pendingActionId)){
      rollbackAutosaveDrafts();
      actionError('Результат операции вытеснен действием другого клиента. Черновик формы сохранён; текущее состояние обновлено с контроллера.');
      clearPendingAction();
    }else if(pendingActionStartedMs&&Date.now()-pendingActionStartedMs>6000){
      rollbackAutosaveDrafts();
      actionError('Контроллер не подтвердил результат операции. Черновик формы сохранён; проверьте текущее состояние.');
      clearPendingAction();
    }
  }
  txt('fw',s.fw.version+' · Stage '+s.fw.stage+' · '+s.fw.target);
  txt('stCorr',corrNames[s.output.correction]||'?');
  txt('stBright',Math.round(s.output.brightness*100/255)+'%');
  const signalFresh=s.ddp.has_frame&&s.ddp.frame_age_ms<=1000;
  txt('stDdp',!s.ddp.running?'ВЫКЛ.':(signalFresh?'ПОЛУЧАЕМ':(s.output.frame_held?'УДЕРЖАНИЕ':'ОЖИДАНИЕ')));
  txt('stTof',tofOperationalStatus(s.tof));
  [0,1,2].forEach(i=>$('corr'+i).classList.toggle('primary',s.output.correction===i));
  setv('brightness',s.output.brightness);txt('brightnessValue',brightnessLabel(s.output.brightness));
  let home='Подсветка '+(s.output.brightness===0?'выключена':'готова')+'. ';
  if(s.commissioning.pattern)home+='Пусконаладочный тест: '+commissioningText(s.commissioning)+'. ';
  else if(signalFresh)home+='Сигнал ПК поступает, последний кадр '+s.ddp.frame_age_ms+' мс назад. ';
  else if(s.output.frame_held)home+='Новых кадров нет, удерживается последний успешно показанный кадр ('+s.ddp.frame_age_ms+' мс). ';
  else home+='Ожидаем первый кадр от ПК. ';
  home+='Коррекция: '+(corrNames[s.output.correction]||'?')+'.';
  txt('homeStatus',home);
  txt('testState',commissioningText(s.commissioning));
  const testMax=Number(s.commissioning.max_brightness||0);
  const safeTest=s.output.brightness>0&&testMax>0&&s.output.brightness<=testMax;
  $('testSegments').disabled=!safeTest;$('testDirection').disabled=!safeTest;$('runLogical').disabled=!safeTest;$('runWhole').disabled=!safeTest;$('runRaw').disabled=!safeTest;
  renderMap(s.map);
  renderPixelMask(s.pixel_mask,s.map);
  $('mapApply').disabled=s.calibration.active;
  $('mapReset').disabled=s.calibration.active;
  const sp=s.spatial;
  renderTofGrid(s.tof,sp);
  $('tofDebugStart').disabled=s.output.correction===2||s.tof.debug_active;
  $('tofDebugStop').disabled=!s.tof.debug_active;
  setv('spW',sp.w10/10);setv('spH',sp.h10/10);setv('spX',sp.x10/10);setv('spY',sp.y10/10);setv('spZ',sp.z10/10);setv('spR',sp.rot);setv('spM',sp.mirror);setv('spD',sp.deadband10/10);
  txt('spSource',sourceLabel(sp.source));txt('curveSource',sourceLabel(s.curve.source));
  const spatialBlocked=s.output.correction===2||s.calibration.active;
  $('spApply').disabled=spatialBlocked;$('spReset').disabled=spatialBlocked;
  const curveBlocked=s.output.correction===2;
  $('curveApply').disabled=curveBlocked;$('curveReset').disabled=curveBlocked;
  setv('curve',s.curve.points.map(p=>p[0]+':'+gainPercent(p[1])).join(','));
  const tofLive=s.tof.available&&s.tof.state==='ranging';
  const tofGainState=!tofLive?'данные неактуальны':(s.tof.gain_fail_open?'резерв: 100%':'коррекция доступна');
  txt('tofDetail',s.tof.available?`Состояние: ${s.tof.state}\nВозраст данных: ${s.tof.age_ms} мс · пригодных зон ${s.tof.valid_zones}/64 · медиана ${s.tof.median_mm} мм\nПлоскость: ${s.tof.plane_valid?'определена':'не определена'} · yaw ${(s.tof.yaw_cdeg/100).toFixed(2)}° · pitch ${(s.tof.pitch_cdeg/100).toFixed(2)}°\nСтена: ${s.tof.min_mm}..${s.tof.max_mm} мм · ${tofGainState}`:'Датчик ToF недоступен');
  txt('calDetail',calibrationText(s.calibration));
  $('calStart').disabled=s.calibration.active||!s.tof.available||s.tof.state!=='ranging';
  $('probeStart').disabled=s.output.correction!==1||s.probe;
  $('probeStart').textContent=s.probe?'Тест модели активен':'Тест модели 10 с';
  const wifiMode=!s.wifi.enabled?'Wi-Fi выключен':(s.wifi.connected?'Подключено':'Не подключено');
  const wifiRssi=s.wifi.connected?(' · RSSI '+s.wifi.rssi+' dBm'):'';
  txt('wifiState',wifiMode+' · '+(s.wifi.ip||'без IP')+wifiRssi+' · '+(s.wifi.ssid||'SSID не задан'));
  setv('ssid',s.wifi.ssid||'');
  txt('diag',`DDP: running=${s.ddp.running} frames=${s.ddp.frames} publications=${s.ddp.publications}\nlast frame: ${s.ddp.has_frame?s.ddp.frame_age_ms+' ms':'none'} · frame hold=${s.output.frame_held}\nsender: ${s.ddp.sender_locked?(s.ddp.sender_ip+':'+s.ddp.sender_port):'none'}\nToF: state=${s.tof.state} age=${s.tof.age_ms} ms valid=${s.tof.valid_zones}/64 plane=${s.tof.plane_valid}\npersistence: ${s.persistence?'available':'unavailable'}\nheap free/min: ${s.heap.free}/${s.heap.min} B\nweb requests/actions/dropped/bad: ${s.web.requests}/${s.web.actions}/${s.web.dropped}/${s.web.bad}`);
  $('factory').disabled=s.output.brightness!==0;
}
async function refresh(){
  if(refreshing)return;refreshing=true;
  try{
    const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw new Error('status HTTP '+r.status);
    const wasOffline=offlineBanner;const s=await r.json();offlineBanner=false;render(s);
    if(wasOffline&&$('action').textContent.startsWith('offline:')){txt('action','Связь восстановлена.');$('action').className='ok'}
  }
  catch(e){offlineBanner=true;txt('action','offline: '+e.message);$('action').className='bad'}
  finally{refreshing=false}
}
$('brightness').addEventListener('input',e=>txt('brightnessValue',brightnessLabel(e.target.value)));
$('brightness').addEventListener('change',async e=>{if(!await post('/api/brightness',e.target.value,['brightness'])){clean(['brightness']);refresh()}});
$('wifiOpen').addEventListener('change',e=>{if(e.target.checked){$('wifiPass').value='';delete $('wifiPass').dataset.dirty}$('wifiPass').disabled=e.target.checked});
markDirty();refresh();setInterval(()=>{if(tofDebugUiActive)refresh()},1000);setInterval(()=>{if(!tofDebugUiActive)refresh()},2000);
</script>
</body>
</html>)HTML";

constexpr char kBadRequestResponse[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"bad request\"}";

constexpr char kForbiddenResponse[] =
    "HTTP/1.1 403 Forbidden\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"control header required\"}";

constexpr char kNotFoundResponse[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"not found\"}";

constexpr char kMethodResponse[] =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"method not allowed\"}";

constexpr char kTooLargeResponse[] =
    "HTTP/1.1 413 Payload Too Large\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"payload too large\"}";

constexpr char kServiceUnavailableResponse[] =
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"status unavailable\"}";

constexpr char kInternalErrorResponse[] =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: application/json\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n"
    "{\"error\":\"response overflow\"}";

const char* boolJson(bool value) {
    return value
        ? "true"
        : "false";
}

const char* sourceName(
    bool customized,
    bool persisted) {

    if (!customized) {
        return "DEFAULT";
    }

    return persisted
        ? "CUSTOM_NVS"
        : "CUSTOM_RUNTIME";
}

class BufferWriter {
public:
    BufferWriter(
        char* data,
        std::size_t capacity)
        : data_(data),
          capacity_(capacity) {

        if (capacity_ > 0) {
            data_[0] = '\0';
        }
    }

    bool append(
        const char* text) {

        if (text == nullptr) {
            return false;
        }

        const std::size_t length =
            std::strlen(text);

        if (!reserve(length)) {
            return false;
        }

        std::memcpy(
            data_ + length_,
            text,
            length);

        length_ += length;
        data_[length_] = '\0';
        return true;
    }

    bool appendf(
        const char* format,
        ...) {

        if (!ok_ ||
            format == nullptr ||
            length_ >= capacity_) {

            ok_ = false;
            return false;
        }

        va_list args;
        va_start(args, format);

        const int written =
            std::vsnprintf(
                data_ + length_,
                capacity_ - length_,
                format,
                args);

        va_end(args);

        if (written < 0 ||
            static_cast<std::size_t>(written) >=
                capacity_ - length_) {

            ok_ = false;
            return false;
        }

        length_ +=
            static_cast<std::size_t>(
                written);

        return true;
    }

    bool appendJsonString(
        const char* text) {

        if (!append("\"")) {
            return false;
        }

        if (text != nullptr) {
            static constexpr char kHex[] =
                "0123456789ABCDEF";

            for (std::size_t index = 0;
                 text[index] != '\0';
                 ++index) {

                const unsigned char value =
                    static_cast<unsigned char>(
                        text[index]);

                if (value == '\"' ||
                    value == '\\') {

                    char escaped[3] = {
                        '\\',
                        static_cast<char>(value),
                        '\0'
                    };

                    if (!append(escaped)) {
                        return false;
                    }

                    continue;
                }

                if (value < 0x20U) {
                    char escaped[7] = {
                        '\\',
                        'u',
                        '0',
                        '0',
                        kHex[
                            (value >> 4) &
                            0x0FU],
                        kHex[
                            value &
                            0x0FU],
                        '\0'
                    };

                    if (!append(escaped)) {
                        return false;
                    }

                    continue;
                }

                if (!reserve(1)) {
                    return false;
                }

                data_[length_++] =
                    static_cast<char>(
                        value);

                data_[length_] = '\0';
            }
        }

        return append("\"");
    }

    bool ok() const {
        return ok_;
    }

    std::size_t length() const {
        return length_;
    }

private:
    bool reserve(
        std::size_t extra) {

        if (!ok_ ||
            capacity_ == 0 ||
            length_ >= capacity_ ||
            extra >
                capacity_ -
                    length_ -
                    1) {

            ok_ = false;
            return false;
        }

        return true;
    }

    char* data_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t length_ = 0;
    bool ok_ = true;
};

} // namespace

WebUiService::~WebUiService() {
    stop();
}

bool WebUiService::setNonBlocking(
    int socket) {

    const int flags =
        fcntl(
            socket,
            F_GETFL,
            0);

    if (flags < 0) {
        return false;
    }

    return
        fcntl(
            socket,
            F_SETFL,
            flags | O_NONBLOCK) == 0;
}

bool WebUiService::begin() {
    if (listenSocket_ >= 0) {
        return true;
    }

    listenSocket_ =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP);

    if (listenSocket_ < 0) {
        ++stats_.startFailures;
        return false;
    }

#ifdef SO_REUSEADDR
    const int reuse = 1;
    setsockopt(
        listenSocket_,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse));
#endif

    if (!setNonBlocking(
            listenSocket_)) {

        close(listenSocket_);
        listenSocket_ = -1;
        ++stats_.startFailures;
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port =
        htons(kPort);

    address.sin_addr.s_addr =
        htonl(INADDR_ANY);

    if (bind(
            listenSocket_,
            reinterpret_cast<
                const sockaddr*>(
                    &address),
            sizeof(address)) != 0 ||
        listen(
            listenSocket_,
            2) != 0) {

        close(listenSocket_);
        listenSocket_ = -1;
        ++stats_.startFailures;
        return false;
    }

    ++stats_.starts;
    return true;
}

void WebUiService::stop() {
    closeClient();
    pendingAction_ = {};

    if (listenSocket_ >= 0) {
        close(listenSocket_);
        listenSocket_ = -1;
    }
}

void WebUiService::resetRequest() {
    requestLength_ = 0;
    requestBuffer_[0] = '\0';

    responseData_ = nullptr;
    responseLength_ = 0;
    responseOffset_ = 0;
}

void WebUiService::closeClient() {
    if (clientSocket_ >= 0) {
        close(clientSocket_);
        clientSocket_ = -1;
    }

    clientLastActivityUs_ = 0;
    resetRequest();
}

bool WebUiService::acceptClient(
    std::uint64_t nowUs) {

    sockaddr_in remote{};
    socklen_t remoteLength =
        sizeof(remote);

    const int accepted =
        accept(
            listenSocket_,
            reinterpret_cast<
                sockaddr*>(
                    &remote),
            &remoteLength);

    if (accepted < 0) {
        return false;
    }

    if (!setNonBlocking(
            accepted)) {

        close(accepted);
        return false;
    }

    clientSocket_ = accepted;
    clientLastActivityUs_ = nowUs;
    resetRequest();

    ++stats_.connections;
    return true;
}

void WebUiService::selectStaticResponse(
    const char* response,
    std::size_t length) {

    responseData_ = response;
    responseLength_ = length;
    responseOffset_ = 0;
}

void WebUiService::selectDynamicResponse(
    std::size_t length) {

    responseData_ =
        dynamicResponse_.data();

    responseLength_ =
        length;

    responseOffset_ = 0;
}

void WebUiService::selectErrorResponse(
    WebUiParseResult result) {

    switch (result) {
    case WebUiParseResult::Forbidden:
        ++stats_.forbiddenRequests;
        selectStaticResponse(
            kForbiddenResponse,
            sizeof(kForbiddenResponse) - 1);
        break;

    case WebUiParseResult::UnknownRoute:
        ++stats_.notFoundRequests;
        selectStaticResponse(
            kNotFoundResponse,
            sizeof(kNotFoundResponse) - 1);
        break;

    case WebUiParseResult::MethodNotAllowed:
        ++stats_.badRequests;
        selectStaticResponse(
            kMethodResponse,
            sizeof(kMethodResponse) - 1);
        break;

    case WebUiParseResult::PayloadTooLarge:
        ++stats_.badRequests;
        selectStaticResponse(
            kTooLargeResponse,
            sizeof(kTooLargeResponse) - 1);
        break;

    default:
        ++stats_.badRequests;
        selectStaticResponse(
            kBadRequestResponse,
            sizeof(kBadRequestResponse) - 1);
        break;
    }
}

bool WebUiService::buildQueuedResponse(
    std::uint32_t sequence) {

    BufferWriter writer(
        dynamicResponse_.data(),
        dynamicResponse_.size());

    writer.append(
        "HTTP/1.1 202 Accepted\r\n"
        "Content-Type: application/json\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n");

    writer.appendf(
        "{\"queued\":%lu}",
        static_cast<unsigned long>(
            sequence));

    if (!writer.ok()) {
        return false;
    }

    selectDynamicResponse(
        writer.length());

    return true;
}

bool WebUiService::buildStatusResponse(
    const WebUiSnapshot& snapshot) {

    BufferWriter writer(
        dynamicResponse_.data(),
        dynamicResponse_.size());

    writer.append(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Connection: close\r\n\r\n");

    writer.append(
        "{\"fw\":{\"version\":");

    writer.appendJsonString(
        config::kFirmwareVersion);

    writer.append(
        ",\"stage\":");

    writer.appendf(
        "%u",
        static_cast<unsigned>(
            config::kDevelopmentStage));

    writer.append(
        ",\"target\":");

    writer.appendJsonString(
        config::kFirmwareTarget);

    writer.append(
        "},\"persistence\":");

    writer.append(
        boolJson(
            snapshot.persistenceAvailable));

    writer.append(
        ",\"output\":{");

    writer.appendf(
        "\"brightness\":%u,"
        "\"correction\":%u,"
        "\"frame_held\":%s",
        static_cast<unsigned>(
            snapshot.brightness),
        static_cast<unsigned>(
            snapshot.correctionMode),
        boolJson(
            snapshot.outputFrameHeld));

    writer.append(
        "},\"wifi\":{");

    writer.appendf(
        "\"enabled\":%s,"
        "\"connected\":%s,"
        "\"rssi\":%ld,"
        "\"ssid\":",
        boolJson(snapshot.wifiEnabled),
        boolJson(snapshot.wifiConnected),
        static_cast<long>(
            snapshot.wifiRssi));

    writer.appendJsonString(
        snapshot.wifiSsid.data());

    writer.append(
        ",\"ip\":");

    writer.appendJsonString(
        snapshot.wifiIp.data());

    writer.append(
        "},\"ddp\":{");

    writer.appendf(
        "\"running\":%s,"
        "\"has_frame\":%s,"
        "\"frame_age_ms\":%llu,"
        "\"frames\":%lu,"
        "\"publications\":%lu,"
        "\"sender_locked\":%s,"
        "\"sender_ip\":",
        boolJson(snapshot.ddpRunning),
        boolJson(snapshot.ddpHasFrame),
        static_cast<unsigned long long>(
            snapshot.ddpFrameAgeMs),
        static_cast<unsigned long>(
            snapshot.ddpCompleteFrames),
        static_cast<unsigned long>(
            snapshot.ddpPublications),
        boolJson(snapshot.senderLocked));

    writer.appendJsonString(
        snapshot.senderIp.data());

    writer.appendf(
        ",\"sender_port\":%u}",
        static_cast<unsigned>(
            snapshot.senderPort));

    writer.append(
        ",\"tof\":{");

    writer.appendf(
        "\"available\":%s,"
        "\"generation\":%lu,"
        "\"age_ms\":%llu,"
        "\"valid_zones\":%u,"
        "\"median_mm\":%u,"
        "\"plane_valid\":%s,"
        "\"yaw_cdeg\":%d,"
        "\"pitch_cdeg\":%d,"
        "\"plane_accepted\":%u,"
        "\"gain_fail_open\":%s,"
        "\"min_mm\":%u,"
        "\"max_mm\":%u,"
        "\"state\":",
        boolJson(snapshot.tofAvailable),
        static_cast<unsigned long>(
            snapshot.tofGeneration),
        static_cast<unsigned long long>(
            snapshot.tofAgeMs),
        static_cast<unsigned>(
            snapshot.tofValidZones),
        static_cast<unsigned>(
            snapshot.tofMedianMm),
        boolJson(snapshot.planeValid),
        static_cast<int>(
            snapshot.planeYawCentiDeg),
        static_cast<int>(
            snapshot.planePitchCentiDeg),
        static_cast<unsigned>(
            snapshot.planeAccepted),
        boolJson(
            snapshot.perimeterFailOpen),
        static_cast<unsigned>(
            snapshot.perimeterMinMm),
        static_cast<unsigned>(
            snapshot.perimeterMaxMm));

    writer.appendJsonString(
        snapshot.tofState.data());

    writer.appendf(
        ",\"debug_active\":%s,"
        "\"debug_remaining_ms\":%lu,"
        "\"grid\":[",
        boolJson(
            snapshot.tofDebugActive),
        static_cast<unsigned long>(
            snapshot.tofDebugRemainingMs));

    for (std::size_t index = 0;
         index <
            snapshot
                .tofNormalizedDistanceMm
                .size();
         ++index) {

        if (index != 0) {
            writer.append(",");
        }

        writer.appendf(
            "[%d,%u,%u]",
            static_cast<int>(
                snapshot
                    .tofNormalizedDistanceMm[
                        index]),
            static_cast<unsigned>(
                snapshot
                    .tofNormalizedStatus[
                        index]),
            static_cast<unsigned>(
                snapshot
                    .tofNormalizedRawIndex[
                        index]));
    }

    writer.append("]}");

    const auto& spatial =
        snapshot.spatialProfile;

    writer.append(
        ",\"spatial\":{");

    writer.appendf(
        "\"w10\":%u,"
        "\"h10\":%u,"
        "\"x10\":%d,"
        "\"y10\":%d,"
        "\"z10\":%d,"
        "\"rot\":%u,"
        "\"mirror\":%u,"
        "\"deadband10\":%u,"
        "\"source\":",
        static_cast<unsigned>(
            spatial.widthMmX10),
        static_cast<unsigned>(
            spatial.heightMmX10),
        static_cast<int>(
            spatial.sensorOffsetXmmX10),
        static_cast<int>(
            spatial.sensorOffsetYmmX10),
        static_cast<int>(
            spatial.ledPlaneZmmX10),
        static_cast<unsigned>(
            spatial.rotationQuarterTurns),
        static_cast<unsigned>(
            spatial.mirrorX),
        static_cast<unsigned>(
            spatial.planeDeadbandMmX10));

    writer.appendJsonString(
        sourceName(
            snapshot.spatialCustomized,
            snapshot.spatialPersisted));

    writer.append(
        "},\"curve\":{\"source\":");

    writer.appendJsonString(
        sourceName(
            snapshot.gainCustomized,
            snapshot.gainPersisted));

    writer.append(
        ",\"points\":[");

    for (std::size_t index = 0;
         index <
            snapshot.gainPointCount &&
         index <
            snapshot.gainPoints.size();
         ++index) {

        if (index != 0) {
            writer.append(",");
        }

        writer.appendf(
            "[%u,%u]",
            static_cast<unsigned>(
                snapshot
                    .gainPoints[index]
                    .distanceMm),
            static_cast<unsigned>(
                snapshot
                    .gainPoints[index]
                    .gainQ12));
    }

    writer.append(
        "]},\"map\":{\"source\":");

    writer.appendJsonString(
        sourceName(
            snapshot.ledMappingCustomized,
            snapshot.ledMappingPersisted));

    writer.append(
        ",\"segments\":[");

    for (std::size_t index = 0;
         index <
            snapshot
                .ledMapping
                .segment
                .size();
         ++index) {

        if (index != 0) {
            writer.append(",");
        }

        const auto& mapping =
            snapshot
                .ledMapping
                .segment[index];

        writer.appendf(
            "[%u,%u,%u]",
            static_cast<unsigned>(
                mapping.logicalLength),
            static_cast<unsigned>(
                config::kLedGpios[
                    mapping.lane]),
            static_cast<unsigned>(
                mapping.reversed));
    }

    writer.append("]}");

    writer.append(
        ",\"pixel_mask\":{\"source\":");

    writer.appendJsonString(
        sourceName(
            snapshot.ledPixelMaskCustomized,
            snapshot.ledPixelMaskPersisted));

    writer.append(
        ",\"offsets\":[");

    for (std::size_t index = 0;
         index <
            snapshot
                .ledPixelMask
                .disabledOffset
                .size();
         ++index) {

        if (index != 0) {
            writer.append(",");
        }

        const std::uint16_t value =
            snapshot
                .ledPixelMask
                .disabledOffset[index];

        if (value ==
            LedPixelMaskProfile::kNone) {

            writer.append("-1");
        } else {
            writer.appendf(
                "%u",
                static_cast<unsigned>(
                    value));
        }
    }

    writer.append("]}");

    writer.appendf(
        ",\"commissioning\":{"
        "\"pattern\":%u,"
        "\"remaining_ms\":%lu,"
        "\"max_brightness\":%u,"
        "\"side\":%u,"
        "\"gpio\":%u,"
        "\"start\":%u,"
        "\"count\":%u}",
        static_cast<unsigned>(
            snapshot.commissioningPattern),
        static_cast<unsigned long>(
            snapshot
                .commissioningRemainingMs),
        static_cast<unsigned>(
            snapshot
                .commissioningMaxBrightness),
        static_cast<unsigned>(
            snapshot.commissioningSide),
        static_cast<unsigned>(
            snapshot.commissioningGpio),
        static_cast<unsigned>(
            snapshot
                .commissioningRangeStart),
        static_cast<unsigned>(
            snapshot
                .commissioningRangeCount));

    writer.appendf(
        ",\"calibration\":{"
        "\"active\":%s,"
        "\"samples\":%lu,"
        "\"has_summary\":%s,"
        "\"summary\":{",
        boolJson(
            snapshot.calibrationActive),
        static_cast<unsigned long>(
            snapshot.calibrationSamples),
        boolJson(
            snapshot
                .calibrationSummaryAvailable));

    const auto& calibration =
        snapshot.calibrationSummary;

    writer.appendf(
        "\"total\":%lu,"
        "\"plane_valid\":%s,"
        "\"yaw\":[%d,%d,%d],"
        "\"pitch\":[%d,%d,%d],"
        "\"z0\":[%u,%u,%u],"
        "\"spatial_valid\":%s,"
        "\"min\":[%u,%u,%u],"
        "\"max\":[%u,%u,%u],"
        "\"segments\":[",
        static_cast<unsigned long>(
            calibration.totalFrames),
        boolJson(
            calibration.plane.valid),
        static_cast<int>(
            calibration
                .plane
                .yawCentiDeg
                .p10),
        static_cast<int>(
            calibration
                .plane
                .yawCentiDeg
                .median),
        static_cast<int>(
            calibration
                .plane
                .yawCentiDeg
                .p90),
        static_cast<int>(
            calibration
                .plane
                .pitchCentiDeg
                .p10),
        static_cast<int>(
            calibration
                .plane
                .pitchCentiDeg
                .median),
        static_cast<int>(
            calibration
                .plane
                .pitchCentiDeg
                .p90),
        static_cast<unsigned>(
            calibration
                .plane
                .interceptMm
                .p10),
        static_cast<unsigned>(
            calibration
                .plane
                .interceptMm
                .median),
        static_cast<unsigned>(
            calibration
                .plane
                .interceptMm
                .p90),
        boolJson(
            calibration.spatial.valid),
        static_cast<unsigned>(
            calibration
                .spatial
                .minDistanceMm
                .p10),
        static_cast<unsigned>(
            calibration
                .spatial
                .minDistanceMm
                .median),
        static_cast<unsigned>(
            calibration
                .spatial
                .minDistanceMm
                .p90),
        static_cast<unsigned>(
            calibration
                .spatial
                .maxDistanceMm
                .p10),
        static_cast<unsigned>(
            calibration
                .spatial
                .maxDistanceMm
                .median),
        static_cast<unsigned>(
            calibration
                .spatial
                .maxDistanceMm
                .p90));

    for (std::size_t index = 0;
         index <
            calibration
                .spatial
                .segment
                .size();
         ++index) {

        if (index != 0) {
            writer.append(",");
        }

        const auto& segment =
            calibration
                .spatial
                .segment[index];

        writer.appendf(
            "[%u,%u]",
            static_cast<unsigned>(
                segment
                    .startMm
                    .median),
            static_cast<unsigned>(
                segment
                    .endMm
                    .median));
    }

    writer.append(
        "]}}");

    writer.appendf(
        ",\"probe\":%s,"
        "\"action\":{"
        "\"id\":%lu,"
        "\"ok\":%s,"
        "\"msg\":",
        boolJson(
            snapshot.shadowProbeActive),
        static_cast<unsigned long>(
            snapshot.lastActionSequence),
        boolJson(
            snapshot.lastActionOk));

    writer.appendJsonString(
        snapshot
            .lastActionMessage
            .data());

    writer.appendf(
        "},\"heap\":{"
        "\"free\":%lu,"
        "\"min\":%lu},"
        "\"web\":{"
        "\"requests\":%lu,"
        "\"actions\":%lu,"
        "\"dropped\":%lu,"
        "\"bad\":%lu}}",
        static_cast<unsigned long>(
            snapshot.freeHeapBytes),
        static_cast<unsigned long>(
            snapshot.minFreeHeapBytes),
        static_cast<unsigned long>(
            stats_.requests),
        static_cast<unsigned long>(
            stats_.actionsQueued),
        static_cast<unsigned long>(
            stats_.actionsDropped),
        static_cast<unsigned long>(
            stats_.badRequests +
            stats_.forbiddenRequests +
            stats_.notFoundRequests));

    if (!writer.ok()) {
        return false;
    }

    selectDynamicResponse(
        writer.length());

    return true;
}

WebUiActionEvent WebUiService::receiveStep(
    WebUiSnapshotProvider snapshotProvider,
    std::uint64_t nowUs) {

    WebUiActionEvent event;

    const std::size_t remaining =
        kRequestBufferBytes -
        requestLength_;

    if (remaining == 0) {
        ++stats_.requests;
        selectErrorResponse(
            WebUiParseResult::
                PayloadTooLarge);

        return event;
    }

    const std::size_t receiveLength =
        std::min<std::size_t>(
            remaining,
            kReceiveChunkBytes);

    const int received =
        recv(
            clientSocket_,
            requestBuffer_.data() +
                requestLength_,
            receiveLength,
            MSG_DONTWAIT);

    if (received < 0) {
        if (errno == EWOULDBLOCK ||
            errno == EAGAIN) {

            return event;
        }

        ++stats_.receiveErrors;
        closeClient();
        return event;
    }

    if (received == 0) {
        closeClient();
        return event;
    }

    requestLength_ +=
        static_cast<std::size_t>(
            received);

    requestBuffer_[
        requestLength_] = '\0';

    clientLastActivityUs_ = nowUs;

    WebUiHttpRequest request;

    const WebUiParseResult parseResult =
        WebUiProtocol::parse(
            requestBuffer_.data(),
            requestLength_,
            request);

    if (parseResult ==
        WebUiParseResult::Incomplete) {

        if (requestLength_ ==
            kRequestBufferBytes) {

            ++stats_.requests;

            selectErrorResponse(
                WebUiParseResult::
                    PayloadTooLarge);
        }

        return event;
    }

    ++stats_.requests;

    if (parseResult !=
        WebUiParseResult::Ok) {

        selectErrorResponse(
            parseResult);

        return event;
    }

    if (request.route ==
        WebUiRoute::Index) {

        selectStaticResponse(
            kIndexResponse,
            sizeof(kIndexResponse) - 1);

        return event;
    }

    if (request.route ==
        WebUiRoute::Status) {

        WebUiSnapshot snapshot;

        if (snapshotProvider == nullptr ||
            !snapshotProvider(
                snapshot)) {

            selectStaticResponse(
                kServiceUnavailableResponse,
                sizeof(
                    kServiceUnavailableResponse) -
                    1);

            return event;
        }

        if (!buildStatusResponse(
                snapshot)) {

            selectStaticResponse(
                kInternalErrorResponse,
                sizeof(
                    kInternalErrorResponse) -
                    1);
        }

        return event;
    }

    ++nextActionSequence_;

    if (nextActionSequence_ == 0) {
        ++nextActionSequence_;
    }

    event.sequence =
        nextActionSequence_;

    event.kind =
        request.action;

    if (request.bodyLength > 0) {
        std::memcpy(
            event.payload.data(),
            requestBuffer_.data() +
                request.bodyOffset,
            request.bodyLength);
    }

    event.payload[
        request.bodyLength] = '\0';

    if (!buildQueuedResponse(
            event.sequence)) {

        event = {};

        selectStaticResponse(
            kInternalErrorResponse,
            sizeof(
                kInternalErrorResponse) -
                1);

        return event;
    }

    pendingAction_ = event;
    ++stats_.actionsQueued;

    return {};
}

void WebUiService::sendStep(
    std::uint64_t nowUs) {

    if (responseData_ == nullptr ||
        responseOffset_ >=
            responseLength_) {

        closeClient();
        return;
    }

    const std::size_t remaining =
        responseLength_ -
        responseOffset_;

    const std::size_t chunk =
        std::min<std::size_t>(
            remaining,
            kSendChunkBytes);

    const int sent =
        send(
            clientSocket_,
            responseData_ +
                responseOffset_,
            chunk,
            MSG_DONTWAIT);

    if (sent < 0) {
        if (errno == EWOULDBLOCK ||
            errno == EAGAIN) {

            return;
        }

        ++stats_.sendErrors;

        if (pendingAction_.ready()) {
            pendingAction_ = {};
            ++stats_.actionsDropped;
        }

        closeClient();
        return;
    }

    if (sent == 0) {
        if (pendingAction_.ready()) {
            pendingAction_ = {};
            ++stats_.actionsDropped;
        }

        closeClient();
        return;
    }

    responseOffset_ +=
        static_cast<std::size_t>(
            sent);

    clientLastActivityUs_ = nowUs;

    if (responseOffset_ >=
        responseLength_) {

        closeClient();
    }
}

WebUiActionEvent WebUiService::poll(
    WebUiSnapshotProvider snapshotProvider,
    std::uint64_t nowUs) {

    WebUiActionEvent event;

    if (pendingAction_.ready() &&
        clientSocket_ < 0) {

        event = pendingAction_;
        pendingAction_ = {};
        return event;
    }

    if (listenSocket_ < 0) {
        return event;
    }

    if (clientSocket_ < 0) {
        acceptClient(
            nowUs);

        return event;
    }

    if (nowUs >=
            clientLastActivityUs_ &&
        nowUs -
            clientLastActivityUs_ >
                kClientIdleTimeoutUs) {

        ++stats_.clientTimeouts;

        if (pendingAction_.ready()) {
            pendingAction_ = {};
            ++stats_.actionsDropped;
        }

        closeClient();
        return event;
    }

    if (responseData_ != nullptr) {
        sendStep(
            nowUs);

        return event;
    }

    return receiveStep(
        snapshotProvider,
        nowUs);
}

} // namespace ambilight
