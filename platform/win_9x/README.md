# Windows 9x platform

This Godot platform code targets Windows 95, 98, 98 SE, and ME systems. Essentially, Windows from the 1990s decade. It will also run on later versions of Windows, tested on Windows 11.

## Current implementation

MinGW x86_32 native-window and headless export templates have been tested on Windows 95 OSR2.1, Windows 98 SE, and Windows 11. Windows 95 has basic debug and release headless support, covering files, clocks, entropy, threads, packed assets, IPv4 networking, and child processes. The implementation includes native ANSI files and directories, safe-save replacement with rollback, and native DLL/GDExtension loading with sibling dependencies. Godot keeps Unicode data internally; native filenames must fit the system code page and legacy path limits. Additional Windows 95 services and rendering drivers remain unqualified.

The single-window display backend supports keyboard and mouse input, captured and confined mouse modes, resizing, and borderless fullscreen. Text clipboard exchanges preserve Unicode between aware applications and provide ANSI text for native applications. D3D3 Software has been tested on Windows 95 with a high-color display driver, including GUI text, textured 3D, mouse capture, and fullscreen. D3D7 RGB and D3D3 Software have been tested on Windows 98 SE; D3D9 and OpenGL1 have also been tested on Windows 11. WinMM (Windows Multimedia) provides stereo audio playback and named-device selection, also tested on Windows 95. Additional platform services remain in development.

WinMM (Windows Multimedia) controller polling supports generic axes, buttons, POV hats, reconnects, and background-input control. Simulated input and native enumeration have been tested on Windows 98 SE and Windows 11. Physical controllers remain untested; vibration is not implemented.

Unicode asset names inside exported PCKs remain supported. The native filename restrictions apply to files on the host filesystem.

WinMM (Windows Multimedia) audio and controllers, native icon bitmap construction, console launchers, and export packaging use [`drivers/win_legacy/common`](../../drivers/win_legacy/common/README.md). The ANSI filesystem, OS, and display implementation live in this platform folder.

Native file drops deliver file and folder paths through `Window.files_dropped`. ANSI paths use the system code page; Unicode-aware senders retain their Unicode paths, subject to the native filesystem restrictions above.

Blocking child-process pipes provide stdin and separate stdout/stderr byte streams. Nonblocking process pipes and local named-pipe servers are unavailable.

IPv4 networking supports TCP, UDP, multicast, WebSockets, ENet multiplayer, and UPnP discovery and port mapping. Bundled TLS supports verified HTTPS and secure WebSockets; encrypted ENet uses DTLS. IPv6 is unavailable.

Use the modern editor's **Windows 95/98/98SE/ME** exporter with a matching debug or release template. EXE and ZIP exports support separate or embedded PCKs. Embedded packs must remain at the end of the executable.

Windowed templates launch without a console. Use the companion `.console.exe` for console input and logs. **Debug > Export Console Wrapper** controls whether exports include it. Dedicated headless builds remain console programs.

Shell services provide standard folders, path expansion, and native file and Explorer opening. Legacy Explorer may show a target's parent folder without selecting the item.

Compatible MinGW thread startup and condition-variable support are linked into the executable. MSVC builds have been tested on Windows 11. Windows 95 requires `target_win_version=0x0400` and an MSVCRT/POSIX compiler runtime without SSE instructions; MinGW-Builds GCC 16.2.0 with MinGW-w64 14.0.0 is tested. D3D3 Software requires a compatible display driver and a desktop color depth of at least 16 bits.

```powershell
scons platform=win_9x target=template_release arch=x86_32 use_mingw=yes mingw_prefix=C:/toolchains/mingw32 d3d3_software=no d3d7=no d3d9=no opengl1=no
```

The command above builds a dedicated headless template. Omit the four renderer options to build a windowed template with the Fixed Function backends.

Use `target=editor` to build an editor. Basic editing, PNG/EXR/glTF import, scene saving, and running projects have been tested on Windows 98 SE and Windows 11.

## License

The entire contents of this `win_9x` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_9x` code also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and the Microsoft Software License Terms.
