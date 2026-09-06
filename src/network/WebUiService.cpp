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
:root{color-scheme:light dark;--bg:#f4f5f7;--card:#fff;--text:#17181a;--muted:#737780;--line:#d9dce2;--accent:#4263eb;--danger:#c92a2a;--ok:#2b8a3e}
@media(prefers-color-scheme:dark){:root{--bg:#111315;--card:#1a1d20;--text:#f3f4f5;--muted:#9aa0a8;--line:#30343a;--accent:#748ffc;--danger:#ff6b6b;--ok:#69db7c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.4 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:920px;margin:auto;padding:24px 16px 64px}header{display:flex;justify-content:space-between;gap:16px;align-items:flex-end;margin-bottom:18px}
h1{font-size:22px;margin:0}h2{font-size:15px;margin:0 0 12px}.muted{color:var(--muted)}.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
.cards,.cols{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.cols{grid-template-columns:repeat(2,1fr)}
.card,details{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px}.card b{display:block;font-size:18px;margin-top:4px}
details{margin-top:10px}summary{cursor:pointer;font-weight:650;user-select:none}.section{padding-top:14px}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
label{color:var(--muted);font-size:12px}input,select,textarea,button{font:inherit;border:1px solid var(--line);border-radius:8px;background:var(--card);color:var(--text);padding:8px 10px}
input[type=number]{width:105px}input[type=range]{padding:0;width:min(360px,70vw)}input[type=text],input[type=password]{min-width:220px;flex:1}
textarea{width:100%;min-height:62px;resize:vertical;font-family:ui-monospace,SFMono-Regular,Consolas,monospace}
button{cursor:pointer}button.primary{background:var(--accent);color:#fff;border-color:transparent}button.danger{color:var(--danger)}button:disabled{opacity:.45;cursor:not-allowed}
.seg{display:grid;grid-template-columns:90px 90px 110px;gap:8px;align-items:center;margin:6px 0}.spatial{display:grid;grid-template-columns:repeat(4,minmax(115px,1fr));gap:8px}.field{display:flex;flex-direction:column;gap:4px}
#action{min-height:20px;margin:12px 0}.ok{color:var(--ok)}.bad{color:var(--danger)}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:var(--bg);padding:10px;border-radius:8px;border:1px solid var(--line)}
@media(max-width:680px){.cards{grid-template-columns:repeat(2,1fr)}.cols,.spatial{grid-template-columns:1fr 1fr}.seg{grid-template-columns:70px 1fr 90px}header{align-items:flex-start;flex-direction:column}}
</style>
</head>
<body>
<main>
<header><div><h1>Ambilight C6</h1><div id="fw" class="muted mono">connecting…</div></div><div class="muted">LAN control · HTTP/80</div></header>

<section class="cards">
<div class="card"><span class="muted">Correction</span><b id="stCorr">—</b></div>
<div class="card"><span class="muted">Brightness</span><b id="stBright">—</b></div>
<div class="card"><span class="muted">DDP</span><b id="stDdp">—</b></div>
<div class="card"><span class="muted">ToF</span><b id="stTof">—</b></div>
</section>
<div id="action" class="muted"></div>

<details open>
<summary>Основное</summary>
<div class="section">
<div class="row">
<button onclick="post('/api/correction','0')">Disabled</button>
<button onclick="post('/api/correction','1')">Shadow</button>
<button class="primary" onclick="post('/api/correction','2')">Active</button>
</div>
<div class="row">
<label>Brightness</label>
<input id="brightness" type="range" min="0" max="255" value="32">
<span id="brightnessValue" class="mono">32</span>
</div>
<div class="muted">Изменение яркости сохраняется при отпускании ползунка.</div>
</div>
</details>

<details>
<summary>LED commissioning</summary>
<div class="section cols">
<div>
<h2>Тест</h2>
<div class="row">
<button id="testSegments" onclick="post('/api/test','1')">Segments</button>
<button id="testDirection" onclick="post('/api/test','2')">Direction</button>
<button onclick="post('/api/test','0')">Stop</button>
</div>
<div id="testState" class="muted"></div>
</div>
<div>
<h2>Mapping</h2>
<div id="mapping"></div>
<div class="row">
<button id="mapApply" class="primary" onclick="applyMap()">Apply</button>
<button id="mapReset" onclick="post('/api/led-map','reset')">Default</button>
</div>
<div class="muted">Mapping меняется только при brightness=0.</div>
</div>
</div>
</details>

<details>
<summary>ToF geometry & correction</summary>
<div class="section">
<h2>Spatial profile</h2>
<div class="spatial">
<div class="field"><label>Width mm</label><input id="spW" type="number" step=".1"></div>
<div class="field"><label>Height mm</label><input id="spH" type="number" step=".1"></div>
<div class="field"><label>Sensor X mm</label><input id="spX" type="number" step=".1"></div>
<div class="field"><label>Sensor Y mm</label><input id="spY" type="number" step=".1"></div>
<div class="field"><label>LED Z mm</label><input id="spZ" type="number" step=".1"></div>
<div class="field"><label>Rotation 0..3</label><input id="spR" type="number" min="0" max="3" step="1"></div>
<div class="field"><label>Mirror X</label><select id="spM"><option value="0">No</option><option value="1">Yes</option></select></div>
<div class="field"><label>Deadband mm</label><input id="spD" type="number" step=".1"></div>
</div>
<div class="row">
<button id="spApply" class="primary" onclick="applySpatial()">Apply spatial</button>
<button id="spReset" onclick="post('/api/spatial','reset')">Default</button>
<span id="spSource" class="muted"></span>
</div>

<h2>Distance → gain Q12</h2>
<textarea id="curve" spellcheck="false" placeholder="50:2048,500:3072,4000:4096"></textarea>
<div class="row">
<button id="curveApply" class="primary" onclick="applyCurve()">Apply curve</button>
<button id="curveReset" onclick="post('/api/curve','reset')">Neutral default</button>
<span id="curveSource" class="muted"></span>
</div>

<div class="cols">
<div>
<h2>Live ToF</h2>
<pre id="tofDetail">—</pre>
</div>
<div>
<h2>Calibration</h2>
<div class="row">
<button id="calStart" onclick="post('/api/calibration','start')">Start 60 s capture</button>
<button id="probeStart" onclick="post('/api/shadow-probe','start')">Shadow probe 10 s</button>
</div>
<pre id="calDetail">No capture yet.</pre>
</div>
</div>
<div class="muted">Spatial/curve changes are refused in ACTIVE. Calibration is observational.</div>
</div>
</details>

<details>
<summary>Сеть</summary>
<div class="section">
<div id="wifiState" class="muted"></div>
<div class="row"><input id="ssid" type="text" maxlength="32" placeholder="SSID"><input id="wifiPass" type="password" maxlength="63" placeholder="Password"></div>
<div class="row">
<button class="primary" onclick="applyWifi()">Save & reconnect</button>
<button onclick="post('/api/wifi','clear')">Clear NVS Wi-Fi</button>
</div>
<div class="muted">Пароль никогда не возвращается браузеру. После смены сети эта страница может потерять соединение.</div>
</div>
</details>

<details>
<summary>Диагностика и восстановление</summary>
<div class="section">
<pre id="diag">—</pre>
<div class="row">
<button id="factory" class="danger" onclick="factoryReset()">Factory reset</button>
</div>
<div class="muted">Factory reset разрешён только при brightness=0. Web UI не имеет отдельной аутентификации: не публикуйте TCP/80 наружу.</div>
</div>
</details>
</main>
<script>
const $=id=>document.getElementById(id);
let lastAction=0;
const corrNames=['DISABLED','SHADOW','ACTIVE'];
const mapNames=['TOP','RIGHT','BOTTOM','LEFT'];
function txt(id,v){$(id).textContent=v}
function setv(id,v){const e=$(id);if(!e.dataset.dirty)e.value=v}
function clean(ids){ids.forEach(id=>{delete $(id).dataset.dirty})}
function markDirty(){document.querySelectorAll('input,select,textarea').forEach(e=>e.addEventListener('input',()=>e.dataset.dirty='1'))}
function src(custom,persisted){return custom?(persisted?'CUSTOM_NVS':'CUSTOM_RUNTIME'):'DEFAULT'}
async function post(path,body){
  txt('action','sending…');$('action').className='muted';
  try{
    const r=await fetch(path,{method:'POST',headers:{'X-Ambilight-Control':'1'},body:String(body)});
    const j=await r.json();
    if(!r.ok)throw new Error(j.error||('HTTP '+r.status));
    lastAction=j.queued||lastAction;
    setTimeout(refresh,180);
  }catch(e){txt('action',e.message);$('action').className='bad'}
}
function applyMap(){
  const p=[];
  for(let i=0;i<4;i++)p.push($('lane'+i).value+':'+($('rev'+i).checked?'1':'0'));
  clean([...Array(4)].flatMap((_,i)=>['lane'+i,'rev'+i]));
  post('/api/led-map',p.join(','));
}
function f1(v){const n=Number(v);return Number.isFinite(n)?n.toFixed(1):'0.0'}
function applySpatial(){
  const ids=['spW','spH','spX','spY','spZ','spR','spM','spD'];
  const p=[f1($('spW').value),f1($('spH').value),f1($('spX').value),f1($('spY').value),f1($('spZ').value),$('spR').value,$('spM').value,f1($('spD').value)];
  clean(ids);post('/api/spatial',p.join(','));
}
function applyCurve(){clean(['curve']);post('/api/curve',$('curve').value.trim())}
function applyWifi(){post('/api/wifi',$('ssid').value+'|'+$('wifiPass').value)}
function factoryReset(){if(confirm('Стереть всю конфигурацию Ambilight и перезагрузить контроллер?'))post('/api/factory','reset')}
function renderMap(m){
  if(!$('mapping').children.length){
    mapNames.forEach((n,i)=>{$('mapping').insertAdjacentHTML('beforeend',`<div class="seg"><b>${n}</b><select id="lane${i}"><option>0</option><option>1</option><option>2</option><option>3</option></select><label><input id="rev${i}" type="checkbox"> reverse</label></div>`)});
    markDirty();
  }
  m.segments.forEach((x,i)=>{setv('lane'+i,x[0]);const e=$('rev'+i);if(!e.dataset.dirty)e.checked=!!x[1]});
}
function calibrationText(c){
  if(c.active)return 'capture active · samples '+c.samples;
  if(!c.has_summary)return 'No capture yet.';
  const s=c.summary;
  let t='frames '+s.total;
  if(s.plane_valid)t+=`\nyaw p10/med/p90: ${s.yaw.join('/')} cdeg\npitch: ${s.pitch.join('/')} cdeg\nz0: ${s.z0.join('/')} mm`;
  if(s.spatial_valid)t+=`\nwall min: ${s.min.join('/')} mm\nwall max: ${s.max.join('/')} mm\nsegments med start/end: ${s.segments.map((x,i)=>mapNames[i]+' '+x[0]+'/'+x[1]).join(', ')}`;
  return t;
}
function render(s){
  txt('fw',s.fw.version+' · Stage '+s.fw.stage+' · '+s.fw.target);
  txt('stCorr',corrNames[s.output.correction]||'?');
  txt('stBright',s.output.brightness+'/255');
  txt('stDdp',s.ddp.running?(s.ddp.sender_locked?'locked':'ready'):'off');
  txt('stTof',s.tof.available?(s.tof.state+(s.tof.gain_fail_open?' · unity':'')):'unavailable');
  setv('brightness',s.output.brightness);txt('brightnessValue',s.output.brightness);
  txt('testState','pattern '+s.commissioning.pattern+' · '+Math.ceil(s.commissioning.remaining_ms/1000)+' s');
  const safeTest=s.output.brightness>0&&s.output.brightness<=64;
  $('testSegments').disabled=!safeTest;$('testDirection').disabled=!safeTest;
  renderMap(s.map);
  $('mapApply').disabled=s.output.brightness!==0;$('mapReset').disabled=s.output.brightness!==0;
  const sp=s.spatial;
  setv('spW',sp.w10/10);setv('spH',sp.h10/10);setv('spX',sp.x10/10);setv('spY',sp.y10/10);setv('spZ',sp.z10/10);setv('spR',sp.rot);setv('spM',sp.mirror);setv('spD',sp.deadband10/10);
  txt('spSource',sp.source);txt('curveSource',s.curve.source);
  const blocked=s.output.correction===2;
  $('spApply').disabled=blocked;$('spReset').disabled=blocked;$('curveApply').disabled=blocked;$('curveReset').disabled=blocked;
  setv('curve',s.curve.points.map(p=>p[0]+':'+p[1]).join(','));
  txt('tofDetail',s.tof.available?`state ${s.tof.state}\nage ${s.tof.age_ms} ms · zones ${s.tof.valid_zones}/64 · median ${s.tof.median_mm} mm\nplane ${s.tof.plane_valid?'ok':'bad'} · yaw ${(s.tof.yaw_cdeg/100).toFixed(2)}° · pitch ${(s.tof.pitch_cdeg/100).toFixed(2)}° · accepted ${s.tof.plane_accepted}\nwall ${s.tof.min_mm}..${s.tof.max_mm} mm · ${s.tof.gain_fail_open?'FAIL OPEN / UNITY':'gain usable'}`:'ToF unavailable');
  txt('calDetail',calibrationText(s.calibration));
  $('calStart').disabled=s.calibration.active;$('probeStart').disabled=s.output.correction!==1;
  txt('wifiState',`${s.wifi.connected?'connected':'disconnected'} · ${s.wifi.ip||'no IP'} · RSSI ${s.wifi.rssi} dBm · ${s.wifi.ssid||'no SSID'}`);
  setv('ssid',s.wifi.ssid||'');
  txt('diag',`DDP frames ${s.ddp.frames} · publications ${s.ddp.publications}\nsender ${s.ddp.sender_locked?(s.ddp.sender_ip+':'+s.ddp.sender_port):'none'}\npersistence ${s.persistence?'available':'unavailable'}\nheap free/min ${s.heap.free}/${s.heap.min} B\nweb requests/actions/bad ${s.web.requests}/${s.web.actions}/${s.web.bad}`);
  $('factory').disabled=s.output.brightness!==0;
  if(s.action.id&&s.action.id!==lastAction){lastAction=s.action.id}
  if(s.action.id===lastAction&&s.action.id){txt('action',s.action.msg|| (s.action.ok?'ok':'failed'));$('action').className=s.action.ok?'ok':'bad'}
}
async function refresh(){
  try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw new Error('status HTTP '+r.status);render(await r.json())}
  catch(e){txt('action','offline: '+e.message);$('action').className='bad'}
}
$('brightness').addEventListener('input',e=>txt('brightnessValue',e.target.value));
$('brightness').addEventListener('change',e=>{clean(['brightness']);post('/api/brightness',e.target.value)});
markDirty();refresh();setInterval(refresh,1500);
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
        "\"correction\":%u",
        static_cast<unsigned>(
            snapshot.brightness),
        static_cast<unsigned>(
            snapshot.correctionMode));

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
        "\"frames\":%lu,"
        "\"publications\":%lu,"
        "\"sender_locked\":%s,"
        "\"sender_ip\":",
        boolJson(snapshot.ddpRunning),
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

    writer.append("}");

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
            "[%u,%u]",
            static_cast<unsigned>(
                mapping.lane),
            static_cast<unsigned>(
                mapping.reversed));
    }

    writer.append("]}");

    writer.appendf(
        ",\"commissioning\":{"
        "\"pattern\":%u,"
        "\"remaining_ms\":%lu}",
        static_cast<unsigned>(
            snapshot.commissioningPattern),
        static_cast<unsigned long>(
            snapshot
                .commissioningRemainingMs));

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
        closeClient();
        return;
    }

    if (sent == 0) {
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
