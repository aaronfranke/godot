# Shared Windows NT desktop integration

NT5 and NT6 use this native display implementation. Their platform folders provide the concrete DisplayServer subclasses and driver registration.

## Mouse input

The display server supports five mouse buttons, wheel events, capture, confinement, and mouse-mode overrides. The cursor can be moved relative to the focused window. In captured mode it stays centered. Windows sends native horizontal wheel messages starting with Vista. [Side buttons](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-xbuttondown) · [Horizontal wheel](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-mousehwheel)

## Shaped windows

`Window.mouse_passthrough_polygon` and `DisplayServer.window_set_mouse_passthrough()` set an input and drawing region in client coordinates. Windows clips everything outside the polygon, including any window decorations. Pass an empty polygon to restore the whole window. `FLAG_MOUSE_PASSTHROUGH` requires another native window in the same application and is unavailable on this single-window backend. [Win32 window regions](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn)

## Window activation

`Window.unfocusable` and `display/window/size/no_focus` are currently unavailable. Windows 2000 can show a window without activating it, but its native maximize and restore calls can still activate that window. [Win32 window visibility and activation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)

## Controller input

The windowed display server polls WinMM joystick devices with up to six axes, 32 buttons, and one point-of-view hat. Headless templates omit the controller backend. The driver exposes calibrated axes and numbered buttons to Godot's existing Input system, where projects can bind actions and set dead zones. On devices with a hat, the hat uses the standard D-pad indices. Numbered buttons from index 11 onward move up by four to keep those inputs distinct. [Microsoft WinMM joystick reference](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/)

WinMM's joystick API supplies axes, buttons, and a hat. It provides no rumble, motion sensor, or touchpad data. Product names are reported by the driver, and the backend does not infer standardized gamepad layouts from them. The controller must be exposed as connected by the Windows joystick driver. Physical controller compatibility still needs hardware testing.

## Audio capture

Enable `audio/driver/enable_input` to use `AudioStreamMicrophone` or the `AudioServer` input methods. WinMM captures 16-bit PCM and tries the playback rate first, then common legacy input rates. It duplicates mono samples into both channels, and `AudioServer.get_input_mix_rate()` reports the negotiated rate. [WinMM recording formats](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveinopen)

Use the device names from `AudioServer.get_input_device_list()` and `get_output_device_list()`. Both lists start with `Default`, which selects the Windows mapper. Unknown names also select the mapper. Playback switches on the audio thread. A live switch keeps the current sample rate and requires the new device to accept it. If WinMM refuses a second capture stream, the driver briefly stops capture and tries the new device, then tries to restore the previous input if the switch fails. A rejected playback switch keeps the current output.

Automatic hotplug recovery remains unavailable. Dedicated headless templates use Dummy audio and offer no capture.

## Cursor shapes

