# Windows NT 6.1+ platform

This Godot platform code targets Windows 7, 8, 8.1, 10, and Server 2008 R2 and later. Essentially, Windows from the 2010s decade. It will also run on later versions of Windows, tested on Windows 11.

`win_nt6` builds GDScript export templates and native editors for x86_32 and x86_64. It uses the [shared NT implementation](../../drivers/win_legacy/nt/README.md), with one native window and embedded dialogs. Templates have been tested on Windows 7 SP1 and Windows 11.

## Building and exporting

MinGW with MSVCRT and POSIX threads is recommended for compatibility with old Windows. MSVC builds are also supported.

```powershell
scons platform=win_nt6 target=template_release arch=x86_32 use_mingw=yes mingw_prefix=C:/toolchains/mingw32
```

Use `target=template_debug` for a debug template or `target=editor` for an editor. Select matching custom templates in the **Windows 7/8/8.1/10** export preset. Windowed builds include a `.console.exe` companion for console input and logs.

## Rendering

Compatibility (OpenGL 3.3) is enabled by default and supports separate-thread rendering. Select `gl_compatibility` as the rendering method. The graphics driver must provide OpenGL 3.3. For a dedicated headless build, use `opengl3=no`.

See the [shared display limitations](../../drivers/win_legacy/nt/display/README.md) for window and input restrictions.

## License

The entire contents of this `win_nt6` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_nt6` code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
