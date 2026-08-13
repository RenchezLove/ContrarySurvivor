<#
    shoot-main-menu.ps1

    Снимает ОДИН чистый кадр главного меню игры в высоком разрешении.

    Как снимается кадр (сверено по исходникам движка UE 5.5, не по памяти):
      * Меню — это UMG/Slate-виджет (StartScreenWidget->AddToViewport,
        ContrarySurvivorPlayerController.cpp:866), поэтому HighResShot не годится:
        он рендерит только СЦЕНУ в отдельный буфер, интерфейс туда не попадает.
      * Консольная команда "shot showui" разбирается в
        UGameViewportClient::HandleScreenshotCommand (GameViewportClient.cpp:3922) и при
        showui уходит в FSlateApplication::TakeScreenshot(окно) (GameViewportClient.cpp:2150-2164),
        то есть кадр берётся ИЗ БУФЕРА ОКНА ИГРЫ, а не с рабочего стола. Значит в кадр не
        попадает ничего нарисованного поверх окна операционной системой (в том числе
        водяной знак "Активация Windows"). Файл кладётся в Saved/Screenshots/WindowsEditor
        (UnrealClient.cpp:279-288, имя ScreenShotNNNNN.png), скрипт копирует его под
        постоянным именем.
      * Команду НЕ приходится биндить самим: движок держит готовую отладочную привязку
        +DebugExecBindings=(Key=F9,Command="shot showui") в Engine/Config/BaseInput.ini:44.
        На загрузочном уровне L_Boot у игры нет персонажа (ContrarySurvivorPlayerController.cpp:861),
        поэтому игровых привязок к F9 там нет и нажатие уходит только в отладочную.
        Свой setbind не используем — значит Saved/Config/.../Input.ini не переписывается.
      * "r.setres <W>x<H>w" переводит окно в оконный режим нужного размера: в сохранённых
        настройках игрока (Saved/Config/WindowsEditor/GameUserSettings.ini) может стоять
        безрамочный полноэкранный режим, который перебивает ключи -windowed -ResX -ResY.
        ВАЖНО: размер в этой команде задаётся в логических единицах рабочего стола, а экран
        масштабирован (150%), поэтому просят 1707x960, а окно выходит физическим 2560x1440.

    Готовность меню доказывается журналом игры, а не на глаз:
      * строка "QA: start screen OPEN" (ContrarySurvivorPlayerController.cpp:894);
      * затем журнал должен молчать -QuietSec секунд (шейдеры/загрузка закончились);
      * затем ещё -SoakSec секунд запаса.

    Побочный эффект, который скрипт убирает за собой: смена разрешения может задеть
    Saved/Config/WindowsEditor/GameUserSettings.ini — файл сохраняется до запуска и
    возвращается после.

    Курсор мыши уводится за пределы меню, чтобы ни один пункт не был подсвечен наведением.
#>

[CmdletBinding()]
param(
    [string]$ProjectRoot   = 'E:\ContrarySurvior\ContrarySurvivor',
    [string]$EngineRoot    = 'E:\UnrealEngine\UE_5.5',
    [int]$WinResX          = 2560,   # желаемый ФИЗИЧЕСКИЙ размер кадра
    [int]$WinResY          = 1440,
    [double]$DpiScale      = 1.5,    # масштаб экрана Windows (2560 физических = 1707 логических)
    [int]$ReadyTimeoutSec  = 420,
    [int]$QuietSec         = 10,
    [int]$SoakSec          = 5,
    [int]$ShotWaitSec      = 60,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Say([string]$msg) { Write-Host ("MENU| " + $msg) }

if (-not ("CSMenuWin32" -as [type])) {
Add-Type -Namespace '' -Name 'CSMenuWin32' -MemberDefinition @'
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint uCode, uint uMapType);
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    public struct RECT { public int Left, Top, Right, Bottom; }
    public struct POINT { public int X, Y; }
'@
}
[void][CSMenuWin32]::SetProcessDPIAware()

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

