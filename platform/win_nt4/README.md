# Windows NT 4 platform

This Godot platform code targets Windows NT 4, which is the fully 32-bit professional/enterprise/server version of Windows from the 1990s, with very limited support for graphics. It will also run on later versions of Windows, tested on Windows 11.

Targets fully patched Windows NT 4 SP6. This is a sibling of NT5 and NT6 over the [shared NT drivers](../../drivers/win_legacy/nt), with a private display helper adapted to Godot 3.x's OS API.

Use an i686 MSVCRT/POSIX MinGW toolchain with a compatible runtime. This target uses x87 and PE OS/subsystem version 4.00. MSVC is unavailable; the **Windows NT 4** export preset is also available in modern editors.

```powershell
scons platform=win_nt4 tools=no target=release bits=32 use_mingw=yes mingw_prefix=C:/toolchains/mingw32 optimize=size
```

D3D3 Software and OpenGL1 are enabled by default. Choose `--video-driver "D3D3 Software"` or `--video-driver OpenGL1`. For a dedicated headless template, disable both with `d3d3_software=no opengl1=no`.

For an editor, use `tools=yes target=release_debug`. Basic editing, scene import/save and running a project have been tested with D3D3 Software.

Without FBO extensions, OpenGL1 draws only the main window: offscreen viewports, viewport textures and readback are unavailable. D3D3 Software supports opaque sub-viewports with one-bit text. Restart D3D3 applications after changing desktop color depth. NT4 uses a single-screen fallback and cannot implement screen keep-on requests.

The runtime provides window/input, Unicode clipboard, WinMM audio, filesystem, process and IPv4 networking services. EXE and ZIP exports support sidecar or embedded PCKs, console wrappers, and optional native icon/version resource editing on Windows editor hosts. Templates use `win_nt4_debug_x86_32.exe` and `win_nt4_release_x86_32.exe` names.

## License

The entire contents of this `win_nt4` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
