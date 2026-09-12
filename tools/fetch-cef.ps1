$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$name = 'cef_binary_152.0.6+g708dc14+chromium-152.0.7977.83_windows64_minimal'
$directory = Join-Path $project 'third_party'
New-Item -ItemType Directory -Force $directory | Out-Null
$archive = Join-Path $directory 'cef.tar.bz2'
if (-not (Test-Path $archive)) {
    Invoke-WebRequest ('https://cef-builds.spotifycdn.com/' + $name.Replace('+','%2B') + '.tar.bz2') -OutFile $archive
}
if ((Get-FileHash $archive -Algorithm SHA1).Hash -ne 'e5e3020627f4528bd43e22f4c4970000b0458e99') {
    throw 'CEF archive checksum mismatch. Remove the archive and retry.'
}
if (-not (Test-Path (Join-Path $directory "$name/include"))) {
    tar -xf $archive -C $directory
    if ($LASTEXITCODE -ne 0) { throw 'CEF extraction failed' }
}
Write-Output (Join-Path $directory $name)
