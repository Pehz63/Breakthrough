# run_rank.ps1 - Build (optionally) and run the agent Elo ranker (rank.exe).
#
# Serial (default, gives clean ms/move timing):
#   .\tools\run_rank.ps1 -Build run --games 8
#   .\tools\run_rank.ps1 check
#   .\tools\run_rank.ps1 history --agent "ab(d4"
#
# Parallel play across processes (rows are tagged par=K and excluded from the
# default ms/move figures):
#   .\tools\run_rank.ps1 -Workers 8 --games 8
#
# The engine keeps board/eval state in globals, so games cannot share a process.
# Each worker plays pending-game indices with index % K == shard and writes its
# own <store>.<shard> file; those are appended to the store only after every
# worker exits cleanly, then a single rate pass runs. Do not pass --out or
# --shard/--of together with -Workers (the driver owns them in that mode).
#
# -NoRate stops after the play + merge and skips that rate pass. REQUIRED when
# playing a cohort into an existing roster, because the automatic rate is
# UNPINNED and writes ranking/ratings.tsv + standings.tsv: with a study roster
# passed through, it silently replaces the canonical fit with one that includes
# the cohort, which is exactly what `rate --pin` exists to avoid. This bit once
# (2026-07-29, TD-Leaf cohort study) -- note the leading "play" token is ABSORBED
# by this driver, so `run_rank.ps1 play ...` still rates. Use:
#   tools/run_rank.ps1 -Workers 12 -NoRate play --roster <r> --cohort <c> --games 8
#   rank.exe rate --roster <r> --pin ranking/standings.tsv
#
# LADDER (default). `--games N` is a target, not an increment: the scheduler
# counts stored games per pair and issues only the deficit. So the driver plays
# rungs 2, 4, 8, ... N, merging the shards between each, which costs the same
# total games as one pass at N but gives a readable store minutes in. Rung 1
# touches every pair, so a broken agent or a mis-specified roster shows up
# there instead of at the end. -NoLadder for a single pass.
#
# The ladder never stops early on its own. Reading a rung and deciding to stop
# is a human call made against an SE target written down BEFORE the run, because
# fitting after every rung and stopping when the answer looks good is optional
# stopping and inflates false positives invisibly. See
# plans/ranking-run-scheduling-plan-1-tidal-lantern.md.
#
# -PinEachRung <ratings.tsv> runs `rate --pin <file>` after each rung's merge, so
# standings appear as the run proceeds. It writes only ranking/*_pinned.tsv and
# cannot disturb the canonical fit.
#
# After each rung's merge the store is sealed (`rank.exe seal --max-mb 90`) when
# it has a part index, so the committed tail never rests above 90 MB and no
# commit carries a file GitHub would reject. Stores without an index (screening
# and scratch stores, all gitignored) stay single files.

# PositionalBinding=$false so pass-through tokens like "--games" land in $Args
# instead of being bound to $Store positionally.
[CmdletBinding(PositionalBinding = $false)]
param(
    [switch]$Build,
    [int]$Workers = 1,
    [switch]$NoRate,
    [switch]$NoLadder,
    [string]$PinEachRung = "",
    [string]$Store = "ranking/matches.jsonl",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args
)

$ErrorActionPreference = "Stop"
$Exe = ".\rank.exe"

if ($Build) {
    Write-Host "Building rank.exe..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_rank.bat"
    if (-not (Test-Path $Exe)) {
        Write-Error "Build did not produce $Exe."
        exit 1
    }
    Write-Host "Build OK -> $Exe"
}

if (-not (Test-Path $Exe)) {
    Write-Error "$Exe not found. Run with -Build first."
    exit 1
}

if ($Workers -le 1) {
    if ($null -eq $Args -or $Args.Count -eq 0) { & $Exe; exit $LASTEXITCODE }
    & $Exe @Args
    exit $LASTEXITCODE
}

# Parallel mode: any leading "play" or "run" token is absorbed (the driver runs
# sharded play + one rate); the remaining --key value options go to both phases.
$extra = @()
if ($null -ne $Args -and $Args.Count -gt 0) {
    $extra = $Args
    if ($extra[0] -eq "play" -or $extra[0] -eq "run") {
        if ($extra.Count -gt 1) { $extra = $extra[1..($extra.Count - 1)] } else { $extra = @() }
    }
}

if (-not (Test-Path "ranking")) { New-Item -ItemType Directory "ranking" | Out-Null }

# Read the target off the pass-through options so the driver can build the rung
# list. rank.exe's own default is 8, so match it when --games is absent.
$target = 8
for ($i = 0; $i -lt $extra.Count - 1; $i++) {
    if ($extra[$i] -eq "--games") { $target = [int]$extra[$i + 1] }
}

# Rungs 2, 4, 8, ... target. The shards themselves run with --no-ladder: a shard
# writes its own file and cannot see its siblings' games, so if it laddered
# in-process its later rungs would re-issue games another shard already played.
# Laddering has to happen HERE, where the merge between rungs makes every
# worker's next schedule see the whole store.
$rungs = @()
if (-not $NoLadder -and $target -gt 2) {
    $g = 2
    while ($g -lt $target) { $rungs += $g; $g = $g * 2 }
}
$rungs += $target

