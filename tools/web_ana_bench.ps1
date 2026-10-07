# web_ana_bench.ps1 - measure the web page's analysis depth and agent move cost.
#
# Serves build\web, opens each page in its own hidden headless Chrome, one page
# at a time so no two browsers compete for cores, waits -Seconds of wall-clock
# time, then reads the page's measurement logs through the DevTools protocol:
#   window.__anaLog     one record per completed analysis depth (or run end):
#                       depth, root moves, wall and compute ms, nodes, top-3
#                       scores, and why the run stopped
#   window.__anaStarts  the moves that led to each analysed position
#   window.__moveLog    one record per agent move job: ms, nodes, depth
#   window.__frames     requestAnimationFrame count, for the frame rate, plus the
#                       longest frame gap and the count of gaps over 50 ms
#   workerSpawns        engine workers started (2 at load, +1 per analysis restart)
# and writes them to build\web_bench\<name>.json.
#
#   tools\web_ana_bench.ps1 -Pages "start=mode=white&hints=1" -Seconds 70
#   tools\web_ana_bench.ps1 -Pages "nocap=mode=white&hints=1&anacap=0" -Seconds 70
#   tools\web_ana_bench.ps1 -Pages "watch=mode=watch&hints=1" -Seconds 300
#
# Page options used here (gui/main_gui.cpp, ApplyWebUrlOptions): moves=c2c,f7f
# sets up a position, anacap=<ms> changes the per-root-move cost cap (0 = none).
# -Gpu drops the SwiftShader software-GL flags, so rendering uses the GPU when
# headless Chrome can reach one. -Fps caps requestAnimationFrame at that rate
# (default 60, 0 = uncapped). Headless Chrome has no display to sync to and
# otherwise renders as fast as it can (about 300 fps here), which keeps the
# page's main thread busy in a way a real browser at the display rate does not.
# Build the page first (build_web.bat).
param(
    [string[]]$Pages = @("start=mode=white&hints=1"),
    [int]$Seconds = 70,
    [int]$Port = 8767,
    [int]$Fps = 60,
    [switch]$Gpu
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not (Test-Path build\web\index.html)) { Write-Error "build\web\index.html not found. Run build_web.bat first." }

$chrome = @("$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
            "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe",
            "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe") |
          Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
if (-not $chrome) { Write-Error "Chrome or Edge not found." }

$out = Join-Path $root "build\web_bench"
New-Item -ItemType Directory -Force $out | Out-Null
$stamp = Get-Date -Format "yyyyMMddHHmmss"
$profRoot = Join-Path $out "profiles_$stamp"
$ct = [Threading.CancellationToken]::None

function Open-Session([int]$dbgPort) {
    for ($k = 0; $k -lt 50; $k++) {
        try {
            $targets = Invoke-RestMethod "http://127.0.0.1:$dbgPort/json/list"
            $page = @($targets | Where-Object { $_.type -eq "page" })[0]
            if ($page) { break }
        } catch { }
        Start-Sleep -Milliseconds 200
    }
    if (-not $page) { throw "no page target on port $dbgPort" }
    $ws = New-Object System.Net.WebSockets.ClientWebSocket
    $ws.Options.KeepAliveInterval = [TimeSpan]::FromSeconds(5)
    $ws.ConnectAsync([Uri]$page.webSocketDebuggerUrl, $ct).Wait()
    [pscustomobject]@{ Ws = $ws; Id = 0 }
}

