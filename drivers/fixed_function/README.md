# Fixed Function rendering in Godot 4.x

Fixed Function is an experimental renderer for a subset of Godot's 2D and 3D features. It runs through the modern Windows platform code and provides four drivers. The table includes Godot's built-in rendering methods for comparison.

| Feature             | Forward+ | Mobile | Compat. | D3D9 | GL1 | D3D7 | D3D3 |
| ------------------- | -------- | ------ | ------- | ---- | --- | ---- | ---- |
| GUI and sprites     | ✅       | ✅     | ✅      | ✅   | ✅  | ✅   | ✅   |
| Textured 3D         | ✅       | ✅     | ✅      | ✅   | ✅  | ✅   | ✅   |
| Skin and morph      | ✅       | ✅     | ✅      | ✅   | ✅  | ✅   | ✅   |
| CPU particles       | ✅       | ✅     | ✅      | ✅   | ✅  | ✅   | ✅   |
| GPU particles       | ✅       | ✅     | 🟡      | ❌   | ❌  | ❌   | ❌   |
| Font outlines       | ✅       | ✅     | ✅      | ✅   | ✅  | ✅   | ⚠️   |
| Subviewports        | ✅       | ✅     | ✅      | ✅   | 🧩  | ⚠️🧩 | ⚠️🧩 |
| Transparent targets | ✅       | ✅     | ✅      | 🧩   | 🧩  | ❌   | ❌   |
| Canvas shaders      | ✅       | ✅     | ✅      | 🟡   | 🟡  | 🟡   | 🟡   |
| Spatial shaders     | ✅       | ✅     | ✅      | 🟡   | 🟡  | 🟡   | 🟡   |
| Sky                 | ✅       | ✅     | ✅      | 🟡   | 🟡  | 🟡   | ⚠️   |
| Emission and AO     | ✅       | ✅     | ✅      | 🟡   | ⚠️  | ⚠️   | ⚠️   |
| 3D lights           | ✅       | ✅     | ✅      | ⚠️   | ⚠️  | ⚠️   | ⚠️   |
| Distance fog        | ✅       | ✅     | ✅      | ⚠️🧩 | ⚠️  | ⚠️🧩 | ❌   |
| Mipmaps             | ✅       | ✅     | ✅      | 🧩   | ✅  | 🧩   | ❌   |
| 3D shadows          | ✅       | ✅     | ✅      | 🟡🧩 | ❌  | ❌   | ❌   |
| Normal maps         | ✅       | ✅     | ✅      | 🧩   | ❌  | ❌   | ❌   |
| 2D lights           | ✅       | ✅     | ✅      | ❌   | ❌  | ❌   | ❌   |
| Triplanar maps      | ✅       | ✅     | ✅      | ❌   | ❌  | ❌   | ❌   |
| Baked lightmaps     | ✅       | ✅     | 🟡      | ❌   | ❌  | ❌   | ❌   |
| 3D MSAA             | ✅       | ✅     | 🟡      | ❌   | ❌  | ❌   | ❌   |
| Screen effects      | ✅       | 🟡     | 🟡      | ❌   | ❌  | ❌   | ❌   |
| HDR 2D              | ✅       | ✅     | ❌      | ❌   | ❌  | ❌   | ❌   |
| VoxelGI and SDFGI   | ✅       | ❌     | ❌      | ❌   | ❌  | ❌   | ❌   |

Legend

- ✅ Supported
- 🟡 Subset
- ⚠️ Approximation
- 🧩 Extra capability required (such as an OpenGL extension)
- ❌ Unavailable

