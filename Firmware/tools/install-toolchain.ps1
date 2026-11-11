[CmdletBinding()]
param([switch]$DownloadDatasheets)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$cache = Join-Path $root '.tools/firmware-downloads'
$destination = Join-Path $root '.tools/avr'
New-Item -ItemType Directory -Force -Path $cache,$destination,(Join-Path $root 'Input/Datasheets') | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$url = 'https://downloads.arduino.cc/tools/avr-gcc-7.3.0-atmel3.6.1-arduino7-i686-w64-mingw32.zip'
$expected = 'a54f64755fff4cb792a1495e5defdd789902a2a3503982e81b898299cf39800e'
$archive = Join-Path $cache 'avr-gcc-7.3.0-arduino7-windows.zip'
if (-not (Test-Path -LiteralPath (Join-Path $destination 'avr/bin/avr-gcc.exe'))) {
    Write-Host 'Downloading the official Arduino AVR-GCC distribution...'
    Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive -TimeoutSec 180
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) { throw 'AVR archive SHA256 mismatch.' }
    Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
}
$references = @{
    'ATmega8_L_datasheet.pdf' = 'https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-2486-8-bit-AVR-microcontroller-ATmega8_L_datasheet.pdf';
    'ST_L78M_datasheet.pdf' = 'https://www.st.com/resource/en/datasheet/l78m.pdf';
    'ATmega8A_datasheet.pdf' = 'https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega8A-Data-Sheet-DS40001974B.pdf'
}
& (Join-Path $destination 'avr/bin/avr-gcc.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'AVR compiler verification failed.' }
if ($DownloadDatasheets) {
    foreach ($name in $references.Keys) {
        $path = Join-Path $root ('Input/Datasheets/' + $name)
        if (-not (Test-Path -LiteralPath $path)) {
            try { Invoke-WebRequest -UseBasicParsing -Uri $references[$name] -OutFile ($path + '.download') -TimeoutSec 45; Move-Item -LiteralPath ($path + '.download') -Destination $path }
            catch { Write-Warning "Optional datasheet download failed: $name. Source URL: $($references[$name]). Compiler installation remains valid." }
        }
    }
}
Write-Host 'Toolchain installed locally. No programmer or fuse operation was performed.'
