# HandposeTimeline

## Installation

1. Clone the [AdditiveBoneCorrection](https://github.com/J-Andersen-UvA/AdditiveBoneCorrection) plugin

2. Clone the [TouchCorrection](https://github.com/J-Andersen-UvA/TouchCorrection.git) plugin

3. Clone the [SemanticSurfacePatches](https://github.com/J-Andersen-UvA/SemanticSurfacePatches.git) plugin

4. Clone this repository

5. Build the HandposeSegmentation submodule:
```powershell
git submodule update --init --recursive
./Source/HandshapeSegmentationAdapter/build.ps1
```
- If the build fails, some required installations may be missing. In that case, please follow the instructions in the [HandshapeSegmentation README](https://github.com/JustinSaenCity/HandshapeSegmentation.git)

6. Build your Unreal project as usual