Compat. means Godot's Compatibility renderer. GL1 means desktop OpenGL 1.1. D3D3 means DirectX 3-era Direct3D in software. The table describes the Windows engine profile. Device limits and optional capabilities can vary. For more detail on the built-in rendering methods, see [Godot's renderer comparison](https://docs.godotengine.org/en/latest/tutorials/rendering/renderers.html).

## Choosing a driver

Select **Fixed Function** in the editor's rendering-method menu and choose a compiled driver in `rendering/fixed_function/driver`. From the command line, use `--rendering-method fixed_function --rendering-driver d3d9`, substituting `d3d7`, `opengl1`, or `d3d3_software` as needed. D3D7 and D3D3 Software are x86_32 only. Fixed Function uses opaque native windows and main-thread rendering.

When sharing scripts across branches, use lowercase driver identifiers on Godot 4.x, such as `d3d9` and `opengl1`. Godot 3.x uses the case-sensitive video-driver names `D3D9` and `OpenGL1` with `--video-driver`.

- [D3D9](../d3d9/) offers optional Shader Model 3 per-pixel Lambert lighting, up to eight selected lights per draw, normal maps, texture AO and one hard directional shadow map. Other lighting draws use per-vertex Lambert.
- [D3D7](../d3d7/) uses per-vertex lighting. Its default window limit is 2048 client pixels per axis. The oversize setting controls scaling or a guarded runtime override.
- [OpenGL1](../opengl1/) requires framebuffer objects for engine viewports. Native non-power-of-two textures are optional. Transparent viewports also need separate-alpha blending.
- [D3D3 Software](../d3d3_software/) is the software fallback. It uses one-bit glyph coverage, simplified sky fills and base-level textures. Texture uploads and viewport backing fit the device's limits.

## What the partial features mean

- **Canvas.** The canvas draws rectangles, nine-patches, clipped and weighted polygons, polylines, meshes, MultiMeshes and nested viewport textures. Parallax requests above 4,096 copies per item draw the original once. Unsupported blend modes fall back to ordinary alpha blending where available. Unsupported custom effects and GPU particle draws are skipped. Use `CPUParticles2D` or `CPUParticles3D` for CPU particles. `Polygon2D` keeps its fill with aliased edges when antialiasing is requested.
- **Text.** MSDF fills and outlines use cached coverage atlases. LCD glyphs and missing atlas pages are skipped individually.
- **Materials.** Fixed Function recognizes selected material and shader inputs such as albedo, UV transforms, opacity, emission and texture AO. Unsupported spatial shaders use recognizable albedo or a magenta diagnostic. Per-instance opacity uses alpha blending where available. Visibility ranges use hard transitions. Lighting uses Lambert diffuse. On D3D9, `rendering/quality/fixed_function/per_pixel_lighting`, `normal_mapping` and `directional_shadows` control the optional shader paths.
- **Native lighting.** D3D9, D3D7 and OpenGL1 can shade eligible retained meshes with native per-vertex lighting. The default uses native directional and ambient lighting. Use `rendering/quality/fixed_function/native_lighting` to disable the native vertex path or opt in to approximate omni and spot falloff. D3D9 still prefers per-pixel lighting where available.
- **Environment.** D3D9, D3D7 and OpenGL1 draw panoramas and approximate procedural skies. Sky ambient is a uniform color. Distance fog uses a linear camera-depth fade, including as an approximation for exponential fog. D3D fixed-function fog requires table fog. Fixed Function ignores debanding and 3D antialiasing requests.
- **Textures and viewports.** Upload copies adapt to size, power-of-two and square-only limits while preserving Godot's source images. D3D7, D3D9, and OpenGL1 can upload BC1–BC3 natively when the device supports it, with CPU decoding as fallback. Drawable textures display their static base color. Fixed Function renders 3D at the viewport's native size and skips compositor effects. Transparent viewport textures use premultiplied alpha. Frozen viewports redraw after device loss.
- **Geometry.** Eligible static meshes reuse native indexed buffers. CPU-lit triangle batches reuse transformed, shaded vertices within a draw. CPU skinning, morphs, repeated instances and software drawing can cost more than on the built-in renderers.
- **Editor.** The grid, axes and selection outlines use clipped line geometry with optional antialiasing. Shader-based grid fading is unavailable. Viewport GPU times and FPS are estimated from elapsed render-thread work.

## License

The entire contents of this `fixed_function` folder is released into the public domain, licensed under The Unlicense.

Usage of this `fixed_function` code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
