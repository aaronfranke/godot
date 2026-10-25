# Direct3D for DirectX 3 software Fixed Function rendering driver

This driver provides Direct3D 3 (the Direct3D that shipped with DirectX 3) software rendering in the Fixed Function renderer. DirectX is the multimedia API family, while Direct3D is its 3D graphics API. The `d3d3_software` folder name identifies the DirectX release and software-only implementation.

**Caution:** Software rendering is much slower than hardware-accelerated rendering. Performance will scale very poorly with high resolutions and large amounts of objects. Even in the best case, this rendering driver will perform worse than every other by a huge margin. Consider switching to D3D7 and later, or OpenGL 1.1 and later, as long as your target hardware supports it (which is extremely likely).

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
