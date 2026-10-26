# Direct3D 7 Fixed Function rendering driver

This directory implements a Direct3D 7 device backend for the Fixed Function renderer. It supports windowed presentation, opaque offscreen passes, colored/textured triangles, Godot-compatible projections, blending, alpha cutoff, clipping, optional depth testing/writing, culling, idle surface recovery and synchronous frame readback. RGBA8 textures support capability-checked nearest/bilinear filtering and U/V repeat, with contents retained across recreation. Device creation preserves the caller's FPU precision. It integrates with the experimental Fixed Function 2D/opaque-unlit 3D renderer and editor GUI.

Direct3D 7, released as part of DirectX 7, represents the fixed-function pipeline era. It is the last Direct3D version without programmable vertex or pixel shaders. [Programmable shaders were introduced with DirectX 8](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-models). Direct3D 8 and 9 also retain fixed-function rendering.

Lighting is limited to per-vertex Lambert shading. Use D3D9 with Shader Model 3 hardware for per-pixel lighting; Windows 95 remains limited to per-vertex lighting in this renderer.

## Windows versions

Windows 2000 and XP ship with full support for Direct3D 7 out of the box, assuming capable graphics hardware and drivers. Both can be updated to DirectX 9.0c for Direct3D 9 support, and XP SP2 and later versions already include DirectX 9.0c.

On fully patched Windows 95, 98, 98 SE and ME, Direct3D 7 is fully supported, assuming capable graphics hardware and drivers. Direct3D 7 is the ideal graphics API when targeting Windows 9x, though there is some support for DirectX 8 and 9.

Direct3D 7 is not available on Windows NT 4.0 or earlier, and is not available on pre-Win95 DOS-based Windows.

Direct3D 7 is not available in Windows apps compiled for the x86_64 or ia64 (Itanium) architectures. The minimum available Direct3D version for 64-bit apps is D3D9, because [Direct3D 8 and earlier versions are unavailable to native 64-bit Windows apps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/sixty-four-bit-programming-for-game-developers#porting-applications-to-64-bit-platforms). Apps compiled for x86_32 can run on x86_64 Windows through WoW64.

## Building

Build the backend with the modern Windows platform using `arch=x86_32 d3d7=yes`. Native x86_64 builds must leave `d3d7=no`. The backend dynamically loads `ddraw.dll` and uses the Direct3D 7 interfaces.

Build an editor with `tests=yes d3d7=yes`, then run it with `--test --test-case="*D3D7*" --no-skip`. The native GPU probe is skipped by default. Add `--d3d7-capture=path.ppm` to save captures of triangles, textures, blending, clipping, projections, splash images, storage, targets, sampling, recovery, pass composition, canvas commands and partial uploads. The probe uses Godot's core math implementation.

Enable the startup setting `rendering/fixed_function/d3d7/force_rgb` to use the Direct3D RGB software device. By default, D3D7 tries hardware devices first and falls back to RGB. To run the native probe on RGB, add `--d3d7-force-rgb`.

The `rendering/fixed_function/d3d7/oversize_mode` setting selects Restricted (default), Stretch, Override or Force Override. Restricted caps normal window resizing at 2048 pixels per axis; forced oversized/fullscreen windows use proportional Stretch. Override attempts a guarded, process-local runtime patch bounded by device capabilities and the launch monitor. Force Override uses device capabilities alone. Unavailable overrides fall back to Stretch. Ordinary image textures retain their device limits.

Opaque render-to-texture uses supported RGB formats and render bit depths. Window/target depth surfaces are allocated lazily, with supported 16-bit fallback. Targets can be sampled after a completed pass; their contents require redraw after surface/device recovery. Transparent targets and sampled depth textures are not implemented.

Immutable indexed geometry uses lazy native vertex buffers with CPU snapshots for recovery. Source indices are partitioned into local 16-bit batches, with immediate indexed fallback when native buffers are unavailable.

## License

The entire contents of this `d3d7` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d7` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