function Test-GameForeground([System.Diagnostics.Process]$proc) {
    $fg = [CSMenuWin32]::GetForegroundWindow()
    $fgPid = 0
    [void][CSMenuWin32]::GetWindowThreadProcessId($fg, [ref]$fgPid)
    return ($fgPid -eq $proc.Id)
}

function Focus-Game([System.Diagnostics.Process]$proc) {
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { throw "Окно игры не найдено." }
    for ($i = 1; $i -le 6; $i++) {
        if ([CSMenuWin32]::IsIconic($h)) { [void][CSMenuWin32]::ShowWindow($h, 9) }  # SW_RESTORE
        # Короткое нажатие ALT снимает блокировку смены переднего плана в Windows,
        # иначе SetForegroundWindow из фонового процесса молча ничего не делает.
        [CSMenuWin32]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
        [CSMenuWin32]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
        [void][CSMenuWin32]::BringWindowToTop($h)
        [void][CSMenuWin32]::SetForegroundWindow($h)
        Start-Sleep -Milliseconds 1200
        if (Test-GameForeground $proc) { Say "окно игры выведено на передний план (попытка $i)"; return $h }
    }
    throw "Не удалось вывести окно игры на передний план."
}

function Send-Key([System.Diagnostics.Process]$proc, [byte]$vk) {
    # Клавишу отправляем только когда переднее окно точно принадлежит игре, иначе нажатие
    # уйдёт в чужое приложение. Если передний план успел уйти — возвращаем его и проверяем снова.
    if (-not (Test-GameForeground $proc)) { [void](Focus-Game $proc) }
    if (-not (Test-GameForeground $proc)) { throw "Переднее окно не принадлежит игре — нажатие не отправляю." }
    $scan = [byte]([CSMenuWin32]::MapVirtualKey([uint32]$vk, 0))
    [CSMenuWin32]::keybd_event($vk, $scan, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 90
    [CSMenuWin32]::keybd_event($vk, $scan, 2, [UIntPtr]::Zero)   # KEYEVENTF_KEYUP
    Say ("нажата клавиша (код {0})" -f $vk)
}

function Get-ClientBox([IntPtr]$hwnd) {
    $r = New-Object 'CSMenuWin32+RECT'
    [void][CSMenuWin32]::GetClientRect($hwnd, [ref]$r)
    $p = New-Object 'CSMenuWin32+POINT'
    $p.X = 0; $p.Y = 0
    [void][CSMenuWin32]::ClientToScreen($hwnd, [ref]$p)
    [pscustomobject]@{ X = $p.X; Y = $p.Y; W = ($r.Right - $r.Left); H = ($r.Bottom - $r.Top) }
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

function Capture-Client([IntPtr]$hwnd, [string]$outPath) {
    # Запасной путь: пиксели снимаются с рабочего стола, поэтому сюда попадает всё,
    # что операционная система рисует поверх окна.
    $b = Get-ClientBox $hwnd
    if ($b.W -le 0 -or $b.H -le 0) { throw "Пустая клиентская область окна: $($b.W)x$($b.H)" }
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap($b.W, $b.H)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.X, $b.Y, 0, 0, (New-Object System.Drawing.Size($b.W, $b.H)))
    $g.Dispose()
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Say ("запасной снимок с рабочего стола -> {0} ({1}x{2})" -f $outPath, $b.W, $b.H)
}

# ---------------------------------------------------------------- подготовка
$uproject  = Join-Path $ProjectRoot 'ContrarySurvivor.uproject'
$editorExe = Join-Path $EngineRoot  'Engine\Binaries\Win64\UnrealEditor.exe'
$bootMap   = Join-Path $ProjectRoot 'Content\Maps\L_Boot.umap'
$outDir    = Join-Path $ProjectRoot 'Saved\Screenshots\GameFrames'
$stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
$logPath   = Join-Path $ProjectRoot ("logs\menushot-$stamp.log")
$outPng    = Join-Path $outDir ("mainmenu_{0}.png" -f $stamp)
$fbPng     = Join-Path $outDir ("mainmenu_{0}_fallback.png" -f $stamp)

$savedCfg  = Join-Path $ProjectRoot 'Saved\Config\WindowsEditor'
$userIni   = Join-Path $savedCfg 'GameUserSettings.ini'
$engineDir = Join-Path $ProjectRoot 'Saved\Screenshots\WindowsEditor'   # куда кладёт кадр сам движок

foreach ($p in @($uproject, $editorExe, $bootMap)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "Не найдено: $p" }
}
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logPath) | Out-Null

