param(
    [int]$DiskNumber = 1
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$device = '\\.\PHYSICALDRIVE' + $DiskNumber
$drive = Get-CimInstance Win32_DiskDrive | Where-Object { $_.Index -eq $DiskNumber }
if ($null -eq $drive -or $drive.Model -ne 'BR23 UBOOT1.00 USB Device' -or
    $drive.PNPDeviceID -notlike 'USBSTOR\DISK&VEN_BR23&PROD_UBOOT1.00*') {
    throw "Disk $DiskNumber is not the expected H9 BR23 UBOOT1.00 updater"
}

$flasher = Join-Path $repo 'firmware-research\h9-flasher\h9_flasher.py'
$stock = Join-Path $repo 'private\downloads\H9_Update-Type-C_41dc2974-af11-4ae0-8302-3fe4d775de0c\H9_Update-Type-C\TD-H9-V1.0.38.fw'
$candidate = Join-Path $repo 'firmware-research\replacement-source\build\boot-probe-main\h9_main_boot_probe_OVERLAY_EXPERIMENT.fw'
$backup = Join-Path $repo 'private\downloads\h9-before-boot-probe-20261106.bin'
$log = Join-Path $repo 'firmware-research\replacement-source\build\boot-probe-main\flash-console-20261106.log'

if ((Get-FileHash -LiteralPath $stock -Algorithm SHA256).Hash -ne
    'C4D0BEB5281F3A06D7A56083E1744B925F78FF9917C8364E006418FF0BBD0CD6') {
    throw 'Stock package hash differs from pinned v1.0.38'
}
if ((Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ne
    'D669B900F1814B218199739BD0F801D6F22977DE8E441F538A952379E4767F8D') {
    throw 'Probe package differs from the reviewed two-sector overlay'
}
if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ne
    '74243EAED19FE0C3E5FBB3EF3C46442F71EE8F7C216F214D322894D26CE4E648') {
    throw 'Fresh backup differs from pinned stock main image'
}
if (-not (Test-Path -LiteralPath ("$backup.json"))) {
    throw 'Fresh backup recovery manifest is missing'
}
if (Test-Path -LiteralPath $log) { throw "Flash log already exists: $log" }

"Device: $device ($($drive.Model))" | Tee-Object -FilePath $log
& py -3 $flasher probe --device $device 2>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw 'Read-only BR23 identity probe failed' }
& py -3 $flasher flash $candidate --stock-base $stock --device $device --backup $backup --yes 2>&1 |
    Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw 'Boot probe flash failed; inspect log and keep radio in update mode' }
'Probe write and full readback verified. Power-cycle the radio normally to observe the LED.' |
    Tee-Object -FilePath $log -Append