# Send one request and return the reply to it, skipping any events in between.
function Send($s, [string]$method, [string]$paramsJson = "{}") {
    $s.Id++
    $msg = '{"id":' + $s.Id + ',"method":"' + $method + '","params":' + $paramsJson + '}'
    $bytes = [Text.Encoding]::UTF8.GetBytes($msg)
    $s.Ws.SendAsync((New-Object ArraySegment[byte] -ArgumentList @(,$bytes)),
                    [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).Wait()
    $sb = New-Object Text.StringBuilder
    $buf = New-Object byte[] 4194304
    while ($true) {
        $r = $s.Ws.ReceiveAsync((New-Object ArraySegment[byte] -ArgumentList @(,$buf)), $ct)
        $r.Wait()
        [void]$sb.Append([Text.Encoding]::UTF8.GetString($buf, 0, $r.Result.Count))
        if ($r.Result.EndOfMessage) {
            $reply = $sb.ToString()
            if ($reply -match ('"id":' + $s.Id + '[,}]')) { return $reply }
            [void]$sb.Clear()
        }
    }
}

$server = Start-Process -FilePath python -WorkingDirectory $root -PassThru -WindowStyle Hidden `
    -ArgumentList "-m http.server $Port --bind 127.0.0.1 -d `"$root\build\web`""
$failed = 0
try {
    $i = 0
    foreach ($entry in $Pages) {
        $name, $query = $entry -split '=', 2
        $dbg = 9500 + $i
        $argv = @("--headless=new", "--user-data-dir=`"$profRoot\$i`"", "--disk-cache-size=1",
                  "--window-size=1280,860", "--remote-debugging-port=$dbg", "about:blank")
        if (-not $Gpu) { $argv = @("--use-angle=swiftshader", "--enable-unsafe-swiftshader") + $argv }
        $proc = Start-Process -FilePath $chrome -ArgumentList $argv -WindowStyle Hidden -PassThru
        $s = $null
        try {
            $s = Open-Session $dbg
            # Frame count, plus frame gaps after the first 5 s (past page load):
            # the longest, and how many exceed 50 ms (a visible stall).
            $cap = ''
            if ($Fps -gt 0) {
                # Every callback requested within one frame runs at the next
                # frame boundary, as on a display refreshing at -Fps.
                $cap = '(function(){var raf=window.requestAnimationFrame.bind(window),t0=performance.now(),step=1000/' + $Fps + ';' +
                       'window.requestAnimationFrame=function(cb){var now=performance.now();' +
                       'var at=t0+(Math.floor((now-t0)/step)+1)*step;return setTimeout(function(){raf(cb);},at-now);};})();'
            }
            $frames = $cap + 'window.__frames=0;window.__maxGap=0;window.__longFrames=0;var __last=0;' +
                      '(function f(t){if(__last>0&&t>5000){var g=t-__last;if(g>window.__maxGap)window.__maxGap=g;' +
                      'if(g>50)window.__longFrames++;}if(t>0)__last=t;window.__frames++;requestAnimationFrame(f);})(0);'
            [void](Send $s "Page.enable")
            [void](Send $s "Page.addScriptToEvaluateOnNewDocument" ('{"source":"' + $frames + '"}'))
            [void](Send $s "Page.navigate" ('{"url":"http://127.0.0.1:' + $Port + '/?' + $query + '&v=' + $stamp + '"}'))
            Start-Sleep -Seconds $Seconds
            $expr = 'JSON.stringify({query:location.search,seconds:' + $Seconds + ',fps:' + $Fps + ',gpu:' + ($(if ($Gpu) { 'true' } else { 'false' })) +
                    ',frames:window.__frames||0,maxGap:window.__maxGap||0,longFrames:window.__longFrames||0,' +
                    'workerSpawns:(window.__engWk||{}).spawns||0,now:performance.now(),anaLog:window.__anaLog||[],' +
                    'anaStarts:window.__anaStarts||[],moveLog:window.__moveLog||[]})'
            $exprJson = $expr.Replace('\', '\\').Replace('"', '\"')
            $reply = Send $s "Runtime.evaluate" ('{"expression":"' + $exprJson + '","returnByValue":true}')
            $obj = $reply | ConvertFrom-Json
            $val = $obj.result.result.value
            if (-not $val) { throw "no value in the reply: $($reply.Substring(0, [Math]::Min(300, $reply.Length)))" }
            $file = Join-Path $out "$name.json"
            [IO.File]::WriteAllText($file, $val)
            "saved $file"
        } catch {
            "FAILED $($name): $($_.Exception.Message)"
            $failed++
        } finally {
            if ($s) { $s.Ws.Dispose() }
            Get-CimInstance Win32_Process -Filter "Name='chrome.exe' OR Name='msedge.exe'" |
                Where-Object { $_.CommandLine -like "*$profRoot\$i*" } |
                ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        }
        $i++
    }
} finally {
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Remove-Item -Recurse -Force $profRoot -ErrorAction SilentlyContinue
}
if ($failed -gt 0) { exit 1 }
