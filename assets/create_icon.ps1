Add-Type -AssemblyName System.Drawing

$pngPath = Join-Path $PSScriptRoot "wingman.png"
$icoPath = Join-Path $PSScriptRoot "wingman.ico"

# Load PNG
$png = [System.Drawing.Image]::FromFile((Resolve-Path $pngPath))

# Create ICO file with multiple sizes
$icoStream = [System.IO.File]::Create($icoPath)

# Sizes to include
$sizes = @(16, 32, 48)

# Write ICO header
$iconDir = New-Object byte[] 6
[BitConverter]::GetBytes([UInt16]0).CopyTo($iconDir, 0)      # Reserved
[BitConverter]::GetBytes([UInt16]1).CopyTo($iconDir, 2)      # Type: 1 = icon
[BitConverter]::GetBytes([UInt16]$sizes.Count).CopyTo($iconDir, 4)  # Count
$icoStream.Write($iconDir, 0, 6)

$imageData = @()
$offset = 6 + (16 * $sizes.Count)

foreach ($size in $sizes) {
    # Create bitmap at this size
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($png, 0, 0, $size, $size)
    $g.Dispose()

    # Convert to ICO format (PNG data for Vista+)
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $ms.ToArray()
    $imageData += ,$bytes

    # Write directory entry
    $entry = New-Object byte[] 16
    $entry[0] = [byte]$size
    $entry[1] = [byte]$size
    [BitConverter]::GetBytes([UInt16]1).CopyTo($entry, 2)   # Color planes
    [BitConverter]::GetBytes([UInt16]32).CopyTo($entry, 4) # Bits per pixel
    [BitConverter]::GetBytes([UInt32]$bytes.Length).CopyTo($entry, 8)   # Size
    [BitConverter]::GetBytes([UInt32]$offset).CopyTo($entry, 12)       # Offset
    $icoStream.Write($entry, 0, 16)

    $offset += $bytes.Length
    $bmp.Dispose()
}

# Write image data
foreach ($data in $imageData) {
    $icoStream.Write($data, 0, $data.Length)
}

$icoStream.Close()
$png.Dispose()

Write-Host "Icon created: $icoPath"
