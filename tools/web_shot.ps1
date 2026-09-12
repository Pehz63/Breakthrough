# web_shot.ps1 - real-time screenshots of the web build in headless Chrome.
#
# Serves build\web on a local port (python -m http.server), opens each page in
# its own headless Chrome (hidden processes, nothing appears on screen), waits
# -Seconds of wall-clock time, and saves one PNG per page through the DevTools
# protocol. Real time matters: under --virtual-time-budget the page's
# clock-paced agent moves and the analysis time limits stall, and --timeout
# does not delay --screenshot.
#
#   tools\web_shot.ps1                     # the default pages below, desktop size
#   tools\web_shot.ps1 -Pages "hard=mode=black&level=hard&hints=1" -Seconds 30
#   tools\web_shot.ps1 -Device 390x760     # the same pages on an emulated phone
#   tools\web_shot.ps1 -Device 390x760 -Pages "p=mode=white" -Steps "tap:182,368","tap:182,324","wait:4000","shot"
#
# Each -Pages entry is "name=query". Output: build\web_shots\<name>.png, plus
# <name>_1.png, <name>_2.png ... for each "shot" step. -Device WxH emulates a
# phone: that viewport in CSS pixels at -Dpr device pixels per CSS pixel, with
# touch input. -Steps run on every page after the first screenshot, in CSS
# pixels: "tap:x,y", "drag:x1,y1,x2,y2" (touch), "wait:ms", "shot". Build the
# page first (build_web.bat). Exits 1 if any screenshot is missing.
param(
    [string[]]$Pages = @("watch=mode=watch&hints=1", "hard=mode=black&level=hard&hints=1", "easy=mode=black&level=easy"),
    [int]$Seconds = 20,
    [int]$Port = 8766,
    [string]$Device = "",
    [double]$Dpr = 3,
    [string[]]$Steps = @()
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

$out = Join-Path $root "build\web_shots"
New-Item -ItemType Directory -Force $out | Out-Null
$stamp = Get-Date -Format "yyyyMMddHHmmss"
$profRoot = Join-Path $out "profiles_$stamp"
$ct = [Threading.CancellationToken]::None

# One DevTools session per page, kept open for the whole run: the emulation
# overrides last only as long as the session that set them.
function Open-Session([int]$dbgPort) {
    for ($k = 0; $k -lt 50; $k++) {
        try {
            # Assigned first: piped straight on, PowerShell 5.1 passes the whole
            # JSON array as one object instead of enumerating it.
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

function Save-Shot($s, [string]$png) {
    $resp = Send $s "Page.captureScreenshot" '{"format":"png"}'
    if ($resp -notmatch '"data":"([^"]+)"') { throw "no image in the reply" }
    [IO.File]::WriteAllBytes($png, [Convert]::FromBase64String($Matches[1]))
    "saved $png"
}

function Touch($s, [string]$type, [string]$points) {
    [void](Send $s "Input.dispatchTouchEvent" ('{"type":"' + $type + '","touchPoints":[' + $points + ']}'))
}

# Start-Process joins -ArgumentList with spaces and does not quote, and the
# project path can contain spaces, so every path argument is quoted by hand.
$server = Start-Process -FilePath python -WorkingDirectory $root -PassThru -WindowStyle Hidden `
    -ArgumentList "-m http.server $Port --bind 127.0.0.1 -d `"$root\build\web`""
$missing = 0
$jobs = @()
try {
    # One browser per page, so no page is a throttled background tab.
    $i = 0
    foreach ($entry in $Pages) {
        $name, $query = $entry -split '=', 2
        $dbg = 9400 + $i
        $argv = @("--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                  "--user-data-dir=`"$profRoot\$i`"", "--disk-cache-size=1", "--window-size=1280,860",
                  "--remote-debugging-port=$dbg", "about:blank")
        Start-Process -FilePath $chrome -ArgumentList $argv -WindowStyle Hidden | Out-Null
        $jobs += [pscustomobject]@{ Name = $name; Url = "http://127.0.0.1:$Port/?$query&v=$stamp"; Port = $dbg; S = $null }
        $i++
    }
    foreach ($j in $jobs) {
        try {
            $j.S = Open-Session $j.Port
            if ($Device) {
                $w, $h = $Device -split 'x'
                [void](Send $j.S "Emulation.setDeviceMetricsOverride" ('{"width":' + $w + ',"height":' + $h + ',"deviceScaleFactor":' + $Dpr + ',"mobile":true}'))
                [void](Send $j.S "Emulation.setTouchEmulationEnabled" '{"enabled":true,"maxTouchPoints":5}')
            }
            [void](Send $j.S "Page.navigate" ('{"url":"' + $j.Url + '"}'))
        } catch {
            "FAILED to open $($j.Name): $($_.Exception.Message)"
        }
    }
    Start-Sleep -Seconds $Seconds
    foreach ($j in $jobs) {
        try {
            if (-not $j.S) { throw "no session" }
            Save-Shot $j.S (Join-Path $out "$($j.Name).png")
            $n = 1
            foreach ($step in $Steps) {
                $kind, $arg = $step -split ':', 2
                switch ($kind) {
                    "tap" {
                        $x, $y = $arg -split ','
                        Touch $j.S "touchStart" ('{"x":' + $x + ',"y":' + $y + '}')
                        Start-Sleep -Milliseconds 90
                        Touch $j.S "touchEnd" ''
                        Start-Sleep -Milliseconds 250
                    }
                    "drag" {
                        $x1, $y1, $x2, $y2 = [double[]]($arg -split ',')
                        Touch $j.S "touchStart" ('{"x":' + $x1 + ',"y":' + $y1 + '}')
                        for ($k = 1; $k -le 8; $k++) {
                            Start-Sleep -Milliseconds 40
                            Touch $j.S "touchMove" ('{"x":' + ($x1 + ($x2 - $x1) * $k / 8) + ',"y":' + ($y1 + ($y2 - $y1) * $k / 8) + '}')
                        }
                        Start-Sleep -Milliseconds 60
                        Touch $j.S "touchEnd" ''
                        Start-Sleep -Milliseconds 250
                    }
                    "wait" { Start-Sleep -Milliseconds ([int]$arg) }
                    "shot" { Save-Shot $j.S (Join-Path $out "$($j.Name)_$n.png"); $n++ }
                    default { throw "unknown step '$step'" }
                }
            }
        } catch {
            "FAILED $($j.Name): $($_.Exception.Message)"
            $missing++
        }
    }
} finally {
    foreach ($j in $jobs) { if ($j.S) { $j.S.Ws.Dispose() } }
    Get-CimInstance Win32_Process -Filter "Name='chrome.exe' OR Name='msedge.exe'" |
        Where-Object { $_.CommandLine -like "*$profRoot*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Remove-Item -Recurse -Force $profRoot -ErrorAction SilentlyContinue
}
if ($missing -gt 0) { exit 1 }
