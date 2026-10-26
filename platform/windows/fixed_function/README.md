# Fixed Function Windows binding

This folder owns the optional modern Windows binding between native windows, Direct3D devices and the shared Fixed Function renderer.

Enabled backends append `D3D9` and `D3D7` to the existing video-driver choices. Selection bypasses GL context setup and connects resize, vsync, presentation and cleanup. The opaque-window 2D/diffuse-3D profile supports main-thread safe/unsafe modes; separate-thread rendering and transparent windows are unavailable. D3D7 is x86_32 only. Compiled backends appear in the experimental editor dropdown and support Save & Restart. The editor GUI, unlit and Lambert-lit 3D meshes, grid, axes and selection outlines can draw; unsupported shaders use visible fallbacks. Bounded unlit alpha blending and optional spatial line coverage antialiasing are supported. Sky requests use a clear-background fallback; CPU directional/ambient lighting supports mesh normals, masks and negative lights. Local-light shading, specular, shadows and shader-based grid fading remain unavailable.

The debug smoke project under `tests/project` checks native handles, ColorRect/Sprite/default-font drawing, viewport counters, resize and vsync. Run it with `--path platform/windows/fixed_function/tests/project --video-driver D3D9`; it exits after 20 frames. Add `--smoke-hold` to keep the window open for inspection.

Select `res://gui.tscn` on the same command line for the 30-frame GUI test: panels, flat/textured styleboxes, buttons, LineEdit, checkbox icons, keyboard activation, transforms and nested clipping. Optional `--gui-font=<font.ttf>` loads an existing font directly and checks live DynamicFont atlas updates, stable texture identity and CPU snapshot isolation. No font file is bundled. `--smoke-hold` also keeps this scene open.

`res://nine_patches.tscn` exercises every stretch/tile/tile-fit mode pair, source regions, center omission and resize; it exits after 24 frames or stays open with `--smoke-hold`.

`res://viewports.tscn` exercises nested ViewportContainers, first-use visibility, a frozen ViewportTexture, stretch/shrink, stable texture identity and forwarded button input. It exits after 30 frames or stays open with `--smoke-hold`.

With D3D7, `res://resolution.tscn` checks the configured oversize mode: programmatic resizing, maximizing, fullscreen fallback and large logical viewport textures. It exits after 25 frames. Set `rendering/quality/d3d7/oversize_mode` before launch to exercise each mode.

`res://unlit_3d.tscn` exercises opaque unlit meshes, material overrides, a CanvasItem overlay, camera projection and resize recovery.

`res://cutout_3d.tscn` exercises unlit alpha-scissor materials, albedo-map alpha, live threshold changes, a background visible through discarded fragments, GUI and resize.

`res://line_quality_3d.tscn` exercises overlapping transparent surfaces, vertex-alpha lines, live material edits, runtime line-AA toggling and resize. It exits after 40 frames or stays open with `--smoke-hold`. The renderer's `rendering/quality/fixed_function/antialiased_lines` setting defaults to true.

Open `res://editor_3d.tscn` in the 3D editor to inspect an unlit blue cube, a default white cube, an unsupported shader's magenta cube and a green Lambert-lit cube, with a directional light and ambient environment. The scene has no game camera. Select `res://lambert_3d.tscn` for a game-camera test with live material/light/ambient/mask edits, mirrored scaling, projection changes and resize; it exits after 48 frames, or stays open with `--smoke-hold`.

`res://local_lights_3d.tscn` exercises omni/spot Lambert lighting, live range/cone attenuation, color/energy/mask edits, mirrored scaling and camera/resize changes. It runs at up to 60 FPS, exits after 36 frames and supports `--smoke-hold`.

`res://pixel_lighting_3d.tscn` compares a coarse plane under per-pixel and per-vertex lighting, with more than eight lights, a GUI overlay and resize. It runs at up to 60 FPS and exits after 40 frames. With `--smoke-hold`, press Space to toggle the lighting path; per-pixel lighting requires D3D9 Shader Model 3 support.

## License

The entire contents of this `platform/windows/fixed_function` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
