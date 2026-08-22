# Генерация набора иконок Android из одной квадратной картинки.
# Схема из Build/Android/res (см. комментарии в icon.xml):
#   drawable*/icon.png  — legacy, простое масштабирование (48/72/96/144/192 + drawable 192)
#   drawable-*/icon_fg.png — адаптивный передний слой: холст залит цветом фона,
#       картинка в центральных 2/3 (маска лаунчера показывает центр)
#   values/icon_colors.xml — цвет фона = среднее цветов периметра картинки
param(
    [string]$Source,
    [string]$ResDir
)
Add-Type -AssemblyName System.Drawing

$src = New-Object System.Drawing.Bitmap($Source)

# Средний цвет периметра (шаг 10 пикселей)
$r = 0; $g = 0; $b = 0; $n = 0
$step = 10
for ($x = 0; $x -lt $src.Width; $x += $step) {
    foreach ($y in @(0, ($src.Height - 1))) {
        $c = $src.GetPixel($x, $y); $r += $c.R; $g += $c.G; $b += $c.B; $n++
    }
}
for ($y = 0; $y -lt $src.Height; $y += $step) {
    foreach ($x in @(0, ($src.Width - 1))) {
        $c = $src.GetPixel($x, $y); $r += $c.R; $g += $c.G; $b += $c.B; $n++
    }
}
$bgColor = [System.Drawing.Color]::FromArgb(255, [int]($r/$n), [int]($g/$n), [int]($b/$n))
$bgHex = "#{0:X2}{1:X2}{2:X2}" -f $bgColor.R, $bgColor.G, $bgColor.B
Write-Output "BG_COLOR=$bgHex"

function Save-Scaled([System.Drawing.Bitmap]$img, [int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $gr = [System.Drawing.Graphics]::FromImage($bmp)
    $gr.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gr.DrawImage($img, 0, 0, $size, $size)
    $gr.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output ("icon {0} -> {1}" -f $size, $path)
}

function Save-Fg([System.Drawing.Bitmap]$img, [int]$size, [System.Drawing.Color]$bg, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $gr = [System.Drawing.Graphics]::FromImage($bmp)
    $gr.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gr.Clear($bg)
    $inner = [int]($size * 2 / 3)
    $off = [int](($size - $inner) / 2)
    $gr.DrawImage($img, $off, $off, $inner, $inner)
    $gr.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output ("fg {0} -> {1}" -f $size, $path)
}

Save-Scaled $src 192 "$ResDir/drawable/icon.png"
Save-Scaled $src 48  "$ResDir/drawable-mdpi/icon.png"
Save-Scaled $src 72  "$ResDir/drawable-hdpi/icon.png"
Save-Scaled $src 96  "$ResDir/drawable-xhdpi/icon.png"
Save-Scaled $src 144 "$ResDir/drawable-xxhdpi/icon.png"
Save-Scaled $src 192 "$ResDir/drawable-xxxhdpi/icon.png"

Save-Fg $src 108 $bgColor "$ResDir/drawable-mdpi/icon_fg.png"
Save-Fg $src 162 $bgColor "$ResDir/drawable-hdpi/icon_fg.png"
Save-Fg $src 216 $bgColor "$ResDir/drawable-xhdpi/icon_fg.png"
Save-Fg $src 324 $bgColor "$ResDir/drawable-xxhdpi/icon_fg.png"
Save-Fg $src 432 $bgColor "$ResDir/drawable-xxxhdpi/icon_fg.png"

$src.Dispose()

# Обновить цвет фона в icon_colors.xml (сохранить комментарии файла)
$colorsPath = "$ResDir/values/icon_colors.xml"
$xmlText = Get-Content -Raw -Encoding UTF8 $colorsPath
$xmlText = $xmlText -replace '(<color name="contrary_icon_bg">)#[0-9A-Fa-f]{6}(</color>)', ('${1}' + $bgHex + '${2}')
Set-Content -Encoding UTF8 -NoNewline $colorsPath $xmlText
Write-Output "icon_colors.xml -> $bgHex"
