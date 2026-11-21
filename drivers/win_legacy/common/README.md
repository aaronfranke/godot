# Windows 9x and NT shared runtime code

This folder contains runtime code shared by all legacy Windows targets: 9x, NT4, NT5, and NT6. Editor-side packaging lives in `../export/` and also builds on other host operating systems.

- `audio/` contains WaveOut playback, capture, and device routing.
- `input/` contains WinMM (Windows Multimedia) joypad/controller polling and mapping.
- `display/` contains native icon and cursor bitmap construction and optional DPI awareness for modern hosts.
- `launcher/` builds the standalone console companion for each windowed target.

- Code just for Windows 9x goes in `platform/win_9x/`.
- Code just for Windows NT4 goes in `platform/win_nt4/`.
- Code just for Windows NT5 goes in `platform/win_nt5/`.
- Code just for Windows NT6 in `platform/win_nt6/`.
