# Budowanie / wgrywanie na Windows bez otwierania "ESP-IDF PowerShell".
#
# Użycie:  powershell -ExecutionPolicy Bypass -File tools\idf-build.ps1 [-Cmd "build"] [-Port COM12]
#          (-Cmd np. "build", "flash", "monitor", "set-target esp32p4", "menuconfig"; z -Port dodaje "-p <port>")
#
# Dlaczego kopia w C:\gp: komponent esp_hosted ma zagnieżdżone katalogi testowe i przy zwykłej ścieżce
# projektu (Documents\PlatformIO\...) rozpakowanie kończy się WinError 206 (MAX_PATH). Dopóki w Windows
# nie ma włączonych długich ścieżek, budujemy z kopii źródeł w krótkiej ścieżce. Wyniki: $Work\build.
#
# Układ instalacji (Espressif Installation Manager, ESP-IDF 5.5.6):
#   IDF_PATH        C:\esphome-idf\v5.5.6\esp-idf
#   IDF_TOOLS_PATH  C:\Espressif\tools   (kompilator, cmake, ninja, ccache, python\v5.5.6\venv)
# Ścieżki można nadpisać parametrami albo zmiennymi $env:IDF_PATH / $env:IDF_TOOLS_PATH.
param(
    [string]$Cmd = 'build',
    [string]$Port = '',
    [string]$Work = 'C:\gp',
    [string]$IdfVer = '5.5.6',
    [string]$IdfPath = $(if ($env:IDF_PATH) { $env:IDF_PATH } else { "C:\esphome-idf\v$IdfVer\esp-idf" }),
    [string]$ToolsPath = $(if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else { 'C:\Espressif\tools' })
)
$ErrorActionPreference = 'Stop'
$src = Split-Path -Parent $PSScriptRoot

function Need([string]$path, [string]$what) {
    if (-not (Test-Path $path)) { throw "Nie znaleziono: $what ($path)" }
    $path
}
# katalog zawierający dany plik .exe gdziekolwiek pod $root (wersje narzędzi są w podkatalogach)
function ExeDir([string]$root, [string]$exe) {
    $f = Get-ChildItem (Need $root $exe) -Recurse -Filter $exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $f) { throw "Nie znaleziono $exe pod $root" }
    $f.DirectoryName
}

$null = Need (Join-Path $IdfPath 'tools\idf.py') 'ESP-IDF (idf.py)'
$pyEnv = Need (Join-Path $ToolsPath "python\v$IdfVer\venv") 'środowisko Python ESP-IDF'

$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $ToolsPath
$env:IDF_PYTHON_ENV_PATH = $pyEnv
$env:IDF_COMPONENT_CACHE_PATH = 'C:\cc'                       # krótka ścieżka cache komponentów
$env:IDF_CCACHE_ENABLE = '1'
$env:ESP_IDF_VERSION = ($IdfVer -split '\.')[0..1] -join '.'  # bez tego Kconfig esp_wifi_remote pomija opcje
$env:ESP_ROM_ELF_DIR = (Get-ChildItem (Need (Join-Path $ToolsPath 'esp-rom-elfs') 'esp-rom-elfs') -Directory | Select-Object -First 1).FullName + '\'

$paths = @(
    "$pyEnv\Scripts",
    (ExeDir (Join-Path $ToolsPath 'riscv32-esp-elf') 'riscv32-esp-elf-gcc.exe'),
    (ExeDir (Join-Path $ToolsPath 'cmake') 'cmake.exe'),
    (ExeDir (Join-Path $ToolsPath 'ninja') 'ninja.exe'),
    (ExeDir (Join-Path $ToolsPath 'ccache') 'ccache.exe'),
    (ExeDir (Join-Path $ToolsPath 'idf-exe') 'idf.py.exe')
)
$env:PATH = ($paths -join ';') + ";$env:PATH"

robocopy $src $Work /MIR /XD .git build managed_components tools /XF dependencies.lock /NFL /NDL /NJH /NJS | Out-Null
if ($LASTEXITCODE -ge 8) { throw "robocopy nie skopiował źródeł do $Work (kod $LASTEXITCODE)" }

Set-Location $Work
$idfArgs = @()
if ($Port) { $idfArgs += @('-p', $Port) }
$idfArgs += $Cmd.Split(' ')
& "$pyEnv\Scripts\python.exe" (Join-Path $IdfPath 'tools\idf.py') @idfArgs
exit $LASTEXITCODE
