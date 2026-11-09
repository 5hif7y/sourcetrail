param(
	# Extra app directories (the folder of sourcetrail.exe / sourcetrail_cli.exe)
	# to deploy into, besides the repository's own build folders.
	[string[]]$AppDir = @(),
	[string]$Configuration = "Release"
)

# Builds scip2sourcetrail and deploys it, with the writer shim it links, to
# <app>/data/scip, where the Rust and C# source groups look for it.

$ErrorActionPreference = "Stop"

# Native tools write progress to stderr; with "Stop", Windows PowerShell 5.1
# would turn that into an error. Only the exit code decides.
function Invoke-Native([string]$What, [scriptblock]$Command) {
	$previous = $ErrorActionPreference
	$ErrorActionPreference = "Continue"
	try { & $Command 2>&1 | ForEach-Object { "$_" } | Out-Host }
	finally { $ErrorActionPreference = $previous }
	if ($LASTEXITCODE -ne 0) { throw "$What failed ($LASTEXITCODE)" }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$writerBuild = Join-Path $repo "build\odin_writer"
$writerDll = Join-Path $writerBuild "bin\sourcetrail_odin_writer.dll"
if (-not (Test-Path $writerDll)) {
	Write-Host "Building the SourcetrailDB writer shim in $writerBuild"
	Invoke-Native "writer configure" { cmake -S (Join-Path $repo "src\odin_writer") -B $writerBuild }
	Invoke-Native "writer build" { cmake --build $writerBuild --config $Configuration --parallel }
}

Write-Host "Building scip2sourcetrail"
Push-Location $PSScriptRoot
try {
	$env:SODB_WRITER_DIR = $writerBuild
	Invoke-Native "cargo build" { cargo build --release --locked }
}
finally {
	Pop-Location
	Remove-Item Env:SODB_WRITER_DIR -ErrorAction SilentlyContinue
}

$exe = Join-Path $PSScriptRoot "target\release\scip2sourcetrail.exe"
$dirs = @(
	(Join-Path $repo "bin\app"),
	(Join-Path $repo "build\$Configuration\app"),
	(Join-Path $repo "build-embedded\$Configuration\app")
) + $AppDir
foreach ($app in $dirs) {
	if (-not (Test-Path $app)) { continue }
	$dir = Join-Path $app "data\scip"
	New-Item -ItemType Directory -Force -Path $dir | Out-Null
	Copy-Item $exe (Join-Path $dir "scip2sourcetrail.exe") -Force
	Copy-Item $writerDll (Join-Path $dir "sourcetrail_odin_writer.dll") -Force
	Write-Host "Deployed scip2sourcetrail to $dir"
}
Write-Host "Done."
