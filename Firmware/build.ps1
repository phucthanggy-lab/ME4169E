[CmdletBinding()]
param(
    [ValidateSet('Development','Release','Test')][string]$Profile = 'Release',
    [switch]$SelfTest,
    [switch]$ApplicationFixture,
    [switch]$ActiveLow,
    [ValidateSet(0,1000000,2000000,4000000,8000000,16000000)][int]$ClockHz = 0,
    [ValidateSet('Quadrature','HallRising')][string]$EncoderMode = 'Quadrature',
    [uint32]$EncoderCountsPerRev = 0,
    [switch]$EncoderReverse,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$avrBin = Join-Path $root '.tools/avr/avr/bin'
if ($env:AVR_BIN) { $avrBin = $env:AVR_BIN }
$compiler = Join-Path $avrBin 'avr-gcc.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'AVR-GCC missing. Run Firmware/tools/install-toolchain.ps1 or set AVR_BIN.' }
if ($Profile -eq 'Test' -and -not ($SelfTest -or $ApplicationFixture)) { throw 'Synthetic profile requires SelfTest or ApplicationFixture, and never exports a HEX.' }
if ($ApplicationFixture -and ($SelfTest -or $Profile -ne 'Test')) { throw 'ApplicationFixture requires Test profile and the normal application main().' }
if ($ActiveLow -and $Profile -ne 'Test') { throw 'ActiveLow is only a synthetic test fixture selector.' }
if ($ClockHz -ne 0 -and $Profile -eq 'Release') { throw 'Release uses CFG_HW_F_CPU_HZ; ClockHz is only a development/test override.' }
if ($EncoderReverse -and $EncoderMode -eq 'HallRising') { throw 'Single-channel Hall cannot measure direction.' }
$profileValue = @{ Development = '1'; Release = '2'; Test = '3' }[$Profile]
if (-not $OutputDirectory) {
    $folderName = $Profile.ToLowerInvariant()
    if ($EncoderMode -eq 'HallRising') { $folderName += '-hall' }
    if ($SelfTest) { $folderName = 'selftest-' + $folderName }
    if ($SelfTest) { $OutputDirectory = Join-Path $root ('.development/Testing/evidence/builds/' + $folderName) }
    else { $OutputDirectory = Join-Path $PSScriptRoot ('build/' + $folderName) }
}
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory = Join-Path $root $OutputDirectory }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $OutputDirectory.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Build output must stay inside this workspace.' }
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$objectHasher = [Security.Cryptography.SHA256]::Create()
try { $objectKey = [BitConverter]::ToString($objectHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($OutputDirectory))).Replace('-','').Substring(0,12) }
finally { $objectHasher.Dispose() }
$objectDirectory = Join-Path $root ('.build/objects/' + $objectKey)
[IO.Directory]::CreateDirectory($objectDirectory) | Out-Null
$encoding = New-Object System.Text.UTF8Encoding($false)
$log = New-Object System.Collections.Generic.List[string]
$started = [DateTime]::UtcNow.ToString('o')
function Run-Tool([string]$Program, [string[]]$Arguments) {
    $log.Add($Program + ' ' + ($Arguments -join ' '))
    $ErrorActionPreference = 'Continue'
    $lines = & $Program @Arguments 2>&1
    $exit = $LASTEXITCODE
    foreach ($line in $lines) { $log.Add([string]$line); Write-Host $line }
    if ($exit -ne 0) { throw "Tool exited $exit`: $Program" }
}
$success = $false
Push-Location $root
try {
    $generatedFiles = @('firmware.elf','firmware.hex','firmware.bin','firmware.map','firmware.lst','manifest.json','hex-verification.json')
    $generatedFiles += @(Get-ChildItem -LiteralPath $OutputDirectory -Filter 'emulation-*.json' -File | ForEach-Object { $_.Name })
    foreach ($generated in $generatedFiles) {
        $path = Join-Path $OutputDirectory $generated
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
    $flags = @('-mmcu=atmega8','-std=gnu11','-Os','-g','-Wall','-Wextra','-Werror','-Wshadow','-Wundef','-Wstrict-prototypes','-Wmissing-prototypes','-Wformat=2','-ffunction-sections','-fdata-sections','-I','Firmware/include',('-DFW_PROFILE=' + $profileValue))
    if ($ClockHz -ne 0) { $flags += '-DCFG_DEV_F_CPU_HZ=' + $ClockHz + 'UL' }
    $flags += '-DCFG_ENCODER_MODE=' + @{ Quadrature = '4'; HallRising = '1' }[$EncoderMode] + 'U'
    if ($EncoderReverse) { $flags += '-DCFG_ENCODER_REVERSED=1' }
    if ($PSBoundParameters.ContainsKey('EncoderCountsPerRev')) { $flags += '-DCFG_ENCODER_COUNTS_PER_MEASURED_REV=' + $EncoderCountsPerRev + 'UL' }
    if ($SelfTest -or $ApplicationFixture) { $flags += @('-DFW_TESTING=1') }
    if ($ActiveLow) { $flags += @('-DCFG_DIRECTION_ACTIVE_LEVEL=0','-DCFG_PWM_ACTIVE_LEVEL=0') }
    if ($Profile -eq 'Test') { $flags += @('-include','.development/Testing/synthetic_config.h') }
    $probe = & $compiler @flags -E -P 'Firmware/tools/config-probe.c' 2>&1
    if ($LASTEXITCODE -ne 0) { throw ('Configuration preflight failed: ' + ($probe -join "`n")) }
    $effective = @{}
    foreach ($line in $probe) {
        if ([string]$line -match '^FW_CONFIG_(\w+) (\d+)(?:UL|U|L)?$') { $effective[$matches[1]] = [uint32]$matches[2] }
        $log.Add([string]$line)
    }
    if (-not $effective.ContainsKey('CLOCK_HZ') -or -not $effective.ContainsKey('OUTPUTS')) { throw 'Cannot resolve effective configuration metadata.' }
    foreach ($required in @('ENCODER_CPR','ENCODER_USE_NOMINAL_CPR','ENCODER_MODE','ENCODER_PPR_REFERENCE','GEAR_RATIO_NUMERATOR','GEAR_RATIO_DENOMINATOR')) {
        if (-not $effective.ContainsKey($required)) { throw "Configuration metadata requires a numeric literal for $required." }
    }
    if ($effective.ENCODER_USE_NOMINAL_CPR -eq 1) {
        $effective.ENCODER_CPR_NUMERATOR = [uint32]([uint64]$effective.ENCODER_PPR_REFERENCE * $effective.ENCODER_MODE * $effective.GEAR_RATIO_NUMERATOR)
        $effective.ENCODER_CPR_DENOMINATOR = $effective.GEAR_RATIO_DENOMINATOR
    } else {
        $effective.ENCODER_CPR_NUMERATOR = $effective.ENCODER_CPR
        $effective.ENCODER_CPR_DENOMINATOR = 1
    }
    $effective.ENCODER_CPR = [double]$effective.ENCODER_CPR_NUMERATOR / $effective.ENCODER_CPR_DENOMINATOR
    $sources = @(Get-ChildItem -LiteralPath 'Firmware/src' -Filter '*.c' | Where-Object { -not $SelfTest -or $_.Name -ne 'main.c' } | Sort-Object Name | ForEach-Object { 'Firmware/src/' + $_.Name })
    if ($SelfTest) { $sources += '.development/Testing/avr_selftest.c' }
    $objects = @()
    foreach ($source in $sources) {
        $object = Join-Path $objectDirectory ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
        Run-Tool $compiler ($flags + @('-c',$source,'-o',$object))
        $objects += $object
    }
    $elf = Join-Path $OutputDirectory 'firmware.elf'
    $hex = Join-Path $OutputDirectory 'firmware.hex'
    $binary = Join-Path $OutputDirectory 'firmware.bin'
    $map = Join-Path $OutputDirectory 'firmware.map'
    Run-Tool $compiler (@('-mmcu=atmega8','-Wl,--gc-sections',('-Wl,-Map=' + $map)) + $objects + @('-o',$elf))
    if (-not $SelfTest -and $Profile -ne 'Test') { Run-Tool (Join-Path $avrBin 'avr-objcopy.exe') @('-O','ihex','-R','.eeprom',$elf,$hex) }
    Run-Tool (Join-Path $avrBin 'avr-objcopy.exe') @('-O','binary','-R','.eeprom',$elf,$binary)
    $size = & (Join-Path $avrBin 'avr-size.exe') -A $elf
    if ($LASTEXITCODE -ne 0) { throw 'avr-size failed.' }
    $size | ForEach-Object { $log.Add([string]$_) }
    $sizes = @{}
    foreach ($line in $size) { if ($line -match '^\.(text|data|bss|noinit)\s+(\d+)') { $sizes[$matches[1]] = [int]$matches[2] } }
    $flash = [int]$sizes.text + [int]$sizes.data
    $sram = [int]$sizes.data + [int]$sizes.bss + [int]$sizes.noinit
    if ($flash -gt 8192 -or $sram -gt 1024) { throw 'ATmega8 Flash/SRAM capacity exceeded.' }
    $disassembly = & (Join-Path $avrBin 'avr-objdump.exe') -d $elf
    if ($LASTEXITCODE -ne 0) { throw 'Disassembly failed.' }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'firmware.lst'), ($disassembly -join "`n"), $encoding)
    $hashInputs = @(Get-ChildItem 'Firmware/src','Firmware/include','Firmware/tools' -File -Recurse; Get-Item 'Firmware/build.ps1','Firmware/Makefile')
    if ($SelfTest) { $hashInputs += Get-Item '.development/Testing/avr_selftest.c' }
    if ($SelfTest -or $Profile -eq 'Test') { $hashInputs += Get-Item '.development/Testing/synthetic_config.h' }
    $sourceHashes = @($hashInputs | Sort-Object FullName | ForEach-Object { @{ path = $_.FullName.Substring($root.Length + 1).Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() } })
    $compilerVersion = (& $compiler --version | Select-Object -First 1)
    $manifest = @{ version = 'ME4169E-0.2.3'; profile = $Profile; syntheticSelfTest = [bool]$SelfTest; syntheticApplication = [bool]$ApplicationFixture; mcuTarget = 'atmega8'; mcuConfirmationDeclared = ($effective.MCU_CONFIRMED -eq 1 -and $Profile -ne 'Test'); clockConfirmed = ($effective.CLOCK_CONFIRMED -eq 1 -and $Profile -eq 'Release'); startedAtUtc = $started; finishedAtUtc = [DateTime]::UtcNow.ToString('o'); compiler = $compilerVersion; compileFlags = $flags; clockHz = $effective.CLOCK_HZ; motorOutputsLocked = ($effective.OUTPUTS -eq 0); effectiveConfiguration = $effective; flashBytes = $flash; staticSramBytes = $sram; flashLimit = 8192; sramLimit = 1024; sourceFiles = $sourceHashes; artifacts = @{} }
    $artifactNames = @('firmware.elf','firmware.bin','firmware.map')
    if (-not $SelfTest -and $Profile -ne 'Test') { $artifactNames += 'firmware.hex' }
    foreach ($artifact in $artifactNames) { $manifest.artifacts[$artifact] = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $artifact) -Algorithm SHA256).Hash.ToLowerInvariant() }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'manifest.json'), ($manifest | ConvertTo-Json -Depth 8), $encoding)
    $success = $true
    Write-Host "BUILD PASS: $Profile; Flash $flash/8192 bytes; static SRAM $sram/1024 bytes. Artifacts: $OutputDirectory"
} finally {
    if (-not $success) {
        $log.Add('BUILD FAILED. No current HEX is certified by this run.')
        $failedHex = Join-Path $OutputDirectory 'firmware.hex'
        if (Test-Path -LiteralPath $failedHex) { Remove-Item -LiteralPath $failedHex }
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'build.log'), ($log -join "`n"), $encoding)
    Pop-Location
}
