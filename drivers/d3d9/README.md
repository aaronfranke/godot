# Direct3D 9 Fixed Function rendering driver

This directory implements an initial Direct3D 9 device backend for the Fixed Function renderer. It supports windowed presentation, colored/textured triangles, Godot-compatible projections, per-draw blending, alpha cutoff and rectangle clipping, resize/reset, and synchronous frame readback. Textures accept RGBA8 images, default to nearest/clamp sampling, and retain their contents across reset. Full Godot renderer integration is in progress.

Shader Model 3 devices support per-pixel Lambert lighting for directional, omni and spot lights. Each mesh draw selects up to eight lights by estimated influence; colors, energy, range, cone attenuation and masks are respected. Older hardware and numerically unsupported inputs use the shared per-vertex fallback. `rendering/quality/fixed_function/per_pixel_lighting` can disable the shader path. Shader bytecode is included; no runtime shader compiler is required.

Optional directional shadows use one hard-edged orthographic map for the first eligible sun, including opaque/cutout, skinned and morphed casters. Per-pixel receivers preserve ambient, emission and other lights; vertex-lit and unavailable shader paths stay unshadowed. `rendering/quality/fixed_function/directional_shadows` disables them, and `rendering/quality/directional_shadow/size` controls map size. Cascades, soft filtering and omni/spot shadows are not implemented.

## Windows versions

On fully patched Windows XP, the maximum DirectX is the latest patch release of 9.0c, `9.29.1974` from 2010-06-07. D3D9 is fully supported and recommended on XP. You can download it here: https://www.microsoft.com/en-us/download/details.aspx?id=8109 or here: https://archive.org/details/directx9-jun2010-redist

On fully patched Windows 2000, the maximum DirectX is a slightly earlier patch release of 9.0c, `9.26.1590` from 2009-03-16. D3D9 is supported and recommended on Windows 2000, assuming you do not need anything added in the last year of DX9's life. Missing features are mostly optional libraries like D3DX, not core Direct3D 9 features.

On fully patched Windows 98 SE and ME, the maximum DirectX is an even earlier patch release of 9.0c, `9.15.779` from 2006-12-08 for 98 SE, and `9.25.1476` from 2008-10-27 for ME. D3D9 may work, but consider using D3D7 instead if supporting these Windows versions is important. You can download it here: https://archive.org/details/directx9-dec2006-redist

Direct3D 9 is not available on Windows 95, Windows NT 4.0, or earlier.

## Building

Build the backend with the modern Windows platform using `d3d9=yes`. It uses the original Direct3D 9 API (`Direct3DCreate9`) and dynamically loads `d3d9.dll`.

Build a debug editor with `d3d9=yes`, then run it with `--test d3d9`. The native GPU probe runs only when explicitly selected. Add `--d3d9-capture=path.ppm` to save the main image and suffixed texture, blend, clip, projection, boot, storage, target, sampling, recovery, passes, and canvas PPM captures. The probe uses Godot's core math implementation.

Render-to-texture uses default-pool RGB or optional premultiplied RGBA targets, recreated after reset. Transparent targets require separate alpha blending and an alpha-capable blendable format; opaque rendering remains available without them. Completed targets can be sampled or read back, including alpha, and require redraw after reset. Sampleable depth textures are not implemented.

Per-draw bilinear min/mag filtering and U/V repeat are capability-checked. Mipmap sampling is not implemented.

Optional depth-enabled passes allocate a private depth buffer for the window or render target, with capability-checked formats and 16-bit fallback. Draws support independent depth testing/writing with conventional near-to-far depth, and explicit depth clears. Color-only passes retain their existing behavior. The shared Fixed Function rasterizer supplies engine mesh/material rendering.

Persistent indexed resources support both ordinary vertices and object-space lighting vertices. The optional SM3 vertex shader applies per-object transforms and normal matrices without reuploading geometry when cameras, lights or materials change. Transient SM3 and per-vertex fallbacks remain available.

## License

The entire contents of this `d3d9` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d9` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
