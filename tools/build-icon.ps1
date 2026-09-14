# Rebuild the Windows icon from the same open-ring/star geometry as the SVG.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$output = Join-Path $PSScriptRoot '../ui/icons/opengod.ico'
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()
foreach ($size in $sizes) {
  $bitmap = [Drawing.Bitmap]::new($size, $size)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $graphics.ScaleTransform($size / 64.0, $size / 64.0)
  $background = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#171a29'))
  $star = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#e8eaff'))
  $pen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#c0c5ff'), 6)
  $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
  $shape = [Drawing.Drawing2D.GraphicsPath]::new()
  foreach ($corner in @(@(0,0,180), @(28,0,270), @(28,28,0), @(0,28,90))) {
    $shape.AddArc($corner[0], $corner[1], 36, 36, $corner[2], 90)
  }
  $shape.CloseFigure()
  $graphics.FillPath($background, $shape)
  $graphics.DrawArc($pen, 12, 12, 40, 40, -53, -298)
  $graphics.DrawLine($pen, 52, 35, 33, 35)
  $points = [Drawing.PointF[]]@([Drawing.PointF]::new(49,7), [Drawing.PointF]::new(51.8,14.2), [Drawing.PointF]::new(59,17), [Drawing.PointF]::new(51.8,19.8), [Drawing.PointF]::new(49,27), [Drawing.PointF]::new(46.2,19.8), [Drawing.PointF]::new(39,17), [Drawing.PointF]::new(46.2,14.2))
  $graphics.FillPolygon($star, $points)
  $stream = [IO.MemoryStream]::new()
  $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
  $images += ,$stream.ToArray()
  $stream.Dispose(); $shape.Dispose(); $pen.Dispose(); $star.Dispose(); $background.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
$file = [IO.File]::Create($output)
$writer = [IO.BinaryWriter]::new($file)
try {
  $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
  $offset = 6 + 16 * $sizes.Count
  for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dimension = $sizes[$i] % 256
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
    $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
  }
  foreach ($bytes in $images) { $writer.Write([byte[]]$bytes) }
} finally { $writer.Dispose(); $file.Dispose() }
