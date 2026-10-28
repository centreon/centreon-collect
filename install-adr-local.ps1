<#
.SYNOPSIS
  Install the CMA ADR kit into a local centreon-collect clone without committing it (Windows).

.DESCRIPTION
  Copies the kit files into the clone, then lists them in the clone's .git/info/exclude
  (local file, never pushed). Works with Windows PowerShell 5.1 and PowerShell 7.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File .\install-adr-local.ps1 -Repo C:\dev\centreon-collect
  powershell -ExecutionPolicy Bypass -File .\install-adr-local.ps1 -Repo C:\dev\centreon-collect -Status
  powershell -ExecutionPolicy Bypass -File .\install-adr-local.ps1 -Repo C:\dev\centreon-collect -Force
  powershell -ExecutionPolicy Bypass -File .\install-adr-local.ps1 -Repo C:\dev\centreon-collect -Uninstall
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$Repo,
  [switch]$Force,
  [switch]$Uninstall,
  [switch]$Status
)
$ErrorActionPreference = 'Stop'

$KitDir = $PSScriptRoot
$MarkBegin = '# >>> cma-adr local kit >>>'
$MarkEnd = '# <<< cma-adr local kit <<<'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

$Files = @(
  'CLAUDE.md',
  '.claude/settings.json',
  '.claude/hooks/cma-adr-context.sh',
  '.claude/hooks/cma-adr-stop.sh',
  '.claude/agents/cma-adr-reviewer.md',
  'agent/CLAUDE.md',
  '.claude/rules/cma-adr.md',
  'docs/adr/README.md',
  'docs/adr/template.md',
  'docs/adr/cma/README.md',
  'docs/adr/cma/ARCHITECTURE.md'
)
$Files += Get-ChildItem -Path (Join-Path $KitDir 'docs/adr/cma') -Filter '????-*.md' |
  Sort-Object Name | ForEach-Object { 'docs/adr/cma/' + $_.Name }

if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git not found in PATH' }
$Repo = (Resolve-Path $Repo).Path
Set-Location $Repo
# Windows PowerShell 5.1 turns native stderr into terminating errors under 'Stop':
# git probes run through this helper with a local 'Continue' preference.
function Invoke-GitQuiet {
  $ErrorActionPreference = 'Continue'
  & git @args 2>&1 | Out-Null
  return $LASTEXITCODE
}
if ((Invoke-GitQuiet rev-parse --is-inside-work-tree) -ne 0) {
  throw "not a git clone: $Repo. Pass the absolute path of your centreon-collect clone, e.g. -Repo C:\dev\centreon-collect ('.' is the current directory of the terminal)."
}
$TopLevel = (Resolve-Path ((& git rev-parse --show-toplevel).Trim())).Path
if ($TopLevel -ne $Repo) { Set-Location $TopLevel; $Repo = $TopLevel }
$SameDir = ((Resolve-Path $KitDir).Path.TrimEnd('\','/') -eq $Repo.TrimEnd('\','/'))
if (-not (Test-Path 'agent/CMakeLists.txt')) { throw "not centreon-collect (agent/ missing): $Repo" }

$Exclude = (& git rev-parse --git-path info/exclude).Trim()
if (-not [System.IO.Path]::IsPathRooted($Exclude)) { $Exclude = Join-Path $Repo $Exclude }
New-Item -ItemType Directory -Force -Path (Split-Path $Exclude) | Out-Null
if (-not (Test-Path $Exclude)) { [System.IO.File]::WriteAllText($Exclude, '', $Utf8NoBom) }

function Test-Tracked([string]$f) { return ((Invoke-GitQuiet ls-files --error-unmatch -- $f) -eq 0) }
function Test-Ignored([string]$f) { return ((Invoke-GitQuiet check-ignore -q -- $f) -eq 0) }
function Remove-Block {
  $lines = [System.IO.File]::ReadAllLines($Exclude)
  $out = New-Object System.Collections.Generic.List[string]
  $skip = $false
  foreach ($l in $lines) {
    if ($l -eq $MarkBegin) { $skip = $true; continue }
    if ($l -eq $MarkEnd) { $skip = $false; continue }
    if (-not $skip) { $out.Add($l) }
  }
  [System.IO.File]::WriteAllLines($Exclude, $out.ToArray(), $Utf8NoBom)
}
function Test-SameContent([string]$a, [string]$b) {
  return ((Get-FileHash $a -Algorithm SHA256).Hash -eq (Get-FileHash $b -Algorithm SHA256).Hash)
}

if ($Status) {
  foreach ($f in $Files) {
    if (Test-Path $f) {
      if (Test-Tracked $f) { $s = 'TRACKED (in repo)' }
      elseif (Test-Ignored $f) { $s = 'installed, ignored' }
      else { $s = 'installed, NOT ignored' }
    } else { $s = 'absent' }
    '{0,-58} {1}' -f $f, $s
  }
  exit 0
}

if ($Uninstall -and $SameDir) {
  throw "refused: the kit was extracted inside the clone itself ($Repo); uninstall would delete the only copy. Extract cma-adr.zip elsewhere (e.g. C:\cma-adr-kit) and run the script from there."
}
if ($Uninstall) {
  foreach ($f in $Files) {
    if ((Test-Path $f) -and -not (Test-Tracked $f)) { Remove-Item $f; "removed $f" }
  }
  foreach ($d in @('docs/adr/cma', 'docs/adr', 'docs', '.claude/rules', '.claude/hooks', '.claude/agents', '.claude')) {
    if ((Test-Path $d) -and -not (Get-ChildItem $d -Force)) { Remove-Item $d -Force }
  }
  Remove-Block
  'uninstalled'
  exit 0
}

# Never overwrite a file tracked by git, nor a different local file unless -Force.
foreach ($f in $Files) {
  if (Test-Tracked $f) { throw "refused: $f is tracked by git in this clone (the repo now has its own version)" }
  if ((Test-Path $f) -and -not $Force -and -not (Test-SameContent (Join-Path $KitDir $f) $f)) {
    throw "refused: $f exists and differs; rerun with -Force to overwrite"
  }
}

if (-not $SameDir) {
  foreach ($f in $Files) {
    $dir = Split-Path $f
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Copy-Item -Path (Join-Path $KitDir $f) -Destination $f -Force
  }
}

Remove-Block
$block = @($MarkBegin) + ($Files | ForEach-Object { '/' + $_ }) + @($MarkEnd)
[System.IO.File]::AppendAllText($Exclude, (($block -join "`n") + "`n"), $Utf8NoBom)

"installed $($Files.Count) files into $Repo"
$dirty = & git status --porcelain -- $Files
if ($dirty) { Write-Warning "some kit files still appear in git status:`n$dirty"; exit 1 }
'git status: clean for kit files'
"note: 'git clean -fdx' or '-fdX' deletes ignored files, including this kit. Rerun this script after it."
