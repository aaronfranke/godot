# Windows NT shared code

This folder contains shared code for the NT4, NT5 and NT6 platforms.

The root contains the Unicode filesystem, path, and IP implementations. `os/` implements shared operating-system services. `display/` implements the native window, input, clipboard, and dialogs, with optional WGL context support in `display/opengl/`.

- `OS_WindowsNT4`, `OS_WindowsNT5` and `OS_WindowsNT6` derive from `OS_WindowsNT`.
- `DisplayServerWindowsNT4`, `DisplayServerWindowsNT5` and `DisplayServerWindowsNT6` derive from `DisplayServerWindowsNT`. The platform subclasses register the appropriate concrete display server, including when Compatibility falls back to Fixed Function.

`build.py` shares compiler setup and module defaults without importing another platform. The consuming platform selects the Windows API and executable baseline. Common sources preserve the Windows 2000-compatible paths used by NT5.

- Code just for Windows NT4 goes in `platform/win_nt4/`.
- Code just for Windows NT5 goes in `platform/win_nt5/`.
- Code just for Windows NT6 goes in `platform/win_nt6/`.
