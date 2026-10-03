param(
    [string[]]$Renderers = @('forward_plus', 'mobile', 'gl_compatibility'),
    [ValidateSet('Dev', 'Release')]
    [string]$Configuration = 'Dev'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testDir = Join-Path $repo 'bin\shader-smoke'
$binary = if ($Configuration -eq 'Release') { 'godot.windows.editor.x86_64.release.exe' } else { 'godot.windows.editor.dev.x86_64.exe' }
$engine = Join-Path $repo "bin\$binary"
if (!(Test-Path $engine)) { throw "Build the $Configuration editor first." }
New-Item -ItemType Directory -Path $testDir -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'smoke_test.gd'), (Join-Path $PSScriptRoot 'validate_shader.gd'), (Join-Path $PSScriptRoot 'directional_shadow.gdshader') $testDir
@'
config_version=5
[application]
config/name="Directional Shadow Smoke Test"
[display]
window/size/viewport_width=256
window/size/viewport_height=256
[rendering]
renderer/rendering_method="forward_plus"
'@ | Set-Content (Join-Path $testDir 'project.godot')

function Invoke-ShaderTest([string]$label, [string]$renderer, [string]$script, [string[]]$extraArgs = @()) {
    $driver = if ($renderer -eq 'gl_compatibility') { 'opengl3' } else { 'vulkan' }
    $stdout = Join-Path $testDir "$label.out.log"
    $stderr = Join-Path $testDir "$label.err.log"
    $engineArgs = @('--path', ('"' + $testDir + '"'), '--script', $script,
        '--rendering-method', $renderer, '--rendering-driver', $driver,
        '--audio-driver', 'Dummy', '--position', '-10000,-10000',
        '--log-file', ('"' + (Join-Path $testDir "$label.engine.log") + '"')) + $extraArgs
    $process = Start-Process -FilePath $engine -ArgumentList $engineArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $process.Handle
    if (!$process.WaitForExit(60000)) {
        $process.Kill()
        throw "$label timed out; inspect $stderr."
    }
    $process.WaitForExit()
    $process.Refresh()
    $output = Get-Content $stdout -Raw
    $errors = Get-Content $stderr -Raw
    if ($extraArgs.Count) {
        if ($errors -notmatch 'Shader compilation failed' -or $output -notmatch 'SHADER_VALIDATION_COMPLETE') {
            throw "$label did not reject the invalid shader; inspect $stderr."
        }
    } elseif ($process.ExitCode -ne 0 -or $output -notmatch 'SHADOW_SMOKE_RESULT failures=0' -or $errors -match 'Shader compilation failed|SCRIPT ERROR|Parse Error') {
        throw "$label failed (exit $($process.ExitCode)); inspect $stdout and $stderr."
    }
    Write-Output "$label passed"
}

foreach ($renderer in $Renderers) {
    Invoke-ShaderTest $renderer $renderer 'smoke_test.gd'
}
foreach ($case in @('vertex', 'vertex_helper', 'canvas', 'wrong_arguments')) {
    Invoke-ShaderTest "reject-$case" 'forward_plus' 'validate_shader.gd' @('--', $case)
}
