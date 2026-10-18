[CmdletBinding()]
param([string]$IsoPath, [string]$SdkPath, [switch]$NoDownload, [switch]$PrepareOnly, [int]$Jobs = 8)
$ErrorActionPreference = 'Stop'
try {
    if (-not $IsoPath) {
        Add-Type -AssemblyName System.Windows.Forms
        $picker = New-Object System.Windows.Forms.OpenFileDialog
        $picker.Title = 'Select your Rockstar Games presents Table Tennis retail ISO'
        $picker.Filter = 'Xbox 360 disc image (*.iso)|*.iso'
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) {
            Write-Host 'No ISO selected. Setup cancelled.'
            exit 1
        }
        $IsoPath = $picker.FileName
        $picker.Dispose()
    }
    $python = Get-Command py.exe -ErrorAction SilentlyContinue
    $pythonArgs = @()
    if ($python) { $pythonArgs += '-3' } else { $python = Get-Command python.exe -ErrorAction SilentlyContinue }
    if (-not $python) { throw 'Install 64-bit Python 3.10 or newer, then run setup again. See README.md.' }
    $pythonArgs += @((Join-Path $PSScriptRoot 'project\tools\setup_build.py'), '--iso', $IsoPath, '--jobs', $Jobs)
    if ($SdkPath) { $pythonArgs += @('--sdk', $SdkPath) }
    if ($NoDownload) { $pythonArgs += '--no-download' }
    if ($PrepareOnly) { $pythonArgs += '--prepare-only' }
    & $python.Source @pythonArgs
    exit $LASTEXITCODE
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
