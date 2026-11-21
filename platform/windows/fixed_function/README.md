# Fixed Function Windows binding

This folder owns the optional modern Windows binding between native windows, Direct3D devices and the shared Fixed Function renderer.

Enabled backends expose `--rendering-method fixed_function --rendering-driver d3d9` or `d3d7`, also selectable through `rendering/renderer/rendering_method` and `rendering/fixed_function/driver`. Selection bypasses GL/RD setup and connects resize, vsync, presentation and cleanup. The opaque-window 2D/unlit 3D profile uses main-thread rendering and one native window; subwindows embed in it. D3D7 is x86_32 only. The editor rendering-method dropdown includes Fixed Function, using the `#afcc19` theme color. Choose the backend through `rendering/fixed_function/driver`; renderer changes restart the editor. The GUI, grid and unlit/fallback 3D geometry can draw, including alpha-cutout/blended materials and optional line coverage antialiasing. Sky and environment effects use clear-background/LDR fallbacks.

The smoke project under `tests/project` checks native handles, ColorRect/Sprite2D/CanvasItemMaterial/default-font drawing, viewport counters, resize and vsync. Run it with `--path platform/windows/fixed_function/tests/project --rendering-driver d3d9`; it exits after 20 frames. Append `-- --smoke-hold` to keep the window open. Additional GUI, tiled nine-patch, nested viewport, D3D7 resolution-policy and opaque unlit camera/mesh scenes are included. Select `res://unlit_3d.tscn` to exercise shared meshes, material overrides, texturing, a mirrored object, GUI, live edits and resize.

`res://cutout_3d.tscn` checks alpha scissor, threshold edits and resize; append `-- --alpha-blend` to switch to ordinary alpha blending during the test. Open `res://editor_3d.tscn` in the editor to inspect unlit, default and shader-fallback cubes with the normal grid and gizmos.

## License

The entire contents of this `platform/windows/fixed_function` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
