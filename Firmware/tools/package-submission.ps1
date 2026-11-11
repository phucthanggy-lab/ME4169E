[CmdletBinding()]
param(
    [switch]$WithoutInput,
    [ValidateSet('Release','Development')][string]$Profile = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Split-Path -Parent (Split-Path -Parent $PSScriptRoot)))
$buildRelative = 'Firmware/build/' + $Profile.ToLowerInvariant()
$manifestPath = Join-Path $root ($buildRelative + '/manifest.json')
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Build first: Firmware/build.ps1' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.profile -ne $Profile -or $manifest.motorOutputsLocked -ne ($Profile -eq 'Development') -or $manifest.syntheticSelfTest -or $manifest.syntheticApplication) {
    throw 'Package requires a normal application with the selected profile and corresponding motor-output state.'
}
if ($manifest.mcuTarget -ne 'atmega8' -or $manifest.effectiveConfiguration.ENCODER_MODE -ne 4) {
    throw 'Primary build folder must contain the ATmega8 quadrature application. Rebuild without -EncoderMode HallRising.'
}
$files = [ordered]@{}
foreach ($file in $manifest.sourceFiles) {
    if ($file.path -notmatch '^Firmware/(src/|include/|tools/|build\.ps1$|Makefile$)' -or $file.path -match '(^|/)\.\.(/|$)') {
        throw "Unexpected firmware source: $($file.path)"
    }
    $files[$file.path] = $file.sha256
}
$currentSources = @(Get-ChildItem (Join-Path $root 'Firmware/src'),(Join-Path $root 'Firmware/include'),(Join-Path $root 'Firmware/tools') -File -Recurse; Get-Item (Join-Path $root 'Firmware/build.ps1'),(Join-Path $root 'Firmware/Makefile'))
if ($currentSources.Count -ne @($manifest.sourceFiles).Count) { throw 'Firmware file list changed; rebuild before packaging.' }
foreach ($file in $currentSources) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\','/')
    if (-not $files.Contains($relative)) { throw "New source requires rebuild: $relative" }
}
foreach ($name in @('firmware.hex','firmware.elf','firmware.bin','firmware.map')) {
    $hash = $manifest.artifacts.$name
    if (-not $hash) { throw "Build manifest missing artifact: $name" }
    $files[$buildRelative + '/' + $name] = $hash
}
foreach ($relative in @('README.md','ME4169E.code-workspace',($buildRelative + '/manifest.json'))) {
    $files[$relative] = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$hallRelative = 'Firmware/build/release-hall'
$hallManifestPath = Join-Path $root ($hallRelative + '/manifest.json')
if ($Profile -eq 'Release' -and (Test-Path -LiteralPath $hallManifestPath)) {
    $hall = Get-Content -LiteralPath $hallManifestPath -Raw | ConvertFrom-Json
    if ($hall.profile -ne 'Release' -or $hall.motorOutputsLocked -or $hall.syntheticSelfTest -or $hall.syntheticApplication -or $hall.clockHz -ne $manifest.clockHz -or $hall.mcuTarget -ne $manifest.mcuTarget -or $hall.effectiveConfiguration.ENCODER_MODE -ne 1) {
        throw 'Invalid Hall alternative application.'
    }
    if (@($hall.sourceFiles).Count -ne @($manifest.sourceFiles).Count) { throw 'Rebuild the Hall alternative after source changes.' }
    foreach ($source in $hall.sourceFiles) {
        if (-not $files.Contains($source.path) -or $files[$source.path] -ne $source.sha256) { throw "Hall source differs from main build: $($source.path)" }
    }
    foreach ($name in @('firmware.hex','firmware.elf','firmware.bin','firmware.map')) {
        if (-not $hall.artifacts.$name) { throw "Missing Hall artifact: $name" }
        $files[$hallRelative + '/' + $name] = $hall.artifacts.$name
    }
    $files[$hallRelative + '/manifest.json'] = (Get-FileHash -LiteralPath $hallManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
}
if (-not $WithoutInput) {
    foreach ($name in @('ME4169E_Assignment.pdf','Schematic_Screenshot.png','Motor_KDE3852.md')) {
        $relative = 'Input/' + $name
        $files[$relative] = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
foreach ($relative in $files.Keys) {
    $actual = (Get-FileHash -LiteralPath (Join-Path $root $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $files[$relative]) { throw "File changed since build: $relative. Rebuild before packaging." }
}
$stagingRoot = [IO.Path]::GetFullPath((Join-Path $root ('.development/package-staging/' + [Guid]::NewGuid().ToString('N'))))
if (-not $stagingRoot.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Staging must stay inside the project.' }
$contentRoot = Join-Path $stagingRoot 'ME4169E'
foreach ($relative in $files.Keys) {
    $target = Join-Path $contentRoot $relative
    [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
$temporaryArchive = Join-Path $stagingRoot 'ME4169E.zip'
Compress-Archive -LiteralPath $contentRoot -DestinationPath $temporaryArchive
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($temporaryArchive)
try {
    $entries = @{}
    foreach ($entry in $zip.Entries) {
        if ($entry.Name) { $entries[$entry.FullName.Replace('\','/')] = $entry }
    }
    if ($entries.Count -ne $files.Count) { throw 'Archive file count differs from the submission list.' }
    foreach ($relative in $files.Keys) {
        $archiveRelative = 'ME4169E/' + $relative
        if (-not $entries.ContainsKey($archiveRelative)) { throw "Archive missing file: $archiveRelative" }
        $stream = $entries[$archiveRelative].Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $actual = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ($actual -ne $files[$relative]) { throw "Archive hash mismatch: $relative" }
    }
} finally { $zip.Dispose() }
$bytes = (Get-Item -LiteralPath $temporaryArchive).Length
if ($bytes -gt 3000000) { throw "Package exceeds 3,000,000 bytes: $bytes" }
$archive = Join-Path $root 'ME4169E.zip'
Copy-Item -LiteralPath $temporaryArchive -Destination $archive -Force
Write-Host "PACKAGE PASS: $archive ($bytes bytes; $($files.Count) verified files)."
Write-Host 'Project folder: ME4169E/. Contents: Firmware/, README.md, VS Code workspace, and Input/ unless -WithoutInput.'
Write-Host "Profile: $Profile; clock: $($manifest.clockHz) Hz. Physical tests are not certified by this package."
