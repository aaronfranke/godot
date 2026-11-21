# Windows NT shared code

This folder contains the Unicode NT runtime used by the `win_nt6` platform.

The root contains filesystem, path, and IP implementations. `os/` implements operating-system services. `display/` implements the native window, input, clipboard, and dialogs, with WGL context support in `display/opengl/`.

- `OS_WindowsNT6` derives from `OS_WindowsNT`.
- `DisplayServerWindowsNT6` derives from `DisplayServerWindowsNT`.
- `build.py` shares compiler setup and module defaults without importing another platform. The consuming platform selects the Windows API and executable baseline.
