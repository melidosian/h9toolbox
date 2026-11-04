# Elevated, read-only preflight for the TD-H9 GPIO3 audio bench probe.
# This script never erases or writes radio flash. It only identifies the
# bootloader, runs SCSI INQUIRY, and saves the current 0xDA000-byte image.
param(
    [string]$BackupPath = $env:H9_GPIO3_BACKUP
)

$ErrorActionPreference = 'Stop'
$research = $PSScriptRoot
$flasher = Join-Path (Split-Path $research -Parent) 'h9-flasher\h9_flasher.py'
$probe = Join-Path $research 'output\gpio3-low-probe.fw'
$stock = $env:H9_STOCK_FW
if (-not $stock -or -not $BackupPath) { throw 'Set H9_STOCK_FW and pass -BackupPath (or set H9_GPIO3_BACKUP).' }
$log = Join-Path $research 'output\probe-backup-console.log'

if (-not (Test-Path -LiteralPath $flasher) -or -not (Test-Path -LiteralPath $probe) -or -not (Test-Path -LiteralPath $stock)) {
    throw 'Flasher, diagnostic image, or stock package is missing.'
}
if (Test-Path -LiteralPath $BackupPath) {
    throw "Refusing to overwrite backup: $BackupPath"
}
if (Test-Path -LiteralPath ($BackupPath + '.json')) {
    throw "Refusing to overwrite backup manifest: $BackupPath.json"
}
if (Test-Path -LiteralPath $log) {
    throw "Refusing to overwrite log: $log"
}

$drives = @(Get-CimInstance Win32_DiskDrive | Where-Object {
    $_.Model -eq 'BR23 UBOOT1.00 USB Device' -and $_.DeviceID -like '\\.\PHYSICALDRIVE*'
})
if ($drives.Count -ne 1) {
    throw "Expected exactly one BR23 UBOOT disk; found $($drives.Count)."
}
$device = $drives[0].DeviceID

Start-Transcript -LiteralPath $log | Out-Null
try {
    Write-Output "Radio disk: $device; $($drives[0].Model)"
    & py -3 $flasher inspect $probe --stock-base $stock
    if ($LASTEXITCODE -ne 0) { throw 'Offline custom-package validation failed.' }
    & py -3 $flasher probe --device $device
    if ($LASTEXITCODE -ne 0) { throw 'Bootloader probe failed.' }
    & py -3 $flasher backup $probe --stock-base $stock --device $device --backup $BackupPath
    if ($LASTEXITCODE -ne 0) { throw 'Pre-probe backup failed.' }
    Write-Output 'BACKUP_COMPLETE; no flash erase or write was requested.'
}
finally {
    Stop-Transcript | Out-Null
}
