# Direct3D 7 driver

This driver supports Direct3D 7 rendering in the Fixed Function renderer.

Direct3D 7, released as part of DirectX 7, represents the fixed-function pipeline era. It is the last Direct3D version without programmable vertex or pixel shaders. [Programmable shaders were introduced with DirectX 8](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-models). Direct3D 8 and 9 also retain fixed-function rendering.

## Windows versions

Windows 2000 and XP ship with full support for Direct3D 7 out of the box, assuming capable graphics hardware and drivers. Both can be updated to DirectX 9.0c for Direct3D 9 support, and XP SP2 and later versions already include DirectX 9.0c.

On fully patched Windows 95, 98, 98 SE and ME, Direct3D 7 is fully supported, assuming capable graphics hardware and drivers. Direct3D 7 is the ideal graphics API when targeting Windows 9x, though there is some support for DirectX 8 and 9.

Direct3D 7 is not available on Windows NT 4.0 or earlier, and is not available on pre-Win95 DOS-based Windows.

Direct3D 7 is not available in Windows apps compiled for the x86_64 or ia64 (Itanium) architectures. The minimum available Direct3D version for 64-bit apps is D3D9, because [Direct3D 8 and earlier versions are unavailable to native 64-bit Windows apps](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/sixty-four-bit-programming-for-game-developers#porting-applications-to-64-bit-platforms). Apps compiled for x86_32 can run on x86_64 Windows through WoW64.

## License

The entire contents of this `d3d7` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d7` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