$busy = Get-Process -ErrorAction SilentlyContinue |
        Where-Object { $_.ProcessName -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool|cl|link|MSBuild)$' }
if ($busy) {
    throw ("Проект занят, работают процессы: " + (($busy | ForEach-Object { "$($_.ProcessName)($($_.Id))" }) -join ', '))
}

# Карту в командной строке НЕ передаём: игра сама грузит GameDefaultMap=/Game/Maps/L_Boot
# (Config/DefaultEngine.ini:122) — так кадр соответствует реальному старту игры.
$logResX  = [int][Math]::Round($WinResX / $DpiScale)
$logResY  = [int][Math]::Round($WinResY / $DpiScale)
$execCmds = ("r.setres {0}x{1}w" -f $logResX, $logResY)
$argLine  = ('"{0}" -game -windowed -ResX={1} -ResY={2} -WinX=0 -WinY=0 -nosplash -nosound -abslog={3} -ExecCmds="{4}"' -f `
             $uproject, $logResX, $logResY, $logPath, $execCmds)

if ($DryRun) {
    Say "СУХОЙ ПРОГОН — ничего не запускается"
    Say "exe : $editorExe"
    Say "args: $argLine"
    Say "кадр был бы здесь: $outPng"
    return
}

if (Test-Path -LiteralPath $userIni) { Copy-Item -LiteralPath $userIni -Destination "$userIni.bak-$stamp" -Force }
if (Test-Path -LiteralPath $outPng) { Remove-Item -LiteralPath $outPng -Force }

# Запоминаем, какие кадры лежали у движка ДО прогона, чтобы потом узнать новый.
New-Item -ItemType Directory -Force -Path $engineDir | Out-Null
$before = @(Get-ChildItem -LiteralPath $engineDir -Filter '*.png' -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName })

Say "запуск: $editorExe $argLine"
$proc = Start-Process -FilePath $editorExe -ArgumentList $argLine -PassThru
Say "PID игры = $($proc.Id), журнал = $logPath"

try {
    # ------------------------------------------------------------ ждём окно
    $t0 = Get-Date
    while ($true) {
        if ($proc.HasExited) { throw "Процесс игры завершился раньше времени (код $($proc.ExitCode)). Журнал: $logPath" }
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break }
        if (((Get-Date) - $t0).TotalSeconds -gt $ReadyTimeoutSec) { throw "Окно игры не появилось за $ReadyTimeoutSec с." }
        Start-Sleep -Seconds 2
    }
    Say ("окно игры появилось через {0:N0} с" -f ((Get-Date) - $t0).TotalSeconds)

    # ------------------------------------------------------------ ждём открытия меню (по журналу)
    $menuOpen = $false
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $ReadyTimeoutSec) {
        if ($proc.HasExited) { throw "Процесс игры завершился до открытия меню. Журнал: $logPath" }
        if ((Read-LogText $logPath) -match 'QA: start screen OPEN') { $menuOpen = $true; break }
        Start-Sleep -Seconds 3
    }
    if (-not $menuOpen) { throw "В журнале так и не появилась строка 'QA: start screen OPEN'. Журнал: $logPath" }
    Say ("журнал: 'QA: start screen OPEN' — меню открыто (через {0:N0} с)" -f ((Get-Date) - $t0).TotalSeconds)

    # ------------------------------------------------------------ ждём тишины в журнале
    Say "жду, пока журнал замолчит на $QuietSec с (компиляция шейдеров / загрузка)"
    $lastLen = -1
    $quietSince = Get-Date
    $wentQuiet = $false
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt 300) {
        $len = 0
        if (Test-Path -LiteralPath $logPath) { $len = (Get-Item -LiteralPath $logPath).Length }
        if ($len -ne $lastLen) { $lastLen = $len; $quietSince = Get-Date }
        elseif (((Get-Date) - $quietSince).TotalSeconds -ge $QuietSec) { $wentQuiet = $true; break }
        Start-Sleep -Seconds 2
    }
    if ($wentQuiet) { Say ("журнал молчит {0} с, размер = {1} байт" -f $QuietSec, $lastLen) }
    else            { Say ("журнал НЕ замолчал за 300 с (размер = {0} байт) — продолжаю" -f $lastLen) }
    Say ("запас ещё {0} с" -f $SoakSec)
    Start-Sleep -Seconds $SoakSec

    # ------------------------------------------------------------ съёмка
    $hwnd = Focus-Game $proc
    $box = Get-ClientBox $hwnd
    Say ("клиентская область окна: {0}x{1} в точке {2},{3}" -f $box.W, $box.H, $box.X, $box.Y)

    # Курсор паркуем ВНУТРИ окна, в пустой левый нижний угол кадра: там нет ни одного пункта
    # меню, значит подсветки наведением не будет. Наружу уводить нельзя — у нижнего края
    # экрана панель задач, и окно игры теряет передний план.
    [void][CSMenuWin32]::SetCursorPos(($box.X + 25), ($box.Y + $box.H - 25))
    Start-Sleep -Seconds 2

    # F9 — готовая отладочная привязка движка на "shot showui" (Engine/Config/BaseInput.ini:44).
    $engineShot = $null
    for ($try = 1; $try -le 3 -and -not $engineShot; $try++) {
        Send-Key $proc 0x78   # VK_F9
        $t0 = Get-Date
        while (((Get-Date) - $t0).TotalSeconds -lt ($ShotWaitSec / 3)) {
            $fresh = @(Get-ChildItem -LiteralPath $engineDir -Filter '*.png' -ErrorAction SilentlyContinue |
                       Where-Object { $before -notcontains $_.FullName })
            if ($fresh.Count -gt 0) {
                $cand = $fresh | Sort-Object LastWriteTime | Select-Object -Last 1
                $sz = Wait-ForFile $cand.FullName 30
                if ($sz -gt 0) { $engineShot = $cand.FullName; break }
            }
            Start-Sleep -Milliseconds 900
        }
        if (-not $engineShot) { Say "после нажатия $try кадр не появился" }
    }

    if ($engineShot) {
        Copy-Item -LiteralPath $engineShot -Destination $outPng -Force
        Say ("кадр движка: {0} -> {1}" -f $engineShot, $outPng)
    } else {
        Say "кадр движка не появился — снимаю запасной вариант с рабочего стола"
        Capture-Client $hwnd $fbPng
    }
}
finally {
    if ($proc -and -not $proc.HasExited) {
        Say "закрываю игру"
        [void]$proc.CloseMainWindow()
        if (-not $proc.WaitForExit(20000)) { $proc.Kill(); Say "игра не закрылась, снял процесс принудительно" }
    }
    Start-Sleep -Seconds 2
    $bak = "$userIni.bak-$stamp"
    if (Test-Path -LiteralPath $bak) {
        Copy-Item -LiteralPath $bak -Destination $userIni -Force
        Remove-Item -LiteralPath $bak -Force
        Say "восстановлен $userIni из резервной копии"
    }
}

Say "---- результат ----"
foreach ($f in @($outPng, $fbPng)) {
    if (Test-Path -LiteralPath $f) {
        $i = Get-Item -LiteralPath $f
        Say ("ФАЙЛ {0}  {1} байт  {2}" -f $i.FullName, $i.Length, $i.LastWriteTime.ToString('HH:mm:ss'))
    }
}
Say "журнал: $logPath"
