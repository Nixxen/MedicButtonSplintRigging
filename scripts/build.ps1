# Version: 1.1.0
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent

$projects = @(Get-ChildItem -Path $repoRoot -Filter *.vcxproj -File)

switch ($projects.Count) {
    0 { throw "No .vcxproj found in '$repoRoot'." }
    1 { $project = $projects[0] }
    default { throw "Expected one .vcxproj in '$repoRoot', found $($projects.Count)." }
}

if ([string]::IsNullOrEmpty($env:MSBUILD_PATH)) {
    throw "MSBUILD_PATH environment variable is not set. Please set it to the Visual Studio MSBuild directory (e.g., 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin')."
}

$msbuild = Join-Path $env:MSBUILD_PATH "MSBuild.exe"

if (!(Test-Path $msbuild)) {
    throw "MSBuild.exe not found at '$msbuild'."
}

Write-Host "Building $($project.Name)..."

& $msbuild `
    $project.FullName `
    /p:Configuration=Release `
    /p:Platform=x64

exit $LASTEXITCODE