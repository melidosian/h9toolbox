param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('backup', 'flash', 'restore', 'trial', 'resume')]
    [string]$Action,
    [int]$DiskNumber = 1
)

# Run in Administrator PowerShell after checking the radio is in BR23 USB
# update mode. Each action opens only the verified updater disk.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$device = '\\.\PHYSICALDRIVE' + $DiskNumber
$matches = @(Get-CimInstance Win32_DiskDrive | Where-Object {
    $_.Model -eq 'BR23 UBOOT1.00 USB Device' -and
    $_.PNPDeviceID -like 'USBSTOR\DISK&VEN_BR23&PROD_UBOOT1.00*'
})
if ($matches.Count -ne 1 -or $matches[0].Index -ne $DiskNumber) {
    throw "Expected exactly one H9 BR23 updater at $device"
}

$flasher = Join-Path $repo 'firmware-research\h9-flasher\h9_flasher.py'
$fallback = Join-Path $repo 'firmware-research\h9-firmware-research\restore_display_task_stage20_over_hid.py'
$stock = Join-Path $repo 'private\downloads\H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c\H9_Update-Type-C\TD-H9-V1.0.38.fw'
$candidate = Join-Path $repo 'firmware-research\replacement-source\build\display-task-stage20\h9_hello_display_OFFLINE_EXPERIMENT.fw'
$backup = Join-Path $repo 'private\downloads\h9-before-display-task-stage20-20261106.bin'
$log = Join-Path $repo "firmware-research\replacement-source\build\display-task-stage20\$Action-console-20261106.log"

$expected = @{
    $stock = 'C4D0BEB5281F3A06D7A56083E1744B925F78FF9917C8364E006418FF0BBD0CD6'
    $candidate = 'AFB132993929B31C53963E7634F3E953E0D0B96F80E653EFBA8EA22B04463F18'
}
if ($Action -notin @('backup', 'trial')) {
    $expected[$backup] = '74243EAED19FE0C3E5FBB3EF3C46442F71EE8F7C216F214D322894D26CE4E648'
    if (-not (Test-Path -LiteralPath ("$backup.json"))) {
        throw 'Fresh backup recovery manifest is missing'
    }
} elseif (Test-Path -LiteralPath $backup) {
    throw "Fresh backup path already exists: $backup"
}
foreach ($path in $expected.Keys) {
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected[$path]) {
        throw "Pinned SHA-256 mismatch: $path"
    }
}
if (Test-Path -LiteralPath $log) { throw "Trial log already exists: $log" }

"Device: $device ($($matches[0].Model)); action: $Action" | Tee-Object -FilePath $log
& py -3 $flasher inspect $candidate --stock-base $stock 2>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw 'Offline package inspection failed' }
& py -3 $flasher probe --device $device 2>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw 'BR23 updater identity probe failed' }

if ($Action -in @('backup', 'trial')) {
    & py -3 $flasher backup $candidate --stock-base $stock --device $device --backup $backup 2>&1 |
        Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw 'Read-only backup failed' }
    if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne
        '74243EAED19FE0C3E5FBB3EF3C46442F71EE8F7C216F214D322894D26CE4E648') {
        throw 'Fresh radio backup differs from the pinned stock main image'
    }
    & py -3 $fallback plan --backup $backup --package $candidate 2>&1 |
        Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw 'HID recovery plan rejected fresh backup' }
    'Fresh stock backup and HID recovery plan verified.' | Tee-Object -FilePath $log -Append
}
if ($Action -eq 'resume') {
    $ErrorActionPreference = 'Continue'
    & py -3 $flasher resume $candidate --stock-base $stock --device $device --backup $backup --yes 2>&1 |
        Tee-Object -FilePath $log -Append
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { throw 'Display image resume/readback failed; keep radio in update mode' }
    'Display image full readback verified; power-cycle normally to inspect screen.' |
        Tee-Object -FilePath $log -Append
} elseif ($Action -in @('flash', 'trial')) {
    & py -3 $flasher flash $candidate --stock-base $stock --device $device --backup $backup --yes 2>&1 |
        Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw 'Display image flash failed; keep radio in update mode' }
    'Display image full readback verified; power-cycle normally to inspect screen.' |
        Tee-Object -FilePath $log -Append
} elseif ($Action -eq 'restore') {
    & py -3 $flasher restore $candidate --stock-base $stock --device $device --backup $backup --yes 2>&1 |
        Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) { throw 'Stock restore failed; keep radio in update mode' }
    'Stock full readback verified; power-cycle normally.' | Tee-Object -FilePath $log -Append
}
