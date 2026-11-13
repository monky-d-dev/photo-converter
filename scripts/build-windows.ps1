param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$PreviousPlatform = $env:QT_QPA_PLATFORM
$PreviousTheme = $env:QT_QPA_PLATFORMTHEME
$PreviousBackend = $env:QT_QUICK_BACKEND
$PreviousLogging = $env:QT_FORCE_STDERR_LOGGING
$PreviousCaptures = $env:PHOTO_CONVERTER_CAPTURE_DIR

function Invoke-Native {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program terminó con código $LASTEXITCODE" }
}

$Qmake = Get-Command qmake -ErrorAction SilentlyContinue
if (-not $Qmake) { $Qmake = Get-Command qmake6 -ErrorAction SilentlyContinue }
if (-not $Qmake) { throw 'Abre la consola del kit Qt 6 de 64 bits o agrega su carpeta bin al PATH.' }
$Spec = (& $Qmake.Source -query QMAKE_XSPEC).Trim()
if ($Spec -match 'msvc') {
    $Make = Get-Command nmake -ErrorAction SilentlyContinue
    if (-not $Make) { throw 'Abre Developer PowerShell for VS 2022 (x64) y agrega el bin de Qt MSVC al PATH.' }
} else {
    $Make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if (-not $Make) { throw 'Agrega el bin del compilador MinGW que corresponde a tu kit Qt al PATH.' }
}
$DeployTool = Get-Command windeployqt -ErrorAction Stop
$Build = Join-Path $Root 'build/windows'
$Deploy = Join-Path $Root 'dist/PhotoConverter'
New-Item -ItemType Directory -Force $Build | Out-Null

Push-Location $Build
try {
    Invoke-Native $Qmake.Source @((Join-Path $Root 'photo-converter.pro'), 'CONFIG+=release', 'CONFIG-=debug_and_release', 'CONFIG-=debug')
    Invoke-Native $Make.Source @()
} finally { Pop-Location }

if (-not $SkipTests) {
    $Tests = Join-Path $Build 'tests'
    $Report = Join-Path $Tests 'test-results.txt'
    $XmlReport = Join-Path $Tests 'test-results.xml'
    $Captures = Join-Path $Tests 'captures'
    New-Item -ItemType Directory -Force $Tests | Out-Null
    New-Item -ItemType Directory -Force $Captures | Out-Null
    Remove-Item $Report, $XmlReport -ErrorAction SilentlyContinue
    Push-Location $Tests
    try {
        Invoke-Native $Qmake.Source @((Join-Path $Root 'tests/photo-converter_tests.pro'), 'CONFIG+=release', 'CONFIG-=debug_and_release', 'CONFIG-=debug')
        Invoke-Native $Make.Source @()
        $env:QT_QPA_PLATFORM = 'offscreen'
        $env:QT_QPA_PLATFORMTHEME = 'generic'
        $env:QT_QUICK_BACKEND = 'software'
        $env:QT_FORCE_STDERR_LOGGING = '1'
        $env:PHOTO_CONVERTER_CAPTURE_DIR = $Captures
        Invoke-Native (Join-Path $Tests 'photo-converter_tests.exe') @('-o', "$Report,txt", '-o', "$XmlReport,junitxml")
    } finally {
        if (Test-Path $Report) { Get-Content $Report }
        $env:QT_QPA_PLATFORM = $PreviousPlatform
        $env:QT_QPA_PLATFORMTHEME = $PreviousTheme
        $env:QT_QUICK_BACKEND = $PreviousBackend
        $env:QT_FORCE_STDERR_LOGGING = $PreviousLogging
        $env:PHOTO_CONVERTER_CAPTURE_DIR = $PreviousCaptures
        Pop-Location
    }
}

# Build a fresh deployment folder, including every QML import used by the app.
if (Test-Path $Deploy) { Remove-Item -Recurse -Force $Deploy }
New-Item -ItemType Directory -Force $Deploy | Out-Null
Copy-Item (Join-Path $Build 'photo-converter.exe') $Deploy
Invoke-Native $DeployTool.Source @('--release', '--compiler-runtime', '--qmldir', (Join-Path $Root 'src'), (Join-Path $Deploy 'photo-converter.exe'))
$Plugins = (& $Qmake.Source -query QT_INSTALL_PLUGINS).Trim()
$Offscreen = Join-Path $Plugins 'platforms/qoffscreen.dll'
if (-not (Test-Path $Offscreen)) { throw 'El kit Qt no incluye qoffscreen.dll, necesario para el exportador sin pantalla.' }
New-Item -ItemType Directory -Force (Join-Path $Deploy 'platforms') | Out-Null
Copy-Item $Offscreen (Join-Path $Deploy 'platforms/qoffscreen.dll') -Force
Copy-Item (Join-Path $Root 'docs/USO-WINDOWS.md') (Join-Path $Deploy 'LEEME.md')
Copy-Item (Join-Path $Root 'LICENSE') $Deploy
Copy-Item (Join-Path $Root 'THIRD_PARTY_NOTICES.md') $Deploy
$Prefix = (& $Qmake.Source -query QT_INSTALL_PREFIX).Trim()
foreach ($Folder in @('LICENSES', 'licenses')) {
    $LicensePath = Join-Path $Prefix $Folder
    if (Test-Path $LicensePath) { Copy-Item -Recurse $LicensePath (Join-Path $Deploy 'Qt-LICENSES'); break }
}

# Smoke-test the actual deployed executable, outside the compiler environment's Qt PATH.
$Smoke = Join-Path $Build 'smoke-output'
New-Item -ItemType Directory -Force $Smoke | Out-Null
$OldPath = $env:PATH
try {
    $env:PATH = "$env:SystemRoot\system32;$env:SystemRoot"
    Invoke-Native (Join-Path $Deploy 'photo-converter.exe') @('export', (Join-Path $Root 'docs/demo-portrait.png'), '--output', $Smoke, '--format', 'jpg', '--json')
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QPA_PLATFORMTHEME = 'generic'
    $env:QT_QUICK_BACKEND = 'software'
    Invoke-Native (Join-Path $Deploy 'photo-converter.exe') @('open', (Join-Path $Root 'docs/demo-portrait.png'), '--capture', (Join-Path $Smoke 'interfaz.png'))
} finally {
    $env:PATH = $OldPath
    $env:QT_QPA_PLATFORM = $PreviousPlatform
    $env:QT_QPA_PLATFORMTHEME = $PreviousTheme
    $env:QT_QUICK_BACKEND = $PreviousBackend
}
$Archive = Join-Path $Root 'dist/PhotoConverter-Windows-x64.zip'
Compress-Archive -Path (Join-Path $Deploy '*') -DestinationPath $Archive -Force
Write-Host "Paquete listo: $Archive"
