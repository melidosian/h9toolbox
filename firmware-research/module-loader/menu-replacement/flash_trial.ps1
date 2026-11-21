param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$StockPackage,
    [Parameter(Mandatory=$true)][string]$BackupDirectory,
    [Parameter(Mandatory=$true)][string]$Python
)

$ErrorActionPreference = 'Stop'
$flasher = Join-Path $PSScriptRoot '..\..\h9-flasher\h9_flasher.py'
New-Item -ItemType Directory -Path $BackupDirectory | Out-Null
Start-Transcript -LiteralPath (Join-Path $BackupDirectory 'flash.log') | Out-Null
try {
    & $Python -I (Join-Path $PSScriptRoot 'prepare_trial.py') $BuildDirectory --stock-base $StockPackage
    if ($LASTEXITCODE -ne 0) { throw 'Package verification failed.' }
    $radio = @(Get-CimInstance Win32_DiskDrive | Where-Object { $_.Model -eq 'BR23 UBOOT1.00 USB Device' })
    if ($radio.Count -ne 1 -or $radio[0].DeviceID -notmatch '^\\\\\.\\PHYSICALDRIVE[0-9]+$') {
        throw 'Expected exactly one TD-H9 BR23 USB update device.'
    }
    $device = $radio[0].DeviceID
    & $Python -I $flasher probe --device $device
    if ($LASTEXITCODE -ne 0) { throw 'Radio bootloader inquiry failed.' }
    & $Python -I $flasher flash (Join-Path $BuildDirectory 'h9_toolbox_modules_OFFLINE_EXPERIMENT.fw') `
        --stock-base $StockPackage --device $device --backup (Join-Path $BackupDirectory 'main-before.bin') `
        --far (Join-Path $BuildDirectory 'unified-far-0xEA000.bin') --far-backup (Join-Path $BackupDirectory 'far-before.bin') --yes
    if ($LASTEXITCODE -ne 0) { throw 'Flash failed. Keep the radio in update mode and check the log.' }
    Write-Output 'UNIFIED_MENU_FLASH_VERIFIED'
}
finally {
    Stop-Transcript | Out-Null
}
