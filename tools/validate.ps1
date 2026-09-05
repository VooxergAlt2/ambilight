param(
    [switch]$SkipNative,
    [switch]$SkipFirmware
)

$ErrorActionPreference = "Stop"

# PowerShell 7 can optionally convert native non-zero exit codes into
# terminating errors. Validation needs both gates to run so exit codes are
# collected explicitly instead.
if (Test-Path variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$OutputDir = Join-Path $RepoRoot ".artifacts\validation\$Timestamp"

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

function Resolve-PlatformIo {
    if (Get-Command pio -ErrorAction SilentlyContinue) {
        return @{
            Kind = "pio"
            Command = "pio"
        }
    }

    if (Get-Command py -ErrorAction SilentlyContinue) {
        return @{
            Kind = "python-module"
            Command = "py"
        }
    }

    if (Get-Command python -ErrorAction SilentlyContinue) {
        return @{
            Kind = "python-module"
            Command = "python"
        }
    }

    throw "PlatformIO launcher not found. Install PlatformIO or make pio/python available in PATH."
}

$PlatformIo = Resolve-PlatformIo

function Invoke-PlatformIoLogged {
    param(
        [string[]]$Arguments,
        [string]$LogFile
    )

    Write-Host ""
    Write-Host ">>> PlatformIO $($Arguments -join ' ')"

    if ($PlatformIo.Kind -eq "pio") {
        & $PlatformIo.Command @Arguments 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
    }
    else {
        & $PlatformIo.Command -m platformio @Arguments 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
    }

    return [int]$LASTEXITCODE
}

Push-Location $RepoRoot

try {
    $NativeExit = 0
    $FirmwareExit = 0

    if (-not $SkipNative) {
        $NativeLog = Join-Path $OutputDir "native-test.log"
        $NativeExit = Invoke-PlatformIoLogged -Arguments @("test", "-e", "native") -LogFile $NativeLog
    }

    if (-not $SkipFirmware) {
        $FirmwareLog = Join-Path $OutputDir "firmware-build.log"
        $FirmwareExit = Invoke-PlatformIoLogged -Arguments @("run", "-e", "esp32-c6-devkitc-1") -LogFile $FirmwareLog
    }

    $Summary = @(
        "Ambilight local validation"
        "timestamp=$Timestamp"
        "native_skipped=$SkipNative"
        "native_exit=$NativeExit"
        "firmware_skipped=$SkipFirmware"
        "firmware_exit=$FirmwareExit"
        "output=$OutputDir"
    )

    $SummaryPath = Join-Path $OutputDir "summary.txt"
    $Summary | Set-Content -Path $SummaryPath -Encoding UTF8

    Write-Host ""
    Write-Host ($Summary -join [Environment]::NewLine)

    if ($NativeExit -ne 0 -or $FirmwareExit -ne 0) {
        exit 1
    }

    exit 0
}
finally {
    Pop-Location
}
