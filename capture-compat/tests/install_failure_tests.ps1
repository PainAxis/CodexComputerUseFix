$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $project ('validation/failure-fixture-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$helper=Join-Path $fixture 'codex-computer-use.exe'
[IO.File]::WriteAllText($helper,'INERT TEST FIXTURE')
function Set-Content {
    param([string]$LiteralPath,[string]$Encoding,[Parameter(ValueFromPipeline)]$Value)
    process { throw 'Injected install-record write failure' }
}
$failed=$false
try { & (Join-Path $project 'install.ps1') -HelperPath $helper }
catch { if($_.Exception.Message -ne 'Injected install-record write failure'){throw};$failed=$true }
finally { Remove-Item Function:/Set-Content }
if(-not $failed){throw 'Failure injection not reached'}
$remaining=@(Get-ChildItem -LiteralPath $fixture -Force | Where-Object Name -ne 'codex-computer-use.exe')
if($remaining.Count){throw ('Install failure left files: '+($remaining.Name -join ','))}
& (Join-Path $project 'install.ps1') -HelperPath $helper
& (Join-Path $project 'install.ps1') -HelperPath $helper -Action Uninstall
'PASS: injected record-write failure leaves no DLL, record or temporary files; retry and uninstall succeed.'
