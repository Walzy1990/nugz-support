$ErrorActionPreference = 'Stop'
$installRoot = Join-Path $env:ProgramFiles 'Nugz Support Tool'
$sourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "{0}"' -f $MyInvocation.MyCommand.Path
    Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments
    exit
}

New-Item -ItemType Directory -Force -Path $installRoot | Out-Null
Copy-Item (Join-Path $sourceRoot 'Support.exe') (Join-Path $installRoot 'Support.exe') -Force
Copy-Item (Join-Path $sourceRoot 'KeyGen.exe') (Join-Path $installRoot 'KeyGen.exe') -Force

$startMenu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Nugz Support Tool'
New-Item -ItemType Directory -Force -Path $startMenu | Out-Null
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $startMenu 'Nugz Support Tool.lnk'))
$shortcut.TargetPath = Join-Path $installRoot 'Support.exe'
$shortcut.WorkingDirectory = $installRoot
$shortcut.Description = 'Nugz Support Tool'
$shortcut.Save()

$keyShortcut = $shell.CreateShortcut((Join-Path $startMenu 'Nugz Key Generator.lnk'))
$keyShortcut.TargetPath = Join-Path $installRoot 'KeyGen.exe'
$keyShortcut.WorkingDirectory = $installRoot
$keyShortcut.Description = 'Generate a Nugz Support Tool access key'
$keyShortcut.Save()

$uninstallPath = Join-Path $installRoot 'Uninstall-Nugz.ps1'
@'
$installRoot = Join-Path $env:ProgramFiles 'Nugz Support Tool'
$startMenu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Nugz Support Tool'
Remove-Item $startMenu -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $installRoot -Recurse -Force -ErrorAction SilentlyContinue
'@ | Set-Content -Path $uninstallPath -Encoding UTF8

Write-Host 'Nugz Support Tool installed.' -ForegroundColor Green
Write-Host "Location: $installRoot"
Read-Host 'Press Enter to close'
