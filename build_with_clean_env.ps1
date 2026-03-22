param(
    [string]$Solution = "DirectXFoundation.sln",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$Target = "Build",
    [int]$MaxCpuCount = 1,
    [string]$Verbosity = "minimal"
)

$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild))
{
    Write-Error "MSBuild not found: $msbuild"
    exit 1
}

$sourceEnv = [System.Environment]::GetEnvironmentVariables("Process")
$cleanEnv = @{}
foreach ($key in $sourceEnv.Keys)
{
    $name = [string]$key
    if ($name -ieq "PATH")
    {
        continue
    }
    $cleanEnv[$name] = [string]$sourceEnv[$key]
}

$pathValue = ""
if ($sourceEnv.ContainsKey("PATH"))
{
    $pathValue = [string]$sourceEnv["PATH"]
}
elseif ($sourceEnv.ContainsKey("Path"))
{
    $pathValue = [string]$sourceEnv["Path"]
}
else
{
    $pathValue = $env:Path
}
$cleanEnv["PATH"] = $pathValue

$args = @(
    $Solution
    "/t:$Target"
    "/p:Configuration=$Configuration"
    "/p:Platform=$Platform"
    "/m:$MaxCpuCount"
    "/v:$Verbosity"
)

Write-Host "Running MSBuild with normalized PATH key..."
Write-Host "$msbuild $($args -join ' ')"

$process = Start-Process -FilePath $msbuild -ArgumentList $args -NoNewWindow -Wait -PassThru -Environment $cleanEnv
Write-Host "MSBuild exit code: $($process.ExitCode)"
exit $process.ExitCode