The windowed display server supports system cursor shapes and custom images with a hotspot. Clearing a custom image restores the system shape. Windows 2000 uses a one-bit transparency mask, with alpha values of 128–255 visible. On Windows 11, custom cursors retain partial alpha. [Win32 cursor behavior](https://learn.microsoft.com/en-us/windows/win32/menurc/about-cursors)

## Window icons

The main window accepts Godot images and native `.ico` files. To package a native icon in the PCK, add `*.ico` to the export preset's non-resource inclusion filter. The display server picks frames for the system's small and large icon sizes and decodes PNG frames with Godot's PNG loader. On Windows 2000, Godot images and PNG frames use one-bit transparency, and bitmap frames use the file's own mask. This controls the window icon at runtime. Explorer reads a separate icon from the executable's resources. [Win32 window icons](https://learn.microsoft.com/en-us/windows/win32/menurc/about-icons)

## Notification area

Windowed templates support `StatusIndicator` icons, Unicode tooltips, mouse and keyboard activation, and native popup menus. With an empty icon, the indicator uses the standard Windows application icon. The display server re-registers its icons after a taskbar creation message. Windows 2000 returns an empty rectangle for indicator bounds and anchors the menu at the pointer. Windows 7 and later can provide the Shell's icon bounds. Right-to-left popup layout is available starting with Windows XP. [Windows notification area](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area)

## Clipboard

The windowed display server supports plain Unicode text copy and paste. Copied text uses Windows CRLF line endings. Windows converts ANSI and OEM clipboard text when needed. [Windows clipboard formats](https://learn.microsoft.com/en-us/windows/win32/dataxchg/clipboard-formats)

Clipboard image reads accept PNG, uncompressed DIB and DIBV5 data, and device-dependent bitmap handles. Explicit alpha in PNG and bitfield DIBs is preserved. Bitmap data with embedded or linked color profiles is read using its stored channel values. Compressed DIBs and other image formats need an accompanying PNG or uncompressed bitmap representation. [Windows bitmap headers](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-header-types)

## Input method editors

The windowed display server uses Windows IMM for input methods supplied by the OS. Godot draws preedit text at the caret and provides the caret position and text bounds to the native candidate window. Losing focus cancels unfinished composition. Windows 2000 needs an enabled IME and its language components for composition. Ordinary Unicode input also works on systems without an input context. [Windows Input Method Manager](https://learn.microsoft.com/en-us/windows/win32/intl/input-method-manager)

## Keyboard layouts

The windowed display server lists the keyboard layouts Windows has loaded and can activate one on the main thread. It reports the layout name and input language and translates physical keys for shortcuts and localized prompts. Letter labels use unshifted mappings from the layout's legacy code page, with a virtual-key fallback for letters outside that set. Label lookups leave pending accents intact. [Win32 keyboard input](https://learn.microsoft.com/en-us/windows/win32/inputdev/keyboard-input)

## File drops

The main window accepts Shell drops of existing files and directories and emits Godot's `Window.files_dropped` signal with their Unicode paths. Drop sources that supply virtual items need to expose them as filesystem paths. [Windows Shell file drops](https://learn.microsoft.com/en-us/windows/win32/shell/wm-dropfiles)

## Native file dialogs

Windowed templates use the Unicode Windows common dialogs to open one or more files and select a Save As path. The Shell browser handles folder and open-any selection. A file picker accepts filename filters and returns a zero-based filter index. The folder and open-any browser uses the Shell's visibility settings, returns one filesystem path shorter than `MAX_PATH`, and reports filter index zero. [Windows common dialogs](https://learn.microsoft.com/en-us/windows/win32/dlgbox/open-and-save-as-dialog-boxes) · [Windows Shell browser](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shbrowseforfolderw)

`DisplayServer.file_dialog_show()` starts one dialog at a time on the main window. The game loop continues while the dialog is open. The callback runs on the main thread and receives an empty selection on cancellation. For restricted roots and additional controls, use Godot's built-in `FileDialog`.

## Native message and input dialogs

Windowed templates show Unicode message and text input prompts on the main window. The game loop pauses until the prompt closes, then the callback runs on the main thread. Message callbacks receive the zero-based button index, or `-1` when the prompt is dismissed. Input callbacks receive the accepted text, including an empty string. Canceling input skips the callback. Long messages scroll, and prompts share the main window with native file pickers one at a time. [Win32 modal dialog API](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxindirectparamw)

Long descriptions use a scrolling text area instead of a full-height dialog. Short messages size to their contents.

`OS.alert()` uses a native alert when the main window is available and writes to standard error in headless mode.

## Screen idle

Windowed templates honor `display/window/energy_saving/keep_screen_on` at startup. Call `DisplayServer.screen_set_keep_on()` on the main thread to change it at runtime. While enabled, the game requests that Windows keep the display and system awake during idle periods. User-initiated sleep and Windows screen saver security policies still apply. Headless templates leave screen idle behavior to Windows. [Win32 execution state](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate) · [Screen saver policy](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-syscommand)

Godot 4.x has no built-in script API for battery status. A native GDExtension can query the system through Win32. [Win32 power status](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getsystempowerstatus)
