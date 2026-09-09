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

    # The official PlatformIO installer (and the VS Code PlatformIO IDE
    # extension) installs into a dedicated virtualenv rather than the
    # system/py-launcher Python, so `pio` is frequently absent from PATH
    # even though PlatformIO itself is fully installed. Check that known
    # location before falling back to a bare `-m platformio`, which fails
    # with "No module named platformio" against an unrelated interpreter.
    $PenvPlatformIo = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\platformio.exe"

    if (Test-Path $PenvPlatformIo) {
        return @{
            Kind = "pio"
            Command = $PenvPlatformIo
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

function Resolve-Python {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        return "py"
    }

    if (Get-Command python -ErrorAction SilentlyContinue) {
        return "python"
    }

    throw "Python launcher not found; partition validation cannot run."
}

$Python = Resolve-Python

function Invoke-PythonLogged {
    param(
        [string[]]$Arguments,
        [string]$LogFile
    )

    Write-Host ""
    Write-Host ">>> Python $($Arguments -join ' ')"

    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    try {
        & $Python @Arguments 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
}

function Invoke-PlatformIoLogged {
    param(
        [string[]]$Arguments,
        [string]$LogFile
    )

    Write-Host ""
    Write-Host ">>> PlatformIO $($Arguments -join ' ')"

    # Windows PowerShell 5 treats stderr from a native command as an error
    # record.  Keep the script's normal fail-fast behavior, but let this
    # wrapper collect the process exit code so the other validation gate runs.
    $PreviousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    try {
        if ($PlatformIo.Kind -eq "pio") {
            & $PlatformIo.Command @Arguments 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
        }
        else {
            & $PlatformIo.Command -m platformio @Arguments 2>&1 | Tee-Object -FilePath $LogFile | Out-Host
        }

        return [int]$LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousErrorActionPreference
    }
}

Push-Location $RepoRoot

try {
    $PartitionLog = Join-Path $OutputDir "partition-check.log"
    $PartitionExit = Invoke-PythonLogged -Arguments @("tools/check_partition.py") -LogFile $PartitionLog

    $WebUiLog = Join-Path $OutputDir "web-ui-check.log"
    $WebUiExit = Invoke-PythonLogged -Arguments @("tools/check_web_ui.py") -LogFile $WebUiLog

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
        "partition_exit=$PartitionExit"
        "web_ui_exit=$WebUiExit"
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

    if ($PartitionExit -ne 0 -or $WebUiExit -ne 0 -or $NativeExit -ne 0 -or $FirmwareExit -ne 0) {
        exit 1
    }

    exit 0
}
finally {
    Pop-Location
}
