<#
    shoot-game-frames.ps1

    Launches ContrarySurvivor as a REAL standalone game process (UnrealEditor.exe -game)
    and captures frames from the PLAYER CAMERA (so camera post-process is included),
    after the level is fully loaded.

    How the capture works (verified against UE 5.5 engine sources, not memory):
      * -ExecCmds is queued into GEngine->DeferredCommands (UnrealEngine.cpp:2192) and executed
        on the first engine tick through the local player, so console commands reach
        UPlayerInput (Player.cpp: UPlayer::Exec -> PlayerController->PlayerInput->ProcessConsoleExec).
      * "setbind <Key> '<command>'" is UPlayerInput::SetBind (PlayerInput.h:469) -> DebugExecBindings.
        In non-shipping builds any bound key runs its console command from UPlayerInput::InputKey
        (PlayerInput.cpp:427) -> ExecInputCommands -> ULocalPlayer::Exec -> GameViewportClient::Exec.
        On IE_Released the command is NOT re-executed (ExecInputCommands routes releases into
        UPlayerInput::Exec, which only understands key names), so one press == one screenshot.
      * "HighResShot <W>x<H> filename=<abs.png>" renders the scene OFF-SCREEN at exactly the
        requested resolution (HighResScreenshot.cpp:116 sets GScreenshotResolutionX/Y directly),
        so the 2400x1080 phone-aspect frame does not require resizing the game window.
        FilenameOverride containing a slash is used as an absolute path
        (UnrealClient.cpp:274 + CreateViewportScreenShotFilename).
      * r.HighResScreenshotDelay = number of run-up frames rendered before the capture
        (UnrealClient.cpp:1518) - lets TAA/streaming converge on the captured frame.

    Readiness is proven from the game log, not by eye:
      * "LoadMap" line for /Game/Maps/L_World_C
      * "QA: intro control handed to player" (ContrarySurvivorPlayerController.cpp:1948)
      * then the log must stay silent for -QuietSec (shader compiles / streaming finished)

    Side effect that is cleaned up: UPlayerInput::SetBind calls SaveConfig(), which writes
    DebugExecBindings into Saved/Config/WindowsEditor/Input.ini. The script backs that file up
    before the run and restores it afterwards.

    NOTE: keys are injected into the game window, so the window is brought to the front and
    the keyboard layout of that window is switched to en-US first: UE resolves letter keys
    through the CHAR code (InputCoreTypes.cpp:1573 + WindowsPlatformInput.cpp), so with a
    Russian layout active the letter teleport keys (Y / T) would not fire at all.
#>

