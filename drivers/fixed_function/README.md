# Fixed Function rendering in Godot 3.x

Fixed Function is an experimental renderer for a subset of Godot's 2D and 3D features. It runs through the modern Windows platform code and provides four drivers. The table includes Godot's GLES backends for comparison.

| Feature             | GLES3 | GLES2 | D3D9 | GL1 | D3D7 | D3D3 |
| ------------------- | ----- | ----- | ---- | --- | ---- | ---- |
| GUI and sprites     | ✅    | ✅    | ✅   | ✅  | ✅   | ✅   |
| Textured 3D         | ✅    | ✅    | ✅   | ✅  | ✅   | ✅   |
| Skin and morph      | ✅    | ✅    | ✅   | ✅  | ✅   | ✅   |
| CPU particles       | ✅    | ✅    | ✅   | ✅  | ✅   | ✅   |
| GPU particles       | ✅    | ❌    | ❌   | ❌  | ❌   | ❌   |
| Font outlines       | ✅    | ✅    | ✅   | ✅  | ✅   | ⚠️   |
| Subviewports        | ✅    | ✅    | ✅   | 🧩  | ⚠️🧩 | ⚠️🧩 |
| Transparent targets | ✅    | ✅    | 🧩   | 🧩  | ❌   | ❌   |
| Canvas shaders      | ✅    | ✅    | 🟡   | 🟡  | 🟡   | 🟡   |
| Spatial shaders     | ✅    | ✅    | 🟡   | 🟡  | 🟡   | 🟡   |
| Sky                 | ✅    | ✅    | 🟡   | 🟡  | 🟡   | ⚠️   |
| Emission and AO     | ✅    | ✅    | 🟡   | ⚠️  | ⚠️   | ⚠️   |
| 3D lights           | ✅    | ✅    | ⚠️   | ⚠️  | ⚠️   | ⚠️   |
| Distance fog        | ✅    | ✅    | ⚠️🧩 | ⚠️  | ⚠️🧩 | ❌   |
| Mipmaps             | ✅    | ✅    | 🧩   | ✅  | 🧩   | ❌   |
| 3D shadows          | ✅    | ✅    | 🟡🧩 | ❌  | ❌   | ❌   |
| Normal maps         | ✅    | ✅    | 🧩   | ❌  | ❌   | ❌   |
| 2D lights           | ✅    | ✅    | ❌   | ❌  | ❌   | ❌   |
| Triplanar maps      | ✅    | ✅    | ❌   | ❌  | ❌   | ❌   |
| Baked lightmaps     | ✅    | ✅    | ❌   | ❌  | ❌   | ❌   |
| 3D MSAA             | ✅    | 🧩    | ❌   | ❌  | ❌   | ❌   |
| Screen effects      | ✅    | 🟡    | ❌   | ❌  | ❌   | ❌   |
| 3D HDR              | ✅    | ❌    | ❌   | ❌  | ❌   | ❌   |
| GI probes           | ✅    | ❌    | ❌   | ❌  | ❌   | ❌   |

Legend:

- ✅ Supported
- 🟡 Subset
- ⚠️ Approximation
- 🧩 Extra capability required (such as an OpenGL extension)
- ❌ Unavailable

GL1 means desktop OpenGL 1.1. D3D3 means DirectX 3-era Direct3D in software. The table describes the Windows engine profile with usable driver capabilities. Individual devices can have smaller texture limits or lack optional sampling and blending features. The [GLES2 and GLES3 comparison](https://docs.godotengine.org/en/3.6/tutorials/rendering/gles2_gles3_differences.html) covers their additional differences.

## Choosing a driver

Select a compiled driver in the editor's video-driver menu or use `--video-driver D3D9`, `D3D7`, `OpenGL1` or `"D3D3 Software"`. D3D7 and D3D3 Software are x86_32 only. Fixed Function currently uses opaque windows and main-thread rendering.

When sharing scripts across branches, use the case-sensitive video-driver names `D3D9` and `OpenGL1` with `--video-driver` on Godot 3.x. Godot 4.x uses lowercase identifiers such as `d3d9` and `opengl1` with `--rendering-driver`.

- [D3D9](../d3d9/) offers optional Shader Model 3 per-pixel Lambert lighting, up to eight selected lights per draw, normal maps, texture AO and one hard directional shadow map. Other lighting draws use per-vertex Lambert.
- [D3D7](../d3d7/) uses per-vertex lighting. Its default window limit is 2048 client pixels per axis. The oversize setting controls scaling or a guarded runtime override.
- [OpenGL1](../opengl1/) requires framebuffer objects for engine viewports. Native non-power-of-two textures are optional. Transparent viewports also need separate-alpha blending.
- [D3D3 Software](../d3d3_software/) is the software fallback. It uses one-bit glyph coverage, simplified sky fills and base-level textures. Texture uploads and viewport backing fit the device's limits.

## What the partial features mean

- **Canvas.** Common rectangles, nine-patches, clipping, meshes, polylines, weighted polygons and nested viewport textures draw through the same canvas adapter. Canvas materials can select supported blend modes. Unsupported custom effects fall back to ordinary drawing or are omitted. `Polygon2D` keeps its fill with aliased edges when antialiasing is requested. For particles, use `CPUParticles` or `CPUParticles2D`.
- **Text.** DynamicFont uses its rasterized outline glyphs. A distance-field BitmapFont Label can use `shadow_as_outline`, which draws four offset copies. Missing distance-field glyph pages are omitted individually.
- **Materials.** Fixed Function recognizes selected material and shader inputs such as albedo, UV transforms, opacity, emission and texture AO. Unsupported spatial shaders use recognizable albedo or a magenta diagnostic. Lighting uses Lambert diffuse, with CPU per-vertex fallback. Emission and AO coverage vary with texture and lighting capabilities.
- **Native lighting.** Eligible per-vertex draws use approximate omni and spot falloff by default. Set `rendering/quality/fixed_function/native_lighting` to `1` for CPU local-light falloff with native directional and ambient lighting. D3D9 still prefers per-pixel lighting where supported.
- **Environment.** D3D9, D3D7 and OpenGL1 draw panoramas and approximate procedural skies. Sky ambient is a uniform color. Distance fog uses a linear camera-depth fade. D3D fixed-function fog requires table fog. GI, reflection probes, glow and screen-space ambient occlusion are unavailable.
- **Textures and viewports.** Upload copies adapt to size, power-of-two and square-only limits while preserving Godot's source images. D3D7, D3D9, and OpenGL1 can upload BC1–BC3 natively when the device supports it, with CPU decoding as fallback. Transparent viewport textures use premultiplied alpha. Frozen viewports redraw after device loss.
- **Performance.** Eligible static meshes reuse native indexed buffers. CPU skinning, morphs, repeated instances and software drawing can cost more than on the GLES backends.

## License

The entire contents of this `fixed_function` folder is released into the public domain, licensed under The Unlicense.

Usage of this `fixed_function` code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
