# Windows NT 6.1+ platform

This Godot platform code targets Windows 7, 8, 8.1, 10, and Server 2008 R2 and later. Essentially, Windows from the 2010s decade. It will also run on later versions of Windows, tested on Windows 11.

Targets fully patched Windows. Builds x86_32/x86_64 export templates and editors over the [shared NT drivers](../../drivers/win_legacy/nt).

MSVCRT/POSIX MinGW is recommended for compatibility with old Windows; MSVC is also supported.

```powershell
scons platform=win_nt6 tools=no target=release bits=64 use_mingw=yes mingw_prefix=C:/toolchains/mingw64
```

Use `target=release_debug` for debug templates, or `tools=yes target=release_debug` for an editor. Select matching templates in the **Windows 7/8/8.1/10** export preset. EXE and ZIP exports support PCKs, console wrappers, and optional native icon/version resource editing on Windows editor hosts.

Godot 3.x's GLES2/GLES3 and [Fixed Function](../../drivers/fixed_function/README.md) are enabled by default. Select `GLES2`, `GLES3`, `D3D9` or `OpenGL1` through `--video-driver`. GLES3 can fall back to GLES2 using the existing project setting; Fixed Function can fall back among available backends. Separate-thread rendering is available only with GLES.

D3D7 and D3D3 Software are optional x86_32 backends. For a dedicated headless build, use `opengl3=no d3d7=no d3d9=no opengl1=no d3d3_software=no`.

## License

The entire contents of this `win_nt6` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
