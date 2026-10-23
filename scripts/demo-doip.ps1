<#
.SYNOPSIS
Runs the local DoIP TCP Server and Tester demonstration on Windows.

.DESCRIPTION
Requires doip_tcp_server.exe and doip_tester.exe in build\c-core. Build the
project first with: cmake -S . -B build; cmake --build build.
Only the Server process started by this script is stopped during cleanup.

.PARAMETER ServerAddress
Numeric IPv4 address for the server. Defaults to 127.0.0.1.

.PARAMETER Port
TCP port to bind and connect to. Defaults to 13400.
#>
param(
    [string]$ServerAddress = "127.0.0.1",
    [ValidateRange(1, 65535)]
    [int]$Port = 13400
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$serverExecutable = Join-Path $repositoryRoot "build\c-core\doip_tcp_server.exe"
$testerExecutable = Join-Path $repositoryRoot "build\c-core\doip_tester.exe"
$serverProcess = $null
$testerProcess = $null
$testerExitCode = 1

function Test-TcpEndpoint {
    param(
        [string]$Address,
        [int]$TargetPort,
        [int]$TimeoutMilliseconds = 300
    )

    $client = New-Object System.Net.Sockets.TcpClient
    $connectTask = $null
    try {
        $connectTask = $client.BeginConnect($Address, $TargetPort, $null, $null)
        if (-not $connectTask.AsyncWaitHandle.WaitOne($TimeoutMilliseconds)) {
            return $false
        }
        $client.EndConnect($connectTask)
        return $true
    }
    catch {
        return $false
    }
    finally {
        if ($null -ne $connectTask) {
            $connectTask.AsyncWaitHandle.Close()
        }
        $client.Close()
    }
}

try {
    $parsedAddress = $null
    if (-not [System.Net.IPAddress]::TryParse($ServerAddress, [ref]$parsedAddress) -or
        $parsedAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
        throw "ServerAddress must be a numeric IPv4 address."
    }

    if (-not (Test-Path -LiteralPath $serverExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath $testerExecutable -PathType Leaf)) {
        throw "Build artifacts are missing. Build first with: cmake -S . -B build; cmake --build build"
    }

    if (Test-TcpEndpoint -Address $ServerAddress -TargetPort $Port) {
        throw "TCP endpoint $ServerAddress`:$Port is already accepting connections. Stop the existing service or choose another port."
    }

    Write-Host "Starting DoIP TCP Server at $ServerAddress`:$Port ..."
    $serverProcess = Start-Process `
        -FilePath $serverExecutable `
        -ArgumentList @($ServerAddress, [string]$Port) `
        -PassThru `
        -NoNewWindow

    $ready = $false
    $readyDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $readyDeadline) {
        $serverProcess.Refresh()
        if ($serverProcess.HasExited) {
            throw "TCP Server exited during startup (exit code $($serverProcess.ExitCode)). The port may be unavailable."
        }
        if (Test-TcpEndpoint -Address $ServerAddress -TargetPort $Port) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 100
    }
    if (-not $ready) {
        throw "TCP Server did not become ready at $ServerAddress`:$Port within 10 seconds."
    }

    Write-Host "Running DoIP Tester ..."
    $testerStartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $testerStartInfo.FileName = $testerExecutable
    $testerStartInfo.Arguments = "`"$ServerAddress`" $Port 0x0E00 0x1000"
    $testerStartInfo.UseShellExecute = $false
    $testerStartInfo.CreateNoWindow = $true
    $testerStartInfo.RedirectStandardOutput = $true
    $testerStartInfo.RedirectStandardError = $true
    $testerProcess = [System.Diagnostics.Process]::Start($testerStartInfo)
    $testerStdout = $testerProcess.StandardOutput.ReadToEndAsync()
    $testerStderr = $testerProcess.StandardError.ReadToEndAsync()

    if (-not $testerProcess.WaitForExit(20000)) {
        try {
            $testerProcess.Kill()
            if (-not $testerProcess.WaitForExit(5000)) {
                Write-Warning "Tester process $($testerProcess.Id) did not exit after termination was requested."
            }
        }
        catch {
            Write-Warning "Could not terminate the Tester process: $_"
        }
        $testerExitCode = 1
    }
    else {
        $testerProcess.WaitForExit()
        $standardOutputText = $testerStdout.Result
        $standardErrorText = $testerStderr.Result
        if (-not [string]::IsNullOrWhiteSpace($standardOutputText)) {
            Write-Host $standardOutputText.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($standardErrorText)) {
            Write-Host $standardErrorText.TrimEnd()
        }
        $testerExitCode = $testerProcess.ExitCode
    }
    if ($testerExitCode -ne 0) {
        throw "Tester failed or timed out (exit code $testerExitCode)."
    }
}
catch {
    Write-Error $_
    $testerExitCode = 1
}
finally {
    if (($null -ne $serverProcess) -and -not $serverProcess.HasExited) {
        Write-Host "Stopping the DoIP TCP Server started by this script ..."
        try {
            $serverProcess.Kill()
            if (-not $serverProcess.WaitForExit(5000)) {
                Write-Warning "Server process $($serverProcess.Id) did not exit after termination was requested."
            }
        }
        catch {
            Write-Warning "Could not stop the Server process $($serverProcess.Id): $_"
        }
    }

    if ($null -ne $testerProcess) {
        $testerProcess.Dispose()
    }
    if ($null -ne $serverProcess) {
        $serverProcess.Dispose()
    }
}

exit $testerExitCode
