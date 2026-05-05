Add-Type -AssemblyName System.Drawing

$pngPath = Join-Path $PSScriptRoot "wingman.png"
$icoPath = Join-Path $PSScriptRoot "wingman.ico"

$png = [System.Drawing.Image]::FromFile((Resolve-Path $pngPath))

# Create ICO file with multiple sizes
$icoStream = [System.IO.File]::Create($icoPath)

# 16x16
$icon16 = New-Object System.Drawing.Bitmap(16, 16)
$g = [System.Drawing.Graphics]::FromImage($icon16)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($png, 0, 0, 16, 16)
$g.Dispose()
$icon16.Save($icon16.BaseName + "_16.png")

# 32x32
$icon32 = New-Object System.Drawing.Bitmap(32, 32)
$g = [System.Drawing.Graphics]::FromImage($icon32)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($png, 0, 0, 32, 32)
$g.Dispose()

# Create ICO from bitmap
$hicon = $icon32.GetHicon()
$ico = [System.Drawing.Icon]::FromHandle($hicon)
$file = [System.IO.FileStream]::new($icoPath, [System.IO.FileMode]::Create)
$ico.Save($file)
$file.Close()

$png.Dispose()
Write-Host "Icon created: $icoPath"
