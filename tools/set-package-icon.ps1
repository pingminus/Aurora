param([Parameter(Mandatory)][string]$Executable, [Parameter(Mandatory)][string]$Icon)
$ErrorActionPreference = 'Stop'
# Change only resources in the packaged copy; leave the CEF SDK bootstrap intact.
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class OpenGodIconResources {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
  public static extern IntPtr BeginUpdateResource(string file, bool deleteExisting);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool UpdateResource(IntPtr update, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool EndUpdateResource(IntPtr update, bool discard);
}
'@
$data = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Icon))
$count = [BitConverter]::ToUInt16($data, 4)
$group = [IO.MemoryStream]::new()
$writer = [IO.BinaryWriter]::new($group)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write($count)
$update = [OpenGodIconResources]::BeginUpdateResource((Resolve-Path -LiteralPath $Executable), $false)
if ($update -eq [IntPtr]::Zero) { throw 'Cannot open packaged executable resources' }
try {
  for ($i = 0; $i -lt $count; $i++) {
    $entry = 6 + 16 * $i
    $length = [BitConverter]::ToUInt32($data, $entry + 8)
    $offset = [BitConverter]::ToUInt32($data, $entry + 12)
    $bytes = [byte[]]::new($length)
    [Array]::Copy($data, $offset, $bytes, 0, $length)
    if (![OpenGodIconResources]::UpdateResource($update, [IntPtr]3, [IntPtr]($i + 1), 0, $bytes, $length)) { throw 'Cannot write icon image' }
    $writer.Write($data, $entry, 12); $writer.Write([uint16]($i + 1))
  }
  $bytes = $group.ToArray()
  if (![OpenGodIconResources]::UpdateResource($update, [IntPtr]14, [IntPtr]1, 0, $bytes, $bytes.Length)) { throw 'Cannot write icon group' }
  if (![OpenGodIconResources]::EndUpdateResource($update, $false)) { throw 'Cannot save packaged icon' }
  $update = [IntPtr]::Zero
} finally {
  if ($update -ne [IntPtr]::Zero) { [void][OpenGodIconResources]::EndUpdateResource($update, $true) }
  $writer.Dispose(); $group.Dispose()
}
