# Construye HyperEZ completo: hyper.exe, pruebas, extensión de VS Code e instalador.
#
#   ./build.ps1                 todo (hyper.exe optimizado con PGO)
#   ./build.ps1 -NoPGO          sin optimización guiada por perfil (compila el doble de rápido)
#   ./build.ps1 -SkipVsix       sin la extensión de VS Code (no necesita Node.js)
#   ./build.ps1 -SkipTests      sin ejecutar las pruebas
#   ./build.ps1 -VSCodeTests    además, pruebas de integración dentro de VS Code (descarga VS Code Insiders)
#
# Resultado en dist/: HyperEZ-Setup-<versión>.exe, hyperez-<versión>.vsix y hyper.exe
param(
    [switch]$NoPGO,
    [switch]$SkipVsix,
    [switch]$SkipTests,
    [switch]$VSCodeTests,
    [string]$BuildDir = "build"
)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

# MSYS2 UCRT64 (GCC + CMake + Ninja) por delante de otros compiladores del PATH
$msys = if ($env:HYPEREZ_MSYS) { $env:HYPEREZ_MSYS } else { "C:\msys64\ucrt64\bin" }
if (Test-Path $msys) { $env:PATH = "$msys;$env:PATH" }

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }
function Run($exe, [string[]]$arguments) {
    # Algunas herramientas escriben avisos en stderr: solo cuenta el código de salida.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $exe @arguments 2>&1 | ForEach-Object { "$_" } } finally { $ErrorActionPreference = $prev }
    if ($LASTEXITCODE -ne 0) { throw "Falló: $exe $($arguments -join ' ') (código $LASTEXITCODE)" }
}
function Quiet($exe, [string[]]$arguments) {
    # Ejecuta sin mostrar la salida (entrenamiento de PGO); los fallos no detienen la compilación.
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $exe @arguments *> $null } finally { $ErrorActionPreference = $prev }
}

$version = (Select-String -Path "src/core/version.h" -Pattern 'HYPEREZ_VERSION "([^"]+)"').Matches[0].Groups[1].Value
Write-Host "HyperEZ $version" -ForegroundColor White
$hyper = Join-Path $root "$BuildDir/hyper.exe"
$profileDir = Join-Path $root "$BuildDir/pgo-perfil"

if ($NoPGO) {
    Step "Compilando hyper.exe"
    Run cmake @("-S", ".", "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DHYPEREZ_PGO=")
    Run cmake @("--build", $BuildDir, "--target", "hyper")
} else {
    Step "Compilando hyper.exe con instrumentación (PGO 1/3)"
    if (Test-Path $profileDir) { Remove-Item -Recurse -Force $profileDir }
    Run cmake @("-S", ".", "-B", $BuildDir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DHYPEREZ_PGO=generate")
    Run cmake @("--build", $BuildDir, "--target", "hyper")

    Step "Entrenando con las pruebas, los ejemplos y los benchmarks (PGO 2/3)"
    $env:NO_COLOR = "1"
    Quiet $hyper @("tests/bench/bench.hz")
    Quiet $hyper @("test", "tests")
    Quiet $hyper @("test", "examples")
    Quiet $hyper @("check", "examples", "tests")
    Remove-Item Env:NO_COLOR
    if (-not (Get-ChildItem $profileDir -Recurse -Filter *.gcda -ErrorAction SilentlyContinue)) { throw "El entrenamiento no generó perfiles en $profileDir" }

    Step "Compilando hyper.exe optimizado con el perfil (PGO 3/3)"
    Run cmake @("-S", ".", "-B", $BuildDir, "-DHYPEREZ_PGO=use")
    Run cmake @("--build", $BuildDir, "--target", "hyper")
}

if (-not $SkipTests) {
    Step "Pruebas del lenguaje"
    Run ctest @("--test-dir", $BuildDir, "--output-on-failure")
}

if (-not $SkipVsix) {
    Step "Extensión de VS Code"
    Push-Location "editors/vscode"
    try {
        if (-not (Test-Path "node_modules")) { Run npm @("ci", "--no-audit", "--no-fund") }
        New-Item -ItemType Directory -Force "bin" | Out-Null
        Copy-Item $hyper "bin/hyper.exe" -Force
        Run python @("tools/gen_grammar.py")
        if (-not $SkipTests) { Run node @("test/grammar.test.js") }
        Run node @("tools/bundle.js")
        Get-ChildItem "*.vsix" -ErrorAction SilentlyContinue | Remove-Item
        Run npx @("vsce", "package", "--no-dependencies", "--allow-missing-repository", "--allow-star-activation", "-o", "hyperez-$version.vsix")
        if ($VSCodeTests) { Run node @("test/runTest.js") }
    } finally { Pop-Location }
}

Step "Instalador"
Run cmake @("-S", ".", "-B", $BuildDir)   # recoge el .vsix nuevo
Run cmake @("--build", $BuildDir)

Step "Copiando a dist/"
New-Item -ItemType Directory -Force "dist" | Out-Null
Get-ChildItem "dist" -File | Remove-Item
Copy-Item "$BuildDir/packaging/windows/HyperEZ-Setup-$version.exe" "dist/" -Force
Copy-Item $hyper "dist/" -Force
if (-not $SkipVsix) { Copy-Item "editors/vscode/hyperez-$version.vsix" "dist/" -Force }
Get-ChildItem dist | ForEach-Object { "{0,-32} {1,8:N0} KB" -f $_.Name, ($_.Length / 1KB) }
Write-Host "`nListo. Ejecuta dist\HyperEZ-Setup-$version.exe para instalar HyperEZ." -ForegroundColor Green
