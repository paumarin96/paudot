param(
    [ValidateSet('Generate', 'Build', 'Clean', 'Open')]
    [string]$Action = 'Open',
    [ValidateRange(1, 256)]
    [int]$Jobs = 8,
    [ValidateSet('Dev', 'Release')]
    [string]$Configuration = 'Dev'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$python = Join-Path $repo '.build-venv\Scripts\python.exe'
$scons = Join-Path $repo '.build-venv\Scripts\scons.exe'
$solutionName = if ($Configuration -eq 'Release') { 'godot-release' } else { 'godot' }
Push-Location $repo
try {
    if (!(Test-Path $python)) {
        & python -m venv .build-venv
        if ($LASTEXITCODE -ne 0) { throw 'Could not create the Python build environment.' }
    }
    if (!(Test-Path $scons)) {
        & $python -m pip install 'scons==4.11.1'
        if ($LASTEXITCODE -ne 0) { throw 'Could not install SCons.' }
    }
    # Visual Studio inherits this PATH, including when it invokes SCons for Rebuild.
    $env:PATH = "$(Split-Path $scons);$env:PATH"
    $buildArgs = @('platform=windows', 'target=editor', 'arch=x86_64',
        'd3d12=no', 'angle=no', 'accesskit=no', "num_jobs=$Jobs")
    if ($Configuration -eq 'Release') {
        $buildArgs += @('dev_build=no', 'production=yes', 'debug_symbols=no',
            'optimize=speed', 'lto=full', 'extra_suffix=release')
    } else {
        $buildArgs += 'dev_build=yes'
    }
    if ($Action -in @('Generate', 'Open')) {
        & $scons @buildArgs vsproj=yes "vsproj_name=$solutionName"
    } elseif ($Action -eq 'Clean') {
        & $scons @buildArgs --clean
    } else {
        & $scons @buildArgs
    }
    if ($LASTEXITCODE -ne 0) { throw "SCons failed with exit code $LASTEXITCODE." }
    if ($Action -in @('Generate', 'Open')) {
        # Godot imports this override after its generated configuration properties.
        # Keep SCons discoverable even when the solution is opened directly.
        $props = @'
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <NMakeBuildCommandLine>set "PATH=$(MSBuildProjectDirectory)\.build-venv\Scripts;%PATH%" &amp; $(NMakeBuildCommandLine)</NMakeBuildCommandLine>
    <NMakeReBuildCommandLine>set "PATH=$(MSBuildProjectDirectory)\.build-venv\Scripts;%PATH%" &amp; $(NMakeReBuildCommandLine)</NMakeReBuildCommandLine>
    <NMakeCleanCommandLine>set "PATH=$(MSBuildProjectDirectory)\.build-venv\Scripts;%PATH%" &amp; $(NMakeCleanCommandLine)</NMakeCleanCommandLine>
  </PropertyGroup>
</Project>
'@
        [IO.File]::WriteAllText((Join-Path $repo "$solutionName.vs.user.props"), $props)
    }
    if ($Action -eq 'Open') {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (!$vs) { throw 'Install Visual Studio with the Desktop development with C++ workload.' }
        Start-Process -FilePath (Join-Path $vs 'Common7\IDE\devenv.exe') -ArgumentList ('"' + (Join-Path $repo "$solutionName.sln") + '"')
    }
} finally {
    Pop-Location
}
