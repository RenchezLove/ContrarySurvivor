<#
    shoot-intro-series.ps1

    Запускает игру отдельным процессом и снимает СЕРИЮ кадров экрана во время вступления,
    чтобы можно было глазами проверить: чёрный экран с титрами -> плавное проявление картинки.

    Снимок делается с экрана (окно игры выводится на передний план), с учётом масштабирования
    Windows (иначе кадр обрезается). Сохранение в Saved/Screenshots/IntroSeries.
#>

[CmdletBinding()]
param(
    [string]$ProjectRoot = 'E:\ContrarySurvior\ContrarySurvivor',
    [string]$EngineRoot  = 'E:\UnrealEngine\UE_5.5',
    [string]$Map         = '/Game/Maps/L_World_C',
    [int]$ResX           = 1280,
    [int]$ResY           = 720,
    [double]$IntervalSec = 0.7,
    [int]$DurationSec    = 22,
    [int]$WarmupTimeoutSec = 300
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinApi {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
}
"@
[void][WinApi]::SetProcessDPIAware()

$outDir = Join-Path $ProjectRoot 'Saved\Screenshots\IntroSeries'
if (Test-Path $outDir) { Remove-Item "$outDir\*.png" -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$logPath = Join-Path $ProjectRoot 'Saved\Logs\ContrarySurvivor.log'
if (Test-Path $logPath) { Remove-Item $logPath -Force -ErrorAction SilentlyContinue }

$exe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$uproject = Join-Path $ProjectRoot 'ContrarySurvivor.uproject'
$args = @("`"$uproject`"", $Map, '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-nosplash')

Write-Host "Запуск игры..."
$proc = Start-Process -FilePath $exe -ArgumentList $args -PassThru

# Ждём появления окна игры
$deadline = (Get-Date).AddSeconds($WarmupTimeoutSec)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
    if ($proc.HasExited) { throw "Процесс игры завершился раньше времени." }
    if ($proc.MainWindowHandle -ne [IntPtr]::Zero) { break }
}
if ($proc.MainWindowHandle -eq [IntPtr]::Zero) { throw "Окно игры не появилось за $WarmupTimeoutSec с." }

# Ждём, пока карта реально загрузится: строка LoadMap в логе игры
$mapLoaded = $false
while ((Get-Date) -lt $deadline) {
    if (Test-Path $logPath) {
        $txt = Get-Content $logPath -Raw -ErrorAction SilentlyContinue
        if ($txt -match 'LoadMap:.*L_World_C') { $mapLoaded = $true; break }
    }
    Start-Sleep -Milliseconds 500
}
Write-Host "Карта загружена: $mapLoaded"

[void][WinApi]::ShowWindow($proc.MainWindowHandle, 5)
[void][WinApi]::SetForegroundWindow($proc.MainWindowHandle)
Start-Sleep -Milliseconds 800

$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$n = 0
$stop = (Get-Date).AddSeconds($DurationSec)
$t0 = Get-Date
while ((Get-Date) -lt $stop) {
    $n++
    $ms = [int]((Get-Date) - $t0).TotalMilliseconds
    $bmp = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    $gfx.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bounds.Size)
    $name = 'intro_{0:d2}_{1:d5}ms.png' -f $n, $ms
    $bmp.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $gfx.Dispose(); $bmp.Dispose()
    Start-Sleep -Milliseconds ([int]($IntervalSec * 1000))
}

Write-Host "Снято кадров: $n"
Start-Sleep -Seconds 1
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
Write-Host "Готово. Папка: $outDir"
