param(
    [string]$BuildType = "Release",
    [string]$Generator = "Ninja"
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found at $vswhere"
    exit 1
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsPath) {
    Write-Error "Unable to locate Visual Studio installation"
    exit 1
}

$vsDevCmd = Join-Path $vsPath "Common7\Tools\VsDevCmd.bat"
if (-not (Test-Path $vsDevCmd)) {
    Write-Error "VsDevCmd.bat not found at $vsDevCmd"
    exit 1
}

$vsDevCmdArgs = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 && set"
cmd /c $vsDevCmdArgs |
    ForEach-Object {
        if ($_ -match '^(.*?)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }

$cmakeArgs = @('-B', 'build', '-G', $Generator, "-DCMAKE_BUILD_TYPE=$BuildType")
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build --config $BuildType
exit $LASTEXITCODE
