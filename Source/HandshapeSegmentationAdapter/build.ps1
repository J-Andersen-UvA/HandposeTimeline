param([string]$handshapeSegmentationRoot)

$adapterRoot = $PSScriptRoot
$pluginRoot = Resolve-Path (Join-Path $adapterRoot "..\..")
$buildDirectory = Join-Path $pluginRoot "Intermediate\HandshapeSegmentationBridge"
if ([string]::IsNullOrWhiteSpace($handshapeSegmentationRoot)) {
    $handshapeSegmentationRoot = Join-Path $pluginRoot "ThirdParty\HandshapeSegmentation"
}
cmake `
    -S $adapterRoot `
    -B $buildDirectory `
    -A x64 `
    "-DHANDSHAPE_SEGMENTATION_ROOT=$handshapeSegmentationRoot"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build $buildDirectory --config Release
exit $LASTEXITCODE
