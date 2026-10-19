param(
	[string]$Odin = "odin",
	[string]$SourcetrailDB = "",
	[string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $SourcetrailDB) {
	$SourcetrailDB = (Resolve-Path (Join-Path $repo "..\sourcetrail-db")).Path
}
$odinCommand = Get-Command $Odin -ErrorAction SilentlyContinue
$odinExe = if ($odinCommand) { $odinCommand.Source } else { $Odin }

$writerBuild = Join-Path $repo "build\odin_writer"
Write-Host "Building SourcetrailDB writer shim in $writerBuild"
cmake -S (Join-Path $repo "src\odin_writer") -B $writerBuild -G "Visual Studio 18 2026" -A x64 "-DSOURCETRAILDB_DIR=$SourcetrailDB"
if ($LASTEXITCODE -ne 0) { throw "writer configure failed" }
cmake --build $writerBuild --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "writer build failed" }

$writerDll = Join-Path $writerBuild "bin\sourcetrail_odin_writer.dll"
$writerLib = Join-Path $writerBuild "lib\sourcetrail_odin_writer.lib"
if (-not (Test-Path $writerDll) -or -not (Test-Path $writerLib)) {
	throw "writer artifacts not found under $writerBuild"
}
Copy-Item $writerLib (Join-Path $PSScriptRoot "sourcetrail_odin_writer.lib") -Force

$indexerBuild = Join-Path $repo "build\odin_indexer"
New-Item -ItemType Directory -Force -Path $indexerBuild | Out-Null
$indexerExe = Join-Path $indexerBuild "SourcetrailOdinIndexer.exe"
Write-Host "Building Odin indexer with $odinExe"
& $odinExe build $PSScriptRoot "-out:$indexerExe"
if ($LASTEXITCODE -ne 0) { throw "odin build failed" }

$deployDirs = @(
	"bin\app\data\odin",
	"build\$Configuration\app\data\odin",
	"build-embedded\$Configuration\app\data\odin"
)
foreach ($relative in $deployDirs) {
	$dir = Join-Path $repo $relative
	New-Item -ItemType Directory -Force -Path $dir | Out-Null
	Copy-Item $indexerExe (Join-Path $dir "SourcetrailOdinIndexer.exe") -Force
	Copy-Item $writerDll (Join-Path $dir "sourcetrail_odin_writer.dll") -Force
	Write-Host "Deployed native indexer to $dir"
}
Write-Host "Done."
