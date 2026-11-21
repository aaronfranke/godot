# Windows NT 5.x platform

This Godot platform code targets Windows 2000, XP, Vista, Server 2003, and Server 2008 R1. Essentially, Windows from the 2000s decade. It will also run on later versions of Windows, tested on Windows 11.

`win_nt5` builds GDScript export templates using the [shared NT implementation](../../drivers/win_legacy/nt/README.md), with one native window and embedded dialogs. The x86_32 build has been tested on Windows 2000 SP4 and Windows 11. The x86_64 baseline is Windows XP Professional x64 Edition; guest verification on that OS remains outstanding. Use a modern editor to create and export projects.

## Building and exporting

Use a MinGW-w64 GCC toolchain with MSVCRT and POSIX threads to target old Windows. MSVC builds are supported for development on modern Windows.

```powershell
scons platform=win_nt5 arch=x86_32 target=template_release use_mingw=yes mingw_prefix=C:/toolchains/mingw32 optimize=size
```

Use `target=template_debug` for a debug template, or `arch=x86_64` for an XP x64 baseline. Select the matching custom templates in the **Windows 2000/XP/Vista** export preset. Windowed builds include a `.console.exe` companion for console input and logs.

For a dedicated headless template, disable graphics with `opengl3=no d3d7=no d3d9=no opengl1=no d3d3_software=no`.

## Rendering and optional features

[Fixed Function](../../drivers/fixed_function/README.md) is enabled by default with D3D7 on x86_32 and D3D9 on both architectures. OpenGL1 and D3D3 Software are optional. Select `fixed_function` as the rendering method and choose a compiled backend through `rendering/fixed_function/driver` or `--rendering-driver`.

Build with `opengl3=yes` to add Compatibility (OpenGL 3.3). It can fall back to Fixed Function on the main rendering thread. Compatibility remains unqualified on XP; separate-thread rendering is available only with Compatibility.

GodotPhysics is the default. Optional builds can enable:

- `module_jolt_physics_enabled=yes` for Jolt Physics.
- `module_raycast_enabled=yes` for Embree occlusion culling.
- `cvtt_export_templates=yes` for runtime BC6H/BC7 encoding.

Jolt and Embree require SSE2 on x86_32. C#, XR, and native camera capture are unavailable. See the [shared display limitations](../../drivers/win_legacy/nt/display/README.md) for window and input restrictions.

TLS uses Godot's bundled CA certificates by default. Applications can explicitly supply the Windows roots returned by `OS.get_system_ca_certificates()`.

GDExtensions and their dependent DLLs must target the selected Windows version and architecture. The `windows.win_nt5.x86_32` feature combination selects an NT5-specific library; keep dependencies beside it. WebRTC requires a compatible native provider.

## License

The entire contents of this `win_nt5` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_nt5` code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
