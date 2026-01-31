# ReptAll - 3D Repeater for Adobe After Effects

A camera-aware layer repeater effect plugin for Adobe After Effects.

## Features

- Creates multiple copies of a layer with incremental 3D transforms
- Camera-aware depth sorting for proper Z-order
- Supports 8-bit, 16-bit, and 32-bit float color depths
- Configurable translation, rotation, and scale steps per copy
- Opacity gradient across copies

## Building

### Windows (Visual Studio 2022)

1. Open `Win/ReptAll.sln` in Visual Studio 2022
2. Set `AE_PLUGIN_BUILD_DIR` environment variable to your After Effects plug-ins directory
3. Build the solution (Debug or Release)

### Mac (Xcode)

1. Open `Mac/ReptAll.xcodeproj` in Xcode
2. Build the target
3. Copy the resulting `.plugin` bundle to your After Effects plug-ins directory

## Requirements

- Adobe After Effects SDK (https://github.com/adobe/after-effects-sdk)
- Visual Studio 2022 (Windows) or Xcode 16+ (Mac)
- Windows 10/11 SDK or macOS 12+ SDK

## Version

Current version: 1.1.0

## License

See LICENSE file for details.

## Links

- GitHub: https://github.com/rebuildup/Ae_ReptAll
