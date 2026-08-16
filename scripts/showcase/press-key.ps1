# Нажать клавишу в окне игры (запущенной launch.sh) — для съёмки экранов, которых нет в QA-командах
# (например меню паузы: Escape). Использование: powershell -File press-key.ps1 "{ESC}" [заголовок окна]
param([string]$Keys = "{ESC}", [string]$Title = "ContrarySurvivor")
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
"@
$p = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { Write-Output "NOWINDOW"; exit 1 }
[W]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait($Keys)
Write-Output ("SENT {0} -> {1} (pid {2})" -f $Keys, $p.MainWindowTitle, $p.Id)
