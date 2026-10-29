# HandshapeSegmentation dependency

This build links the Unreal adapter to the original HandshapeSegmentation checkout. It does not copy the upstream math or matching implementation into the plugin.

Requirements:

- A current standalone vcpkg checkout in `VCPKG_ROOT`, or passed with `-vcpkgRoot`.
- The local HandshapeSegmentation checkout.
- CMake and Visual Studio 2022.

Build from PowerShell:

```powershell
./build.ps1 -handshapeSegmentationRoot "C:\path\to\HandshapeSegmentation"
```

The generated static library is written to:

```text
lib/Win64/HandshapeSegmentationUpstream.lib
```

The Unreal adapter contains only animation-pose extraction and conversion code. Segmentation math, pose distance, and match sorting execute inside the upstream static library.
