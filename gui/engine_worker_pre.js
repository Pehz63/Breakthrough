// engine_worker_pre.js - the engine worker's message pump (web build only).
//
// build_web.bat compiles gui/gui_engine.cpp a second time with
// -DGUI_ENGINE_WORKER into build/web/engine_worker.js, and the page runs two
// copies of it as Web Workers: one for agent moves, one for analysis (see
// gui/CLAUDE.md, "Web engine workers"). The page sends each command as one
// Uint8Array (gui_engine.cpp, the CMD_* encoding). This pump queues commands
// until the runtime is ready, hands each to _wk_message, then runs one unit of
// engine work with _wk_pump per macrotask, so commands that arrive mid-analysis
// are read between two root-move searches.
var Module = Module || {};
var wkInbox = [];
var wkReady = false;
var wkScheduled = false;
var wkChannel = new MessageChannel();

function wkSchedule() {
    if (wkReady && !wkScheduled) { wkScheduled = true; wkChannel.port2.postMessage(0); }
}

function wkDeliver(bytes) {
    var p = _malloc(bytes.length);
    HEAPU8.set(bytes, p);
    _wk_message(p, bytes.length);
    _free(p);
}

wkChannel.port1.onmessage = function () {
    wkScheduled = false;
    while (wkInbox.length) wkDeliver(wkInbox.shift());
    if (_wk_pump() || wkInbox.length) wkSchedule();
};

self.onmessage = function (e) { wkInbox.push(e.data); wkSchedule(); };

Module['onRuntimeInitialized'] = function () { wkReady = true; wkSchedule(); };
