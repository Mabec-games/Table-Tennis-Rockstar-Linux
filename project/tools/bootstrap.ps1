[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Root,
    [Parameter(Mandatory=$true)][string]$IsoPath,
    [switch]$AllowToolInstall,
    [string]$SdkPath,
    [switch]$NoDownload,
    [switch]$PrepareOnly,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Stage([string]$Name) { Write-Output ("STAGE:" + $Name) }
function Get-VerifiedDownload([string]$Url, [string]$Path, [string]$Sha256) {
    if ((Test-Path -LiteralPath $Path) -and (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Sha256) { return }
    if ($NoDownload) { throw 'A required component is not cached. Internet access is needed for the first installation.' }
    $temporary = $Path + '.download'
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $temporary
    if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $Sha256) { throw 'A downloaded component failed its checksum check. It was not used.' }
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

try {
    $Root = [IO.Path]::GetFullPath($Root)
    $IsoPath = [IO.Path]::GetFullPath($IsoPath)
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required.' }
    if (-not (Test-Path -LiteralPath $IsoPath -PathType Leaf)) { throw 'The selected ISO was not found.' }
    $toolsDir = Join-Path $Root '.tools'
    New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null
    Stage 'Preparing the installer'
    $runtimeArchive = Join-Path $toolsDir 'python-3.14.8-embed-amd64.zip'
    Get-VerifiedDownload 'https://www.python.org/ftp/python/3.14.8/python-3.14.8-embed-amd64.zip' $runtimeArchive 'a93abe456ab01bd96d7a085b3cdb6566b3063f4241360d114142fbdb07f0a310'
    $runtimeDir = Join-Path $toolsDir 'python314'
    if (-not (Test-Path -LiteralPath (Join-Path $runtimeDir 'python.exe'))) {
        Expand-Archive -LiteralPath $runtimeArchive -DestinationPath $runtimeDir -Force
    }
    # The isolated official Python runtime needs only our standard-library scripts.
    [IO.File]::WriteAllText((Join-Path $runtimeDir 'python314._pth'), "python314.zip`n.`n../../project/tools`n", [Text.Encoding]::ASCII)
    $python = Join-Path $runtimeDir 'python.exe'
    $builder = Join-Path $Root 'project\tools\setup_build.py'
    Stage 'Checking your game disc'
    $ErrorActionPreference = 'Continue'
    & $python $builder --iso $IsoPath --validate-iso
    $validateExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($validateExit -ne 0) { throw 'This disc image is not supported. Select an unmodified USA/Europe retail Table Tennis ISO.' }
    Stage 'Checking required components'
    # PowerShell 5 treats redirected native stderr as an error record; capture a
    # tool check's exit code rather than aborting before automatic repair.
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $checkOutput = & $python $builder --check-tools 2>&1
    $checkExit = $LASTEXITCODE
    $ErrorActionPreference = $previousPreference
    if ($checkExit -ne 0 -and -not $PrepareOnly) {
        if (-not $AllowToolInstall) { throw 'Required Microsoft components are missing. Allow their installation to continue.' }
        if ($NoDownload) { throw 'Microsoft components are missing and downloads are disabled.' }
        Stage 'Installing Microsoft components - approve the Windows permission prompt if shown'
        $bootstrap = Join-Path $toolsDir 'vs_buildtools.exe'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://aka.ms/vs/17/release/vs_buildtools.exe' -OutFile $bootstrap
        $signature = Get-AuthenticodeSignature -LiteralPath $bootstrap
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
            throw 'The Microsoft installer signature could not be verified. It was not started.'
        }
        $installArgs = @('--passive', '--wait', '--norestart', '--add', 'Microsoft.VisualStudio.Workload.VCTools',
                         '--includeRecommended', '--add', 'Microsoft.VisualStudio.Component.VC.Llvm.Clang',
                         '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project')
        $process = Start-Process -FilePath $bootstrap -ArgumentList $installArgs -Verb RunAs -WindowStyle Hidden -PassThru -Wait
        if ($process.ExitCode -eq 3010 -or $process.ExitCode -eq 1641) {
            Write-Output 'RESTART:Restart Windows, then open this installer again to finish.'
            exit 3010
        }
        if ($process.ExitCode -ne 0) { throw ("Microsoft component installation did not complete (code " + $process.ExitCode + ').') }
        & $python $builder --check-tools
        if ($LASTEXITCODE -ne 0) { throw 'Required components are still unavailable. Open the installation log for details.' }
    }
    Stage 'Creating your local game - this can take a while'
    $buildArgs = @($builder, '--iso', $IsoPath, '--jobs', $Jobs)
    if ($SdkPath) { $buildArgs += @('--sdk', $SdkPath) }
    if ($NoDownload) { $buildArgs += '--no-download' }
    if ($PrepareOnly) { $buildArgs += '--prepare-only' }
    $ErrorActionPreference = 'Continue'
    & $python @buildArgs
    $buildExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($buildExit -ne 0) { throw 'The local game build did not finish. See the log above for the component that failed.' }
    Stage 'Ready'
    exit 0
} catch {
    Write-Output ('INSTALL_ERROR:' + $_.Exception.Message)
    Write-Output 'No Windows security settings were changed. The ISO was not modified.'
    exit 1
}
