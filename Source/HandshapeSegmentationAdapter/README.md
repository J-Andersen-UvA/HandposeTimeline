# HandshapeSegmentation dependency

This build links the Unreal adapter to the HandshapeSegmentation Git submodule. It does not copy the upstream math or matching implementation into the plugin.

Requirements:

- The initialized `../../ThirdParty/HandshapeSegmentation` submodule.
- CMake and Visual Studio 2022.

Build from PowerShell:

```powershell
git submodule update --init --recursive
./Source/HandshapeSegmentationAdapter/build.ps1
```

The generated static library is written to:

```text
Source/HandshapeSegmentationAdapter/Lib/Win64/HandshapeSegmentationUpstream.lib
```

The Unreal adapter contains only animation-pose extraction and conversion code. Segmentation math, pose distance, and match sorting execute inside the upstream static library.
