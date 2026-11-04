# Restore the exact pre-probe main flash image captured from this radio.
# Run only after the radio has re-entered BR23 USB Update mode.
$ErrorActionPreference = 'Stop'
$research = $PSScriptRoot
$flasher = Join-Path (Split-Path $research -Parent) 'h9-flasher\h9_flasher.py'
$probe = Join-Path $research 'output\gpio3-low-probe.fw'
$stock = $env:H9_STOCK_FW
$backup = $env:H9_GPIO3_BACKUP
if (-not $stock -or -not $backup) { throw 'Set H9_STOCK_FW and H9_GPIO3_BACKUP to the local stock package and verified backup.' }
$log = Join-Path $research ('output\probe-restore-console-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.log')

if (Test-Path -LiteralPath $log) { throw "Refusing to overwrite log: $log" }
foreach ($path in @($flasher, $probe, $stock, $backup, ($backup + '.json'))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required file missing: $path" }
}
$expected = @{
    $probe = 'C8B7B896DB1A26E279F52D08A9E884041FE7C5640AEB235F04E7F391915FCEAD'
    $stock = 'C4D0BEB5281F3A06D7A56083E1744B925F78FF9917C8364E006418FF0BBD0CD6'
    $backup = '74243EAED19FE0C3E5FBB3EF3C46442F71EE8F7C216F214D322894D26CE4E648'
}
foreach ($path in $expected.Keys) {
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected[$path]) {
        throw "Hash mismatch: $path"
    }
}

$drives = @(Get-CimInstance Win32_DiskDrive | Where-Object {
    $_.Model -eq 'BR23 UBOOT1.00 USB Device' -and $_.DeviceID -like '\\.\PHYSICALDRIVE*'
})
if ($drives.Count -ne 1) { throw "Expected exactly one BR23 UBOOT disk; found $($drives.Count)." }
$device = $drives[0].DeviceID

Start-Transcript -LiteralPath $log | Out-Null
try {
    Write-Output "Radio disk: $device; $($drives[0].Model)"
    & py -3 $flasher inspect $probe --stock-base $stock
    if ($LASTEXITCODE -ne 0) { throw 'Offline custom-package validation failed.' }
    & py -3 $flasher probe --device $device
    if ($LASTEXITCODE -ne 0) { throw 'Bootloader probe failed.' }
    & py -3 $flasher restore $probe --stock-base $stock --device $device --backup $backup --yes
    if ($LASTEXITCODE -ne 0) { throw 'Restore failed; inspect the console log before power-cycling.' }
    Write-Output 'PROBE_RESTORE_COMPLETE; full image readback verified by flasher.'
}
finally {
    Stop-Transcript | Out-Null
}
