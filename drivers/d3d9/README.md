# Direct3D 9 driver

This directory implements an initial Direct3D 9 device backend for the Fixed Function renderer. It supports windowed presentation, colored triangles, transforms, resize/reset, and synchronous frame readback. Godot rasterizer integration is not implemented yet.

## Windows versions

On fully patched Windows XP, the maximum DirectX is the latest patch release of 9.0c, `9.29.1974` from 2010-06-07. D3D9 is fully supported and recommended on XP. You can download it here: https://www.microsoft.com/en-us/download/details.aspx?id=8109 or here: https://archive.org/details/directx9-jun2010-redist

On fully patched Windows 2000, the maximum DirectX is a slightly earlier patch release of 9.0c, `9.26.1590` from 2009-03-16. D3D9 is supported and recommended on Windows 2000, assuming you do not need anything added in the last year of DX9's life. Missing features are mostly optional libraries like D3DX, not core Direct3D 9 features.

On fully patched Windows 98 SE and ME, the maximum DirectX is an even earlier patch release of 9.0c, `9.15.779` from 2006-12-08 for 98 SE, and `9.25.1476` from 2008-10-27 for ME. D3D9 may work, but consider using D3D7 instead if supporting these Windows versions is important. You can download it here: https://archive.org/details/directx9-dec2006-redist

Direct3D 9 is not available on Windows 95, Windows NT 4.0, or earlier.

## License

The entire contents of this `d3d9` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `d3d9` driver also requires agreeing to Godot's MIT License, Godot's other third-party licenses, and DirectX's license under the Microsoft Software License Terms.
