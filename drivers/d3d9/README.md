# Direct3D 9 Fixed Function rendering driver

Shader Model 3 hardware optionally supports per-pixel Lambert lighting with up to eight selected light influences per draw. The shared per-vertex path remains available on older hardware and through `rendering/quality/fixed_function/per_pixel_lighting=false`. Shader bytecode is included; no runtime shader compiler is required.

Optional directional shadows use one hard-edged orthographic map for the first eligible sun, including opaque/cutout and skinned/morphed casters. Per-pixel receivers preserve ambient, emission and other lights, with material opt-out and shadow opacity. Vertex-lit and unavailable shader paths stay unshadowed. `rendering/quality/fixed_function/directional_shadows` disables them; the existing directional-shadow size setting controls map size. Cascades, soft filtering and omni/spot shadows are not implemented.

This directory implements a Direct3D 9 device backend for the Fixed Function renderer. It supports windowed presentation, opaque offscreen passes, colored/textured triangles, Godot-compatible projections, blending, alpha cutoff, clipping, optional depth testing/writing, culling, idle reset/recovery and synchronous frame readback. RGBA8 textures support capability-checked nearest/bilinear filtering and U/V repeat, with contents retained across reset. It integrates with the experimental Fixed Function 2D/diffuse 3D renderer and editor GUI.

Opaque and capability-supported transparent LDR viewports can be sampled and read back after a completed pass. Transparent targets preserve premultiplied RGBA and require redraw after reset. Streamed lit meshes can use standalone per-pixel AO with independent UVs.

## Windows versions

On fully patched Windows XP, the maximum DirectX is the latest patch release of 9.0c, `9.29.1974` from 2010-06-07. D3D9 is fully supported and recommended on XP. You can download it here: https://www.microsoft.com/en-us/download/details.aspx?id=8109 or here: https://archive.org/details/directx9-jun2010-redist

On fully patched Windows 2000, the maximum DirectX is a slightly earlier patch release of 9.0c, `9.26.1590` from 2009-03-16. D3D9 is supported and recommended on Windows 2000, assuming you do not need anything added in the last year of DX9's life. Missing features are mostly optional libraries like D3DX, not core Direct3D 9 features.

On fully patched Windows 98 SE and ME, the maximum DirectX is an even earlier patch release of 9.0c, `9.15.779` from 2006-12-08 for 98 SE, and `9.25.1476` from 2008-10-27 for ME. D3D9 may work, but consider using D3D7 instead if supporting these Windows versions is important. You can download it here: https://archive.org/details/directx9-dec2006-redist

Direct3D 9 is not available on Windows 95, Windows NT 4.0, or earlier.

## Building

Build the backend with the modern Windows platform using `d3d9=yes`. It uses the original Direct3D 9 API (`Direct3DCreate9`) and dynamically loads `d3d9.dll`.

Build an editor with `tests=yes d3d9=yes`, then run it with `--test --test-case="*D3D9*" --no-skip`. The native GPU probe is skipped by default. Add `--d3d9-capture=path.ppm` to save captures of triangles, textures, blending, clipping, projections, splash images, storage, targets, sampling, recovery, pass composition, canvas commands and partial uploads. The probe uses Godot's core math implementation.

Opaque render-to-texture uses default-pool XRGB textures. Window/target depth surfaces are allocated lazily, including D16 support. Targets can be sampled after a completed pass; their contents require redraw after device reset. Transparent targets and sampled depth textures are not implemented.

Immutable indexed geometry uses lazy default-pool vertex/index buffers with CPU snapshots for recovery. Eligible lit meshes use per-object SM3 transforms and lighting constants; optional normal textures use a separate tangent/UV layout and lazily created shaders. Skinned and morphed meshes with authored tangent frames use streamed normal mapping, optionally combined with texture AO. Source indices are partitioned into local 16-bit batches, with immediate indexed fallback when native buffers are unavailable.

## License

The entire contents of this `d3d9` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d9` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
