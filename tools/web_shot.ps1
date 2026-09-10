# web_shot.ps1 - real-time screenshots of the web build in headless Chrome.
#
# Serves build\web on a local port (python -m http.server), opens each page in
# its own headless Chrome (hidden processes, nothing appears on screen), waits
# -Seconds of wall-clock time, and saves one PNG per page through the DevTools
# protocol. Real time matters: under --virtual-time-budget the page's
# clock-paced agent moves and the analysis time limits stall, and --timeout
# does not delay --screenshot.
#
#   tools\web_shot.ps1                     # the default pages below
#   tools\web_shot.ps1 -Pages "hard=mode=black&level=hard&hints=1" -Seconds 30
#
# Each -Pages entry is "name=query". Output: build\web_shots\<name>.png. Build
# the page first (build_web.bat). Exits 1 if any screenshot is missing.
param(
    [string[]]$Pages = @("watch=mode=watch&hints=1", "hard=mode=black&level=hard&hints=1", "easy=mode=black&level=easy"),
    [int]$Seconds = 20,
    [int]$Port = 8766
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

# One DevTools request over a fresh websocket, returning the reply to id 1.
function Cdp([string]$wsUrl, [string]$method, [string]$paramsJson) {
    $ws = New-Object System.Net.WebSockets.ClientWebSocket
    $ct = [Threading.CancellationToken]::None
    $ws.ConnectAsync([Uri]$wsUrl, $ct).Wait()
    $msg = '{"id":1,"method":"' + $method + '","params":' + $paramsJson + '}'
    $bytes = [Text.Encoding]::UTF8.GetBytes($msg)
    $ws.SendAsync((New-Object ArraySegment[byte] -ArgumentList @(,$bytes)),
                  [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).Wait()
    $sb = New-Object Text.StringBuilder
    $buf = New-Object byte[] 1048576
    while ($true) {
        $r = $ws.ReceiveAsync((New-Object ArraySegment[byte] -ArgumentList @(,$buf)), $ct)
        $r.Wait()
        [void]$sb.Append([Text.Encoding]::UTF8.GetString($buf, 0, $r.Result.Count))
        if ($r.Result.EndOfMessage) {
            if ($sb.ToString() -match '"id":1[,}]') { break }
            [void]$sb.Clear()
        }
    }
    $ws.Dispose()
    return $sb.ToString()
}

# Start-Process joins -ArgumentList with spaces and does not quote, and the
# project path can contain spaces, so every path argument is quoted by hand.
$server = Start-Process -FilePath python -WorkingDirectory $root -PassThru -WindowStyle Hidden `
    -ArgumentList "-m http.server $Port --bind 127.0.0.1 -d `"$root\build\web`""
$missing = 0
try {
    # One browser per page, so no page is a throttled background tab.
    $jobs = @()
    $i = 0
    foreach ($entry in $Pages) {
        $name, $query = $entry -split '=', 2
        $dbg = 9400 + $i
        $url = "http://127.0.0.1:$Port/?$query&v=$stamp"
        $argv = @("--headless=new", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
                  "--user-data-dir=`"$profRoot\$i`"", "--disk-cache-size=1", "--window-size=1280,860",
                  "--remote-debugging-port=$dbg", $url)
        Start-Process -FilePath $chrome -ArgumentList $argv -WindowStyle Hidden | Out-Null
        $jobs += [pscustomobject]@{ Name = $name; Port = $dbg }
        $i++
    }
    Start-Sleep -Seconds $Seconds
    foreach ($j in $jobs) {
        $png = Join-Path $out "$($j.Name).png"
        try {
            $targets = Invoke-RestMethod "http://127.0.0.1:$($j.Port)/json/list"
            $page = @($targets | Where-Object { $_.type -eq "page" })[0]
            $resp = Cdp $page.webSocketDebuggerUrl "Page.captureScreenshot" '{"format":"png"}'
            if ($resp -notmatch '"data":"([^"]+)"') { throw "no image in the reply" }
            [IO.File]::WriteAllBytes($png, [Convert]::FromBase64String($Matches[1]))
            "saved $png"
        } catch {
            "FAILED $($j.Name): $($_.Exception.Message)"
            $missing++
        }
    }
} finally {
    Get-CimInstance Win32_Process -Filter "Name='chrome.exe' OR Name='msedge.exe'" |
        Where-Object { $_.CommandLine -like "*$profRoot*" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Stop-Process -Id $server.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Remove-Item -Recurse -Force $profRoot -ErrorAction SilentlyContinue
}
if ($missing -gt 0) { exit 1 }
