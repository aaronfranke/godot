# Direct3D 7 Fixed Function rendering driver

This directory implements an initial Direct3D 7 device backend for the Fixed Function renderer. It supports windowed presentation, colored/textured triangles, Godot-compatible projections, per-draw blending, alpha cutoff and rectangle clipping, surface recreation, and synchronous frame readback. Textures accept RGBA8 images, default to nearest/clamp sampling, and retain their contents across recreation. Full Godot renderer integration is in progress.

Direct3D 7, released as part of DirectX 7, represents the fixed-function pipeline era. It is the last Direct3D version without programmable vertex or pixel shaders. [Programmable shaders were introduced with DirectX 8](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-models). Direct3D 8 and 9 also retain fixed-function rendering.

Lighting in this backend is limited to per-vertex lighting. Use D3D9 with Shader Model 3 hardware for per-pixel lighting. Windows 95 therefore remains limited to per-vertex lighting in this renderer.

## Windows versions

Windows 2000 and XP ship with full support for Direct3D 7 out of the box, assuming capable graphics hardware and drivers. Both can be updated to DirectX 9.0c for Direct3D 9 support, and XP SP2 and later versions already include DirectX 9.0c.

On fully patched Windows 95, 98, 98 SE and ME, Direct3D 7 is fully supported, assuming capable graphics hardware and drivers. Direct3D 7 is the ideal graphics API when targeting Windows 9x, though there is some support for DirectX 8 and 9.

Direct3D 7 is not available on Windows NT 4.0 or earlier, and is not available on pre-Win95 DOS-based Windows.

Direct3D 7 is not available in Windows apps compiled for the x86_64 or ia64 (Itanium) architectures. The minimum available Direct3D version for 64-bit apps is D3D9, because [Direct3D 8 and earlier versions are unavailable to native 64-bit Windows apps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/sixty-four-bit-programming-for-game-developers#porting-applications-to-64-bit-platforms). Apps compiled for x86_32 can run on x86_64 Windows through WoW64.

## Building

Build the backend with the modern Windows platform using `bits=32 arch=x86_32 d3d7=yes`. Native x86_64 builds must leave `d3d7=no`. The backend dynamically loads `ddraw.dll` and uses the Direct3D 7 interfaces.

Build a debug editor with `d3d7=yes`, then run it with `--test d3d7`. The native GPU probe runs only when explicitly selected. Add `--d3d7-capture=path.ppm` to save the main image and suffixed texture, blend, clip, projection, boot, storage, target, sampling, recovery, passes, and canvas PPM captures. The probe uses Godot's core math implementation.

Enable the startup setting `rendering/quality/d3d7/force_rgb` to use the Direct3D RGB software device. By default, D3D7 tries hardware devices first and falls back to RGB. To run the native probe on RGB, add `--d3d7-force-rgb`.

Opaque render-to-texture uses supported RGB formats and render bit depths. Targets can be sampled after a completed pass; their contents require redraw after surface/device recovery. Transparent targets and sampleable depth textures are not implemented.

Per-draw bilinear min/mag filtering and U/V repeat are capability-checked. Mipmap sampling is not implemented.

`rendering/quality/d3d7/oversize_mode` selects Restricted (default), Stretch, Override or Force Override and requires restart. Restricted caps ordinary window resizing at 2048 pixels per client axis; forced larger windows and oversized viewports use proportional backing and stretching. Override raises the native render-target limit using device caps and the launch monitor, while Force Override uses device caps alone. Both require recognized runtime instructions and writable code pages, modify only the loaded runtime on supported NT-family systems, and fall back to Stretch when unavailable. Ordinary image textures retain device-reported limits. Tiling is not implemented.

Optional depth-enabled passes allocate a private depth buffer for the window or render target, with capability-checked formats and 16-bit fallback. Draws support independent depth testing/writing with conventional near-to-far depth, and explicit depth clears. Color-only passes retain their existing behavior. The shared Fixed Function rasterizer supplies engine mesh/material rendering.

## License

The entire contents of this `d3d7` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d7` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
