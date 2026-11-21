# Windows Legacy shared platform code

This folder contains shared code for old Windows platforms. Named "win" to fit in with the platform folders using the "win" prefix, which are themselves named "win" to distinguish them from upstream Godot modern "windows" platform code.

- The `export/` folder contains editor-side packaging for these targets, built on every editor host platform.
- Code just for Windows 9x goes in `platform/win_9x/`.
- Code just for Windows NT4 goes in `platform/win_nt4/`.
- Code just for Windows NT5 goes in `platform/win_nt5/`.
- Code just for Windows NT6 in `platform/win_nt6/`.

## License

The entire contents of this `win_legacy` folder is released into the public domain, licensed under The Unlicense.

Usage of this `win_legacy` code also requires agreeing to Godot's MIT License and Godot's other third-party licenses.
