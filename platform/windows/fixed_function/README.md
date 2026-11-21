# Fixed Function Windows binding

This folder owns the optional modern Windows binding between native windows, Direct3D devices and the shared Fixed Function renderer.

Compiled backends expose `--rendering-method fixed_function --rendering-driver d3d9`, `d3d7`, `opengl1` or `d3d3_software`. Choose the backend through `rendering/fixed_function/driver` or the command line. This binding connects native windows to device initialization, resize, vsync, presentation and cleanup. It uses opaque windows, main-thread rendering and one native window with embedded subwindows. D3D7 and D3D3 Software are x86_32 only.

The editor GUI and supported 2D/3D features use the shared renderer. See the [Fixed Function feature comparison](../../../drivers/fixed_function/README.md) for capabilities and limitations.

There is a debug test project under [`tests/project`](tests/project). See [its README](tests/project/README.md) for usage.

## License

The entire contents of this `platform/windows/fixed_function` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
