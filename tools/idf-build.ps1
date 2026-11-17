# Budowanie / wgrywanie na Windows bez zainstalowanego "ESP-IDF Command Prompt".
#
# Użycie:  powershell -ExecutionPolicy Bypass -File tools\idf-build.ps1 [-Cmd "build"] [-Port COM12]
#          (-Cmd np. "build", "flash", "set-target esp32p4", "menuconfig"; z -Port dodaje "-p <port>")
#
# Dlaczego kopia w C:\gp: komponent esp_hosted ma zagnieżdżone katalogi testowe i przy zwykłej ścieżce
# projektu (Documents\PlatformIO\...) rozpakowanie kończy się WinError 206 (MAX_PATH). Dopóki w Windows
# nie ma włączonych długich ścieżek, budujemy z kopii źródeł w krótkiej ścieżce. Wyniki: $Work\build.
#
# Zmienne można nadpisać: $env:IDF_ROOT (katalog z frameworks\<wersja>, tools\, penvs\<wersja>).
param(
    [string]$Cmd = 'build',
    [string]$Port = '',
    [string]$Work = 'C:\gp',
    [string]$IdfRoot = $(if ($env:IDF_ROOT) { $env:IDF_ROOT } else { 'C:\esphome-idf' }),
    [string]$IdfVer = '5.5.6'
)
$ErrorActionPreference = 'Stop'
$src = Split-Path -Parent $PSScriptRoot

robocopy $src $Work /MIR /XD .git build managed_components tools /XF dependencies.lock /NFL /NDL /NJH /NJS | Out-Null

$t = Join-Path $IdfRoot 'tools'
$env:IDF_TOOLS_PATH = $IdfRoot
$env:IDF_PATH = Join-Path $IdfRoot "frameworks\$IdfVer"
$env:IDF_PYTHON_ENV_PATH = Join-Path $IdfRoot "penvs\$IdfVer"
$env:IDF_COMPONENT_CACHE_PATH = 'C:\cc'                      # krótka ścieżka cache komponentów
$env:ESP_IDF_VERSION = ($IdfVer -split '\.')[0..1] -join '.'  # bez tego Kconfig esp_wifi_remote pomija opcje
$env:ESP_ROM_ELF_DIR = (Get-ChildItem (Join-Path $t 'esp-rom-elfs') | Select-Object -First 1).FullName

$gcc = (Get-ChildItem (Join-Path $t 'riscv32-esp-elf') -Recurse -Filter riscv32-esp-elf-gcc.exe | Select-Object -First 1).DirectoryName
$cmake = (Get-ChildItem (Join-Path $t 'cmake') | Select-Object -First 1).FullName
$ninja = (Get-ChildItem (Join-Path $t 'ninja') | Select-Object -First 1).FullName
$ccache = (Get-ChildItem (Join-Path $t 'ccache') | Select-Object -First 1).FullName
$env:PATH = "$env:IDF_PYTHON_ENV_PATH\Scripts;$gcc;$ninja;$cmake\bin;$ccache;$env:PATH"

Set-Location $Work
$idfArgs = @()
if ($Port) { $idfArgs += @('-p', $Port) }
$idfArgs += $Cmd.Split(' ')
& python (Join-Path $env:IDF_PATH 'tools\idf.py') @idfArgs
exit $LASTEXITCODE
