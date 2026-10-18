[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SourceZip, [Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$SourceZip = [IO.Path]::GetFullPath($SourceZip)
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($OutputPath)) -Force | Out-Null
$python = Get-Command py.exe -ErrorAction SilentlyContinue
if ($python) { & $python.Source -3 (Join-Path $PSScriptRoot 'package_source.py') --verify-zip $SourceZip }
else {
    $python = Get-Command python.exe -ErrorAction Stop
    & $python.Source (Join-Path $PSScriptRoot 'package_source.py') --verify-zip $SourceZip
}
if ($LASTEXITCODE -ne 0) { throw 'Installer payload verification failed. Only reviewed public source may be embedded.' }
$csc = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$wpf = Join-Path ([IO.Path]::GetDirectoryName($csc)) 'WPF'
& $csc /nologo /target:winexe /platform:x64 /optimize /codepage:65001 "/out:$OutputPath" "/resource:$SourceZip,source.zip" "/lib:$wpf" '/r:PresentationCore.dll,PresentationFramework.dll,WindowsBase.dll,System.dll,System.Xaml.dll,System.Windows.Forms.dll,System.IO.Compression.dll,System.IO.Compression.FileSystem.dll' (Join-Path $root 'project\installer\Installer.cs')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
Write-Output ('Built project-authored installer: ' + $OutputPath)
