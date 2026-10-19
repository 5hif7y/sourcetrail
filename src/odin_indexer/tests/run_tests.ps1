param(
	[string]$Indexer = "",
	[string]$Python = "",
	[string]$WorkDir = ""
)

$ErrorActionPreference = "Stop"

$repo = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
if (-not $Indexer) {
	$Indexer = Join-Path $repo "build\odin_indexer\SourcetrailOdinIndexer.exe"
}
if (-not (Test-Path $Indexer)) {
	throw "indexer not found: $Indexer (run src/odin_indexer/build.ps1 first)"
}
$Indexer = (Resolve-Path $Indexer).Path

$writerDll = Join-Path $repo "build\odin_writer\bin\sourcetrail_odin_writer.dll"
$indexerDll = Join-Path (Split-Path $Indexer -Parent) "sourcetrail_odin_writer.dll"
if (-not (Test-Path $indexerDll) -and (Test-Path $writerDll)) {
	Copy-Item $writerDll $indexerDll -Force
}
if (-not (Test-Path $indexerDll)) {
	throw "sourcetrail_odin_writer.dll not found next to the indexer"
}

if ($Python) {
	$pythonExe = $Python
	$pythonArgs = @()
}
elseif (Get-Command python -ErrorAction SilentlyContinue) {
	$pythonExe = "python"
	$pythonArgs = @()
}
else {
	$pythonExe = "py"
	$pythonArgs = @("-3")
}

if (-not $WorkDir) {
	$WorkDir = Join-Path $env:TEMP ("sourcetrail-odin-tests-" + [guid]::NewGuid().ToString("N"))
}
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

$checkScript = Join-Path $PSScriptRoot "check_db.py"
$fixtures = Join-Path $PSScriptRoot "fixtures"
$sample = Join-Path $repo "bin\app\user\projects\odin-sample\src"

function Invoke-Index {
	param(
		[string]$Database,
		[string[]]$Files,
		[string[]]$Extra = @(),
		[switch]$Keep
	)

	if ((Test-Path $Database) -and -not $Keep) {
		Remove-Item $Database -Force
	}
	$arguments = @("index")
	foreach ($file in $Files) {
		$arguments += "--source-file-path=$file"
	}
	$arguments += "--database-file-path=$Database"
	$arguments += $Extra

	& $Indexer @arguments
	if ($LASTEXITCODE -ne 0) {
		throw "indexer failed with exit code $LASTEXITCODE for $Database"
	}
}

function Invoke-Check {
	param([string[]]$Arguments)

	& $pythonExe @pythonArgs $checkScript @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "check failed: $($Arguments -join ' ')"
	}
}

$script:failures = 0
function Run-Test {
	param([string]$Name, [scriptblock]$Body)

	try {
		& $Body
		Write-Host "PASS $Name"
	}
	catch {
		$script:failures++
		Write-Host "FAIL $Name"
		Write-Host "     $_"
	}
}

$sampleFiles = @(
	(Join-Path $sample "main.odin"),
	(Join-Path $sample "model.odin"),
	(Join-Path (Join-Path $sample "mathx") "mathx.odin")
)

Run-Test "sample batch equals per-file indexing" {
	$batch = Join-Path $WorkDir "sample-batch.srctrldb"
	$single = Join-Path $WorkDir "sample-single.srctrldb"
	Invoke-Index $batch $sampleFiles
	if (Test-Path $single) {
		Remove-Item $single -Force
	}
	foreach ($file in $sampleFiles) {
		Invoke-Index -Database $single -Files @($file) -Keep
	}
	Invoke-Check @("compare", $batch, $single)
	Invoke-Check @(
		"check", $batch,
		"--require", "main.User,main.Color.Red,main.greet,mathx.add,mathx.mul",
		"--unsolved", "0")
}

Run-Test "member chains resolve to declared symbols" {
	$db = Join-Path $WorkDir "globals.srctrldb"
	Invoke-Index $db @((Join-Path $fixtures "globals.odin"))
	Invoke-Check @(
		"check", $db,
		"--require", "globals.g,globals.f,globals.count,globals.use_globals",
		"--forbid", "globals.g.x,globals.f.p,globals.f.p.y",
		"--require-edge", "globals.use_globals|4|globals.g",
		"--require-edge", "globals.use_globals|4|globals.f",
		"--require-edge", "globals.use_globals|4|globals.count")
}

Run-Test "unresolved references are opt-in" {
	$without = Join-Path $WorkDir "unsolved-off.srctrldb"
	$with = Join-Path $WorkDir "unsolved-on.srctrldb"
	Invoke-Index $without @((Join-Path $fixtures "unsolved.odin"))
	Invoke-Index $with @((Join-Path $fixtures "unsolved.odin")) @("--unsolved")
	Invoke-Check @("check", $without, "--unsolved", "0")
	Invoke-Check @("check", $with, "--unsolved", "1", "--min-edges", "1")
}

Run-Test "attributes, directives and foreign blocks declare symbols" {
	$db = Join-Path $WorkDir "syntax.srctrldb"
	Invoke-Index $db @((Join-Path $fixtures "syntax.odin"))
	Invoke-Check @(
		"check", $db,
		"--require", "syntax.CF,syntax.cacos,syntax.PRIVATE_CONST,syntax.forced,syntax.Generic,syntax.Generic.value,syntax.platform",
		"--forbid", "syntax.private",
		"--unsolved", "0")
}

Run-Test "proc type parameter names do not leak" {
	$db = Join-Path $WorkDir "proctypes.srctrldb"
	Invoke-Index $db @((Join-Path $fixtures "proctypes.odin")) @("--unsolved")
	Invoke-Check @(
		"check", $db,
		"--require", "proctypes.Callback,proctypes.Holder,proctypes.Holder.cmp,proctypes.Holder.swap,proctypes.make_callback",
		"--unsolved", "0")
}

Run-Test "raw strings, enum aliases and offset_of field names" {
	$db = Join-Path $WorkDir "strings-enum.srctrldb"
	Invoke-Index $db @((Join-Path $fixtures "strings_enum.odin")) @("--unsolved")
	Invoke-Check @(
		"check", $db,
		"--require", "strings_enum.Color,strings_enum.Color.Red,strings_enum.Color.Green,strings_enum.Color.Blue,strings_enum.State,strings_enum.State.first,strings_enum.State.second",
		"--forbid", "strings_enum.fake,strings_enum.phantom,strings_enum.missing_call,strings_enum.second",
		"--require-edge", "strings_enum.Color.Blue|4|strings_enum.Color.Green",
		"--unsolved", "0")
}

Run-Test "batch indexing is deterministic" {
	$first = Join-Path $WorkDir "deterministic-a.srctrldb"
	$second = Join-Path $WorkDir "deterministic-b.srctrldb"
	Invoke-Index $first $sampleFiles
	Invoke-Index $second $sampleFiles
	Invoke-Check @("compare", $first, $second)
}

if ($script:failures -ne 0) {
	Write-Host "$script:failures test(s) failed; artifacts in $WorkDir"
	exit 1
}

Write-Host "All Odin indexer tests passed. Artifacts in $WorkDir"
