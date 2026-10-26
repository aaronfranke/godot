# Direct3D for DirectX 3 software Fixed Function rendering driver

This driver provides Direct3D 3 (the Direct3D that shipped with DirectX 3) software rendering in the Fixed Function renderer. DirectX is the multimedia API family, while Direct3D is its 3D graphics API. The `d3d3_software` folder name identifies the DirectX release and software-only implementation.

**Caution:** Software rendering is much slower than hardware-accelerated rendering. Performance will scale very poorly with high resolutions and large amounts of objects. Even in the best case, this rendering driver will perform worse than every other by a huge margin. Consider switching to D3D7 and later, or OpenGL 1.1 and later, as long as your target hardware supports it (which is extremely likely).

Build the DirectX 3-era Direct3D backend with `arch=x86_32 d3d3_software=yes`. Use `--rendering-method fixed_function --rendering-driver d3d3_software` to run a game with the RGB software device. Build with `tests=yes` and run the native probe with `--test --test-case="*D3D3 Software*" --no-skip`.

The software fallback supports basic textured 3D, per-vertex lighting, and 16-bit depth. Startup probes enable alpha blending and opaque viewport textures where the runtime supports them. Godot integration requires opaque viewport textures, including for the editor's basic 3D preview; standalone device probes can still draw directly to a window. Startup uses Godot's window-size API to limit the window to 2048 client pixels per axis, preserving stricter existing limits. Fullscreen and forced oversized windows have no supported layout fallback. Offscreen viewport backing surfaces also fit within 2048 pixels per axis and honor lower device limits. Ordinary image textures are single-level and resized on upload as needed to fit the device. Choose OpenGL1, D3D7, or D3D9 for broader coverage and better editor performance.

Opaque, pixel-aligned canvas rectangles and solid bordered panels can use DirectDraw fills. Untinted, point-sampled viewport textures can use DirectDraw for one-to-one pixel copies and unclipped integer upscales, including mirrored draws where supported.

Sky backgrounds use DirectDraw fills for representative sky and ground colors separated by a horizontal horizon. Its height follows camera pitch and projection. Choose OpenGL1, D3D7, or D3D9 for skies that follow camera roll and sky orientation or display panorama detail.

D3D3 Software uses 1-bit glyph coverage. Eligible pixel-aligned monochrome glyphs use color-keyed DirectDraw blits. Other glyphs use a point-sampled binary mask in the triangle renderer.

Alpha-scissor materials fall back to ordinary alpha blending when available, without depth writes; hard alpha cutoffs are unsupported.

## Windows versions

Windows 98, 98 SE, ME, 2000, and XP all ship with full support for Direct3D 3 out of the box. This driver always uses software rendering.

On Windows 95, DirectX 3.0 support requires installing updates and redistributables.

On Windows NT 4.0, DirectX 3.0 support requires Service Pack 3 or later. Its Direct3D support is software-only. DirectX 3 is the maximum version for Windows NT 4.0. It is not possible to use later DirectX runtime versions, such as 5, 7, 9, etc.

Direct3D 3 is not available on Windows NT 3.5 or earlier, and is not available on pre-Win95 DOS-based Windows.

Direct3D 3 is not available in Windows apps compiled for the x86_64 or ia64 (Itanium) architectures. The minimum available Direct3D version for 64-bit apps is D3D9, because [Direct3D 8 and earlier versions are unavailable to native 64-bit Windows apps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/sixty-four-bit-programming-for-game-developers#porting-applications-to-64-bit-platforms). Apps compiled for x86_32 can run on x86_64 Windows through WoW64.

DirectX 3 has limited availability on Windows 8 and later, through partially-present legacy support.

## License

The entire contents of this `d3d3_software` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d3_software` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
