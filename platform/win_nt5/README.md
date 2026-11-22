# Windows NT 5 platform

This Godot platform code targets Windows 2000 SP4, XP, Vista, Server 2003, and Server 2008 R1. Essentially, Windows from the 2000s decade. It will also run on later versions of Windows, tested on Windows 11.

Targets fully patched Windows. x86_64 builds require Windows XP Professional x64 Edition or newer.

This platform builds editors and GDScript export templates over the [shared NT drivers](../../drivers/win_legacy/nt). The **Windows 2000/XP/Vista** export preset is also available in modern editors.

```powershell
scons platform=win_nt5 tools=no target=release bits=32 use_mingw=yes mingw_prefix=C:/toolchains/mingw32 optimize=size
```

Use an MSVCRT/POSIX MinGW toolchain for old Windows deployment. MSVC builds are supported for development on modern Windows. Use `bits=64` with an x86_64 compiler for the XP x64 baseline.

For an editor, use `tools=yes target=release_debug`. Basic editing, scene import/save and running a project have been tested on Windows 2000 SP4.

[Fixed Function](../../drivers/fixed_function/README.md) is enabled by default: D3D7 on x86_32, D3D9 and OpenGL1 on both architectures. Select a compiled backend with `--video-driver D3D7`, `D3D9` or `OpenGL1`. Optional `opengl3=yes` builds Godot 3.x's existing GLES2/GLES3 renderers; they require a capable installed OpenGL driver. Separate-thread rendering is available only with GLES.

For a dedicated headless template, use `opengl3=no d3d7=no d3d9=no opengl1=no d3d3_software=no`. Windowed templates also accept `--no-window` or `--video-driver Dummy`.

The runtime provides window/input, Unicode filesystem and clipboard, WinMM audio/MIDI/controllers, processes, networking/TLS and bundled media codecs. Set `network/ssl/certificates` for a project CA bundle. Optional GDNative libraries (`module_gdnative_enabled=yes`) and their dependencies must target the selected Windows version; `Windows.win_nt5.32` identifies NT5-specific x86_32 libraries.

EXE and ZIP exports support sidecar or embedded PCKs, console wrappers, and optional native icon/version resource editing on Windows editor hosts. Templates use names such as `win_nt5_release_x86_32.exe`.

## License

The entire contents of this `win_nt5` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
