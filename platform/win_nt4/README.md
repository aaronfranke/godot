# Windows NT 4 platform

This Godot platform code targets Windows NT 4, which is the fully 32-bit professional/enterprise/server version of Windows from the 1990s, with very limited support for graphics. It will also run on later versions of Windows, tested on Windows 11.

This platform builds x86_32 editors and GDScript export templates and shares the Unicode NT implementation in [`drivers/win_legacy/nt`](../../drivers/win_legacy/nt/README.md). It does not depend on the NT5 platform.

Use an i686 MSVCRT/POSIX MinGW toolchain. The build uses x87 rather than SSE/SSE2 and PE OS/subsystem version 4.00. MSVC is unavailable for this target. Use `target=editor` to build an editor.

```powershell
scons platform=win_nt4 arch=x86_32 target=template_release use_mingw=yes mingw_prefix=C:/toolchains/mingw32 optimize=size
```

D3D3 Software and OpenGL1 are enabled by default. Select Fixed Function with the `d3d3_software` or `opengl1` driver. For a dedicated headless template, disable both with `d3d3_software=no opengl1=no`. D3D7, D3D9, Compatibility and Jolt are unavailable.

The runtime supports a native window, keyboard and captured mouse input, borderless fullscreen, Unicode clipboard text, native message/input prompts, WinMM audio playback/capture, filesystem/process services, networking/TLS, and GodotPhysics2D/3D.

OpenGL1 without framebuffer objects draws only the main window; offscreen viewports, viewport textures, readback and embedded dialogs are unavailable. D3D3 Software provides opaque sub-viewports and embedded dialogs with one-bit text. Restart D3D3 applications after changing desktop color depth. NT4 has no native menus or screen keep-on request and uses a single-screen fallback.

The **Windows NT 4** export preset supports EXE and ZIP exports, separate or embedded PCKs, console wrappers, and optional icon/version resource editing on Windows editor hosts. Templates use the `win_nt4_debug_x86_32.exe` and `win_nt4_release_x86_32.exe` names. Use the `windows` and `win_nt4` feature tags for native extensions built for NT4.

## License

The entire contents of this `win_nt4` folder is released into the public domain, licensed under The Unlicense.

Usage of this code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
