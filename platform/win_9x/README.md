# Windows 9x platform

This Godot platform code targets Windows 95, 98, 98 SE, and ME systems. Essentially, Windows from the 1990s decade. It will also run on later versions of Windows, tested on Windows 11.

The x86_32 builds provide native ANSI windows, keyboard and mouse input,
clipboard text, WinMM audio/controllers, files, child processes, and IPv4 networking.
Rendering uses the shared Fixed Function backends. Native filesystem paths must be
representable in the system code page; resource names inside PCKs retain Unicode.

Use an MSVCRT/POSIX MinGW runtime for deployment. Windows 95 is the default target;
select `target_win_version=0x0410` for Windows 98. The Windows 95 profile also needs
a runtime built without SSE instructions. Modern MSVC builds are for development on newer Windows,
not deployment to Windows 9x.

```powershell
scons platform=win_9x bits=32 tools=no target=release use_mingw=yes
```

For an editor, use `tools=yes target=release_debug`. Basic editing, scene import/save
and running a project have been tested on Windows 98 SE. Basic project editing is
also tested on Windows 95 with an SSE-free compiler runtime.

## License

The entire contents of this `win_9x` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_9x` code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
