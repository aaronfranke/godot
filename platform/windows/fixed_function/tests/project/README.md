# Fixed Function Windows test project

This smoke project checks native handles, ColorRect/Sprite2D/CanvasItemMaterial/default-font drawing, viewport counters, resize and vsync. Run it with `--path platform/windows/fixed_function/tests/project --rendering-method fixed_function --rendering-driver d3d9`; it exits after 20 frames. Append `-- --smoke-hold` to keep the window open. Additional GUI, tiled nine-patch, nested viewport, D3D7 resolution-policy and opaque unlit camera/mesh scenes are included. Select `res://unlit_3d.tscn` to exercise shared meshes, material overrides, texturing, a mirrored object, GUI, live edits and resize.

`res://cutout_3d.tscn` checks alpha scissor, threshold edits and resize; append `-- --alpha-blend` to switch to ordinary alpha blending during the test. Open `res://editor_3d.tscn` in the editor to inspect unlit, default and shader-fallback cubes with the normal grid and gizmos.