[CmdletBinding()]
param(
    [string]$ProjectRoot     = 'E:\ContrarySurvior\ContrarySurvivor',
    [string]$EngineRoot      = 'E:\UnrealEngine\UE_5.5',
    [string]$Map             = '/Game/Maps/L_World_C',
    [string]$Tag             = 'pp',
    [int]$WinResX            = 1920,
    [int]$WinResY            = 1080,
    [int]$PhoneResX          = 2400,
    [int]$PhoneResY          = 1080,
    [int]$ShotDelayFrames    = 12,
    [int]$ReadyTimeoutSec    = 300,
    [int]$QuietSec           = 12,
    [int]$SoakSec            = 10,
    [int]$SettleSec          = 8,
    [int]$ShotWaitSec        = 45,
    [switch]$NoTraderShots,
    [switch]$NoGodMode,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Say([string]$msg) { Write-Host ("SHOT| " + $msg) }

# ---------------------------------------------------------------- Win32 helpers
if (-not ("CSWin32" -as [type])) {
Add-Type -Namespace '' -Name 'CSWin32' -MemberDefinition @'
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
    [DllImport("user32.dll")] public static extern IntPtr LoadKeyboardLayout(string pwszKLID, uint Flags);
    [DllImport("user32.dll")] public static extern IntPtr GetKeyboardLayout(uint idThread);
    [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    public struct RECT { public int Left, Top, Right, Bottom; }
    public struct POINT { public int X, Y; }
'@
}

[void][CSWin32]::SetProcessDPIAware()

$VK = @{
    'F5'       = 0x74
    'F8'       = 0x77
    'Home'     = 0x24
    'End'      = 0x23
    'Insert'   = 0x2D
    'Delete'   = 0x2E
    'PageUp'   = 0x21
    'PageDown' = 0x22
    'Multiply' = 0x6A
    'Y'        = 0x59
    'T'        = 0x54
    'J'        = 0x4A
    'O'        = 0x4F
}
$ExtendedKeys = @('Home','End','Insert','Delete','PageUp','PageDown')

function Assert-GameForeground([System.Diagnostics.Process]$proc) {
    # Safety guard: never inject keys unless the foreground window really belongs to our game.
    $fg = [CSWin32]::GetForegroundWindow()
    $fgPid = 0
    [void][CSWin32]::GetWindowThreadProcessId($fg, [ref]$fgPid)
    if ($fgPid -ne $proc.Id) {
        throw "Foreground window belongs to PID $fgPid, not to the game (PID $($proc.Id)). Aborting key injection."
    }
}

function Focus-Game([System.Diagnostics.Process]$proc) {
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { throw "Game window handle not found." }
    if ([CSWin32]::IsIconic($h)) { [void][CSWin32]::ShowWindow($h, 9) }  # SW_RESTORE
    [void][CSWin32]::BringWindowToTop($h)
    [void][CSWin32]::SetForegroundWindow($h)
    # The project sets bShouldFlushPressedKeysOnViewportFocusLost=True, so the first key sent
    # right after a focus change gets swallowed. Give the viewport time to settle.
    Start-Sleep -Milliseconds 2500
    Assert-GameForeground $proc
    return $h
}

function Set-EnglishLayout([IntPtr]$hwnd) {
    # UE derives the FKey for letters from the CHAR code of the active layout, so a Russian
    # layout silently breaks letter keys. Ask the game window's thread to switch to en-US.
    $hkl = [CSWin32]::LoadKeyboardLayout('00000409', 1)   # KLF_ACTIVATE
    [void][CSWin32]::PostMessage($hwnd, 0x0050, [IntPtr]::Zero, $hkl)   # WM_INPUTLANGCHANGEREQUEST
    Start-Sleep -Milliseconds 600
    $ownerPid = 0
    $tid = [CSWin32]::GetWindowThreadProcessId($hwnd, [ref]$ownerPid)
    $cur = [CSWin32]::GetKeyboardLayout([uint32]$tid)
    $lang = ([int64]$cur) -band 0xFFFF
    Say ("keyboard layout of game window: 0x{0:X4} ({1})" -f $lang, $(if ($lang -eq 0x409) { 'en-US OK' } else { 'NOT en-US' }))
    return $lang
}

function Send-Key([System.Diagnostics.Process]$proc, [string]$name) {
    if (-not $VK.ContainsKey($name)) { throw "Unknown key $name" }
    Assert-GameForeground $proc
    $vk = [byte]$VK[$name]
    $scan = [byte]([CSWin32]::MapVirtualKey([uint32]$vk, 0))
    $flags = 0
    if ($ExtendedKeys -contains $name) { $flags = 1 }   # KEYEVENTF_EXTENDEDKEY
    [CSWin32]::keybd_event($vk, $scan, [uint32]$flags, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 90
    [CSWin32]::keybd_event($vk, $scan, [uint32]($flags -bor 2), [UIntPtr]::Zero)  # KEYEVENTF_KEYUP
    Say "key $name sent"
}

function Send-KeyUntilLogged([System.Diagnostics.Process]$proc, [string]$key, [string]$logPath, [string]$marker, [int]$tries = 3, [int]$waitSec = 6) {
    # Action keys (teleports, god mode) are only trusted when the game log says they fired.
    for ($i = 1; $i -le $tries; $i++) {
        Send-Key $proc $key
        Start-Sleep -Seconds $waitSec
        if ((Read-LogText $logPath) -match $marker) {
            Say "key $key confirmed by log ('$marker') on attempt $i"
            return $true
        }
        Say "key $key not confirmed yet (attempt $i of $tries)"
    }
    return $false
}

function Read-LogText([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return '' }
    try {
        $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $sr = New-Object System.IO.StreamReader($fs)
            return $sr.ReadToEnd()
        } finally { $fs.Dispose() }
    } catch { return '' }
}

function Wait-ForFile([string]$path, [int]$timeoutSec) {
    $t0 = Get-Date
    $lastSize = -1
    while (((Get-Date) - $t0).TotalSeconds -lt $timeoutSec) {
        if (Test-Path -LiteralPath $path) {
            $size = (Get-Item -LiteralPath $path).Length
            if ($size -gt 0 -and $size -eq $lastSize) { return $size }
            $lastSize = $size
        }
        Start-Sleep -Milliseconds 900
    }
    return -1
}

function Capture-WindowFallback([IntPtr]$hwnd, [string]$outPath) {
    # Fallback only: grab the game window client area straight off the desktop.
    # The process is DPI-aware, so these are physical pixels (no cropped frame).
    $r = New-Object 'CSWin32+RECT'
    [void][CSWin32]::GetClientRect($hwnd, [ref]$r)
    $p = New-Object 'CSWin32+POINT'
    $p.X = 0; $p.Y = 0
    [void][CSWin32]::ClientToScreen($hwnd, [ref]$p)
    $w = $r.Right - $r.Left
    $h = $r.Bottom - $r.Top
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($p.X, $p.Y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $g.Dispose()
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Say "fallback desktop capture -> $outPath ($w x $h)"
}

# ---------------------------------------------------------------- preflight
$uproject  = Join-Path $ProjectRoot 'ContrarySurvivor.uproject'
$editorExe = Join-Path $EngineRoot  'Engine\Binaries\Win64\UnrealEditor.exe'
$mapFile   = Join-Path $ProjectRoot ('Content\Maps\' + ($Map.Split('/')[-1]) + '.umap')
$outDir    = Join-Path $ProjectRoot 'Saved\Screenshots\GameFrames'
$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath   = Join-Path $ProjectRoot ("logs\gameshot-$stamp.log")
$inputIni  = Join-Path $ProjectRoot 'Saved\Config\WindowsEditor\Input.ini'
$iniBackup = "$inputIni.bak-$stamp"

foreach ($p in @($uproject, $editorExe, $mapFile)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Not found: $p" }
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logPath) | Out-Null

$busy = Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|cl|link|MSBuild)$' }
if ($busy) {
    throw ("Project is busy, these processes are running: " + (($busy | ForEach-Object { "$($_.ProcessName)($($_.Id))" }) -join ', '))
}

# ---------------------------------------------------------------- shot table
function New-Shot([string]$key, [string]$name, [int]$w, [int]$h) {
    $file = Join-Path $outDir ("{0}_{1}_{2}x{3}.png" -f $Tag, $name, $w, $h)
    [pscustomobject]@{
        Key  = $key
        Name = $name
        W    = $w
        H    = $h
        Path = $file
        Cmd  = ("HighResShot {0}x{1} filename={2}" -f $w, $h, ($file -replace '\\','/'))
    }
}

$shots = @(
    (New-Shot 'F5'       'start'        $WinResX   $WinResY),
    (New-Shot 'F8'       'start'        $PhoneResX $PhoneResY),
    (New-Shot 'Home'     'village'      $WinResX   $WinResY),
    (New-Shot 'End'      'village'      $PhoneResX $PhoneResY),
    (New-Shot 'PageDown' 'village2' $WinResX   $WinResY),
    (New-Shot 'PageUp'   'village2' $PhoneResX $PhoneResY),
    (New-Shot 'Insert'   'trader'       $WinResX   $WinResY),
    (New-Shot 'Delete'   'trader'       $PhoneResX $PhoneResY)
)

foreach ($s in $shots) { if (Test-Path -LiteralPath $s.Path) { Remove-Item -LiteralPath $s.Path -Force } }

# ---------------------------------------------------------------- launch
$execParts = @("r.HighResScreenshotDelay $ShotDelayFrames")
foreach ($s in $shots) { $execParts += ("setbind {0} '{1}'" -f $s.Key, $s.Cmd) }
$execParts += "setbind Multiply 'ShowHUD'"
$execCmds = ($execParts -join ', ')

$argLine = ('"{0}" {1} -game -windowed -ResX={2} -ResY={3} -WinX=0 -WinY=0 -nosplash -nosound -abslog={4} -ExecCmds="{5}"' -f `
            $uproject, $Map, $WinResX, $WinResY, $logPath, $execCmds)

if ($DryRun) {
    Say "DRY RUN - nothing is launched"
    Say "exe : $editorExe"
    Say "args: $argLine"
    foreach ($s in $shots) { Say ("bind {0,-6} -> {1}" -f $s.Key, $s.Cmd) }
    Say "log would be: $logPath"
    return
}

if (Test-Path -LiteralPath $inputIni) { Copy-Item -LiteralPath $inputIni -Destination $iniBackup -Force }

Say "launching: $editorExe $argLine"
$proc = Start-Process -FilePath $editorExe -ArgumentList $argLine -PassThru
Say "game PID = $($proc.Id), log = $logPath"

try {
    # ------------------------------------------------------------ wait: window
    $t0 = Get-Date
    while ($true) {
        if ($proc.HasExited) { throw "Game process exited early (code $($proc.ExitCode)). See $logPath" }
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break }
        if (((Get-Date) - $t0).TotalSeconds -gt $ReadyTimeoutSec) { throw "No game window after $ReadyTimeoutSec s." }
        Start-Sleep -Seconds 2
    }
    Say ("game window appeared after {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)

    # ------------------------------------------------------------ wait: level + intro handoff
    $handoff = $false
    $loadSeen = $null
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $ReadyTimeoutSec) {
        if ($proc.HasExited) { throw "Game process exited before the level was ready. See $logPath" }
        $log = Read-LogText $logPath
        if (-not $loadSeen -and $log -match 'LoadMap') { $loadSeen = Get-Date; Say "log: LoadMap seen" }
        if ($log -match 'intro control handed to player') { $handoff = $true; break }
        if ($loadSeen -and ((Get-Date) - $loadSeen).TotalSeconds -gt 60) {
            Say "log: no intro handoff line in 60 s after LoadMap - intro is probably disabled, continuing"
            break
        }
        Start-Sleep -Seconds 3
    }
    if ($handoff) { Say "log: intro finished, control is with the player" }
    if (-not $loadSeen) { throw "Level never reported LoadMap. See $logPath" }

    # ------------------------------------------------------------ wait: log quiet (shaders/streaming done)
    Say "waiting for the log to go quiet for $QuietSec s (shader compiles / streaming)"
    $lastLen = -1
    $quietSince = Get-Date
    $wentQuiet = $false
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt 180) {
        $len = 0
        if (Test-Path -LiteralPath $logPath) { $len = (Get-Item -LiteralPath $logPath).Length }
        if ($len -ne $lastLen) { $lastLen = $len; $quietSince = Get-Date }
        elseif (((Get-Date) - $quietSince).TotalSeconds -ge $QuietSec) { $wentQuiet = $true; break }
        Start-Sleep -Seconds 2
    }
    if ($wentQuiet) { Say ("log went quiet for {0} s, size = {1} bytes" -f $QuietSec, $lastLen) }
    else            { Say ("log NEVER went quiet in 180 s (size = {0} bytes) - continuing anyway" -f $lastLen) }
    Say ("soaking {0} s more" -f $SoakSec)
    Start-Sleep -Seconds $SoakSec

    # ------------------------------------------------------------ drive the game
    $hwnd = Focus-Game $proc
    $lang = Set-EnglishLayout $hwnd

    # Invulnerability so wolves cannot kill the character while we stand around taking shots.
    # J also force-enables the QA screen overlay, so O switches that overlay back off.
    if (-not $NoGodMode) {
        [void](Send-KeyUntilLogged $proc 'J' $logPath 'GODMODE on' 3 4)
        [void](Send-KeyUntilLogged $proc 'O' $logPath 'overlay off' 3 3)
    }

    $taken = @()

    # 1) where the character stands right after the intro
    foreach ($s in ($shots | Where-Object { $_.Name -eq 'start' })) {
        Send-Key $proc $s.Key
        $size = Wait-ForFile $s.Path $ShotWaitSec
        Say ("{0} {1}x{2} -> {3} ({4} bytes)" -f $s.Name, $s.W, $s.H, $s.Path, $size)
        if ($size -gt 0) { $taken += $s }
        Start-Sleep -Seconds 2
    }

    # 2) MAIN: the village (Y = teleport next to the elder, who stands in the village)
    $teleported = Send-KeyUntilLogged $proc 'Y' $logPath 'teleported to elder' 4 $SettleSec
    Say ("village teleport confirmed by log: {0}" -f $teleported)
    Start-Sleep -Seconds 3
    # EndIntro() forces the intro colour grade back to normal (SetIntroGradeAlpha(1.0f),
    # ContrarySurvivorPlayerController.cpp), so this line is the proof that the intro dimming
    # is gone and the frame shows the real post-process.
    $introEnded = [bool]((Read-LogText $logPath) -match 'intro ended \(entered village\)')
    Say ("intro fully ended (grade forced to normal) confirmed by log: {0}" -f $introEnded)
    foreach ($s in ($shots | Where-Object { $_.Name -eq 'village' })) {
        Send-Key $proc $s.Key
        $size = Wait-ForFile $s.Path $ShotWaitSec
        Say ("{0} {1}x{2} -> {3} ({4} bytes)" -f $s.Name, $s.W, $s.H, $s.Path, $size)
        if ($size -gt 0) { $taken += $s }
        Start-Sleep -Seconds 2
    }

    # 2b) same village view with the canvas HUD switched off (pure scene + post-process)
    # ShowHUD is a toggle and prints nothing to the log, so it is sent exactly once and the
    # resulting frame is checked by eye afterwards. No re-focus here: re-focusing the window
    # makes the viewport drop the next key (bShouldFlushPressedKeysOnViewportFocusLost=True).
    Send-Key $proc 'Multiply'
    Start-Sleep -Seconds 3
    foreach ($s in ($shots | Where-Object { $_.Name -eq 'village2' })) {
        Send-Key $proc $s.Key
        $size = Wait-ForFile $s.Path $ShotWaitSec
        Say ("{0} {1}x{2} -> {3} ({4} bytes)" -f $s.Name, $s.W, $s.H, $s.Path, $size)
        if ($size -gt 0) { $taken += $s }
        Start-Sleep -Seconds 2
    }

    # 3) second location: the trader booth (T)
    $atTrader = $true
    if (-not $NoTraderShots) {
        $atTrader = Send-KeyUntilLogged $proc 'T' $logPath 'teleported to trader' 4 $SettleSec
        Say ("trader teleport confirmed by log: {0}" -f $atTrader)
        foreach ($s in ($shots | Where-Object { $_.Name -eq 'trader' })) {
            Send-Key $proc $s.Key
            $size = Wait-ForFile $s.Path $ShotWaitSec
            Say ("{0} {1}x{2} -> {3} ({4} bytes)" -f $s.Name, $s.W, $s.H, $s.Path, $size)
            if ($size -gt 0) { $taken += $s }
            Start-Sleep -Seconds 2
        }
    }

    # Never keep a frame under a place name the log did not prove.
    if (-not $atTrader) {
        foreach ($s in ($shots | Where-Object { $_.Name -eq 'trader' })) {
            if (Test-Path -LiteralPath $s.Path) {
                Remove-Item -LiteralPath $s.Path -Force
                Say ("deleted unproven trader frame {0} (teleport never fired)" -f $s.Path)
            }
        }
    }
    if (-not $teleported) {
        foreach ($s in ($shots | Where-Object { $_.Name -like 'village*' })) {
            if (Test-Path -LiteralPath $s.Path) {
                $bad = $s.Path -replace '\.png$', '_TELEPORT_UNCONFIRMED.png'
                Move-Item -LiteralPath $s.Path -Destination $bad -Force
                Say "renamed unproven frame -> $bad"
            }
        }
    }

    # 4) if the in-game path produced nothing at all, grab the window off the desktop
    if ($taken.Count -eq 0) {
        Say "no in-game screenshot files appeared - using the desktop fallback"
        Focus-Game $proc | Out-Null
        $fb = Join-Path $outDir ("{0}_fallback_window.png" -f $Tag)
        Capture-WindowFallback $hwnd $fb
    }
}
finally {
    # ------------------------------------------------------------ shutdown + cleanup
    if ($proc -and -not $proc.HasExited) {
        Say "closing the game"
        [void]$proc.CloseMainWindow()
        if (-not $proc.WaitForExit(20000)) { $proc.Kill(); Say "game did not close, killed" }
    }
    Start-Sleep -Seconds 2
    if (Test-Path -LiteralPath $iniBackup) {
        Copy-Item -LiteralPath $iniBackup -Destination $inputIni -Force
        Remove-Item -LiteralPath $iniBackup -Force
        Say "restored $inputIni from backup (setbind writes DebugExecBindings into it)"
    } elseif (Test-Path -LiteralPath $inputIni) {
        Remove-Item -LiteralPath $inputIni -Force
        Say "removed generated $inputIni (there was none before the run)"
    }
}

Say "---- result ----"
Get-ChildItem -LiteralPath $outDir -Filter "$Tag*.png" | Sort-Object Name | ForEach-Object {
    Say ("FILE {0}  {1} bytes  {2}" -f $_.FullName, $_.Length, $_.LastWriteTime.ToString('HH:mm:ss'))
}
Say "log: $logPath"
