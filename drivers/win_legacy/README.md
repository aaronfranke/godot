# Windows Legacy shared platform code

This folder contains shared code for old Windows platforms. Named "win" to fit in with the platform folders using the "win" prefix, which are themselves named "win" to distinguish them from upstream Godot modern "windows" platform code.

- The `common/` folder contains runtime code shared by all legacy Windows targets: 9x, NT4, NT5, and NT6.
- The `nt/` folder contains runtime code shared by the NT4, NT5 and NT6 targets.
- The `export/` folder contains editor-side packaging for these targets, built on every editor host platform.
- Code just for Windows 9x goes in `platform/win_9x/`.
- Code just for Windows NT4 goes in `platform/win_nt4/`.
- Code just for Windows NT5 goes in `platform/win_nt5/`.
- Code just for Windows NT6 in `platform/win_nt6/`.

Godot 3.x keeps window APIs on OS; the shared display helper does not add a DisplayServer API to the engine. The platform owns its build options, API baseline, entry point, feature tags, and exporter identity.

Shared runtime sources are selected by this folder's `SCsub`. Windowed builds define `WIN_LEGACY_WINDOWED_ENABLED`; dedicated headless builds omit the window and device sources. Shared export packaging is compiled for editor builds on every host platform.

## License

The entire contents of this `win_legacy` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_legacy` code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
