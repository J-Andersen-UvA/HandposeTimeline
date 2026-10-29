param(
    [Parameter(Mandatory = $true)]
    [string]$handshapeSegmentationRoot,

    [string]$vcpkgRoot = $env:VCPKG_ROOT
)

$dependencyRoot = $PSScriptRoot
$buildDirectory = Join-Path $dependencyRoot "build"
if ([string]::IsNullOrWhiteSpace($vcpkgRoot)) {
    throw "Set VCPKG_ROOT to a current standalone vcpkg checkout or pass -vcpkgRoot."
}
$toolchainFile = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $toolchainFile)) {
    throw "Could not find the vcpkg toolchain at $toolchainFile"
}

cmake `
    -S $dependencyRoot `
    -B $buildDirectory `
    -A x64 `
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile" `
    "-DHANDSHAPE_SEGMENTATION_ROOT=$handshapeSegmentationRoot"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build $buildDirectory --config Release
exit $LASTEXITCODE