if ($rungs.Count -gt 1) {
    Write-Host "Ladder: rungs $($rungs -join ', ') games/pair, merging between each."
    Write-Host "  Same total games as one pass at $target. Rung 1 touches every pair."
}

$runStart = Get-Date
for ($r = 0; $r -lt $rungs.Count; $r++) {
    $rg = $rungs[$r]
    # Strip any caller-supplied --games and substitute this rung's target.
    $rungExtra = @()
    for ($i = 0; $i -lt $extra.Count; $i++) {
        if ($extra[$i] -eq "--games") { $i++; continue }
        $rungExtra += $extra[$i]
    }
    $rungExtra += @("--games", $rg, "--no-ladder")

    if ($rungs.Count -gt 1) {
        Write-Host ""
        Write-Host "=== rung $($r + 1)/$($rungs.Count): --games $rg ==="
    }
    Write-Host "Launching $Workers rank shards (store=$Store)..."
    $procs = @()
    $shardFiles = @()
    for ($s = 0; $s -lt $Workers; $s++) {
        $sf = "$Store.$s"
        if (Test-Path $sf) { Remove-Item $sf -Force }
        $shardFiles += $sf
        $playArgs = @("play", "--shard", $s, "--of", $Workers, "--in", $Store, "--out", $sf) + $rungExtra
        # Start-Process -ArgumentList (Windows PowerShell 5.1) does NOT auto-quote array
        # elements containing spaces -- it joins them with plain spaces into one command
        # line, so an unquoted path like a project root under "...\Board Games\..." gets
        # split into multiple argv tokens on the receiving end. Quote any element that
        # needs it before handing the array to Start-Process. (Caught 2026-07-30: this
        # cohort's --roster is an absolute path built from $Root, which does contain a
        # space; an earlier manual invocation with a relative, space-free path happened
        # not to trigger it.)
        $quotedArgs = $playArgs | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }
        $procs += Start-Process -FilePath $Exe -ArgumentList $quotedArgs -NoNewWindow -PassThru
    }

    Write-Host "Waiting for $($procs.Count) shards..."
    $procs | Wait-Process

    $bad = @($procs | Where-Object { $null -ne $_.ExitCode -and $_.ExitCode -ne 0 })
    if ($bad.Count -gt 0) {
        Write-Error "A shard failed; shard files were NOT merged (inspect $Store.<shard>)."
        exit 1
    }

    # Append each shard's rows to the permanent store, then clean up. Every line
    # is checked for structural completeness first: on 2026-09-04 a blind merge
    # put 3 torn rows (one game split across two lines) into the store, and a
    # ladder merges once per rung rather than once per run, so an unguarded
    # append would multiply that.
    $merged = 0; $torn = 0
    foreach ($sf in $shardFiles) {
        if (-not (Test-Path $sf)) { continue }
        $good = New-Object System.Collections.Generic.List[string]
        foreach ($line in [System.IO.File]::ReadLines((Resolve-Path $sf))) {
            $t = $line.Trim()
            if ($t.Length -eq 0) { continue }
            if ($t.StartsWith("{") -and $t.EndsWith("}")) { $good.Add($t); $merged++ }
            else { $torn++ }
        }
        if ($good.Count -gt 0) { Add-Content -Path $Store -Value $good -Encoding Ascii }
        Remove-Item $sf -Force
    }
    if ($torn -gt 0) {
        Write-Warning "$torn torn line(s) in the shard files were DROPPED, not merged."
    }

    $mins = ((Get-Date) - $runStart).TotalMinutes
    Write-Host ("rung $($r + 1)/$($rungs.Count) merged: {0} row(s), {1:N1} min elapsed for the run so far." -f $merged, $mins)

    $index = ($Store -replace '\.jsonl$', '') + ".index.txt"
    if (Test-Path $index) {
        & $Exe seal --in $Store --max-mb 90
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Sealing $Store failed (message above). Merged rows are in the store."
            exit 1
        }
    }

    if ($PinEachRung -ne "") {
        # `rate` takes --roster and --board; --games/--cohort/--no-ladder are
        # play-only and would be ignored or rejected, so pass only what it uses.
        $rateArgs = @()
        for ($i = 0; $i -lt $extra.Count; $i++) {
            if ($extra[$i] -eq "--roster" -or $extra[$i] -eq "--board") {
                $rateArgs += @($extra[$i], $extra[$i + 1]); $i++
            }
        }
        Write-Host "Pinned fit after rung $($r + 1) (writes ranking/*_pinned.tsv only)..."
        & $Exe rate --in $Store --pin $PinEachRung @rateArgs
    }
}

if ($NoRate) {
    Write-Host "-NoRate: skipping the rate pass (canonical ranking/*.tsv untouched)."
    Write-Host "  Rate explicitly when ready, e.g. rank.exe rate --pin ranking/standings.tsv"
    exit 0
}

& $Exe rate --in $Store @extra
exit $LASTEXITCODE
