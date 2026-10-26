# Desktop OpenGL 1.1 for Fixed Function rendering driver

This driver supports desktop-class OpenGL 1.1 rendering in the Fixed Function renderer.

The OpenGL device implements window rendering with OpenGL 1.1 client arrays and a single texture stage. It uses optional framebuffer objects for offscreen targets and accepts non-power-of-two textures when supported. Optional core/EXT separate-alpha blending enables transparent LDR viewports and alpha readback; missing or unusable procedures retain opaque FBO support. Transparent textures store premultiplied RGBA. The platform-neutral device uses a context provider.

On Windows, LinuxBSD, or macOS, build with `opengl1=yes`. Choose the Fixed Function rendering method and `opengl1` driver, or launch with `--rendering-method fixed_function --rendering-driver opengl1`. The engine profile requires framebuffer objects. Native non-power-of-two textures are optional. The renderer adapts native textures and render targets while preserving CPU images and logical dimensions.

On LinuxBSD, the build requires X11 support and desktop OpenGL development files (`pkg-config gl`). The compiled executable also needs the userspace ABI and X11 libraries it was built to target. The driver uses GLX on X11, including XWayland on Wayland desktops. It supports a single opaque native window, with Godot subwindows embedded in it. Swap interval changes depend on the GLX swap control extensions exposed by the driver.

On macOS, the driver uses a legacy-profile AppKit OpenGL context with a single opaque native window. Godot subwindows are embedded in that window. Run games in a standalone window when launching them from the editor. The macOS backend currently targets the existing Godot macOS platform's deployment range.

On Windows, build with `tests=yes` and run `--test --test-case=*OpenGL1* --no-skip`. Add `--opengl1-software` for the system software pixel format, or `--opengl1-disable-npot` to test POT adaptation. The latter also works on all three platforms as a user argument after `--` when running the engine. The strict 1.1 native probe remains usable without FBOs or native NPOT textures. Supplied mip chains use native nearest-level or trilinear filtering. BC1–BC3 textures use native S3TC when available. Decoded uploads and channel-only views use the accepted source image. Single-level textures and viewport targets retain base-level sampling.

## Operating system versions

OpenGL 1.1 is supported on Windows 95 OSR2 (1996) and later, and Windows NT 4 (1996) and later. OpenGL 1.1 is the best API available for Windows NT 4. Windows NT 4 also supports D3D3 software rendering, but OpenGL 1.1 will be orders of magnitude faster.

OpenGL 1.1 is supported on Mac OS 9 (1999) and later, including all Mac OS X versions.

OpenGL 1.1 is supported on Linux kernel 2.2 (1999) and later, and any Linux/Unix system with Mesa 2.0 or later.

As for this specific Godot OpenGL 1.1 driver, it may or may not be possible to actually run on those ancient systems.

## License

The entire contents of this `opengl1` driver folder is released into the public domain, licensed under The Unlicense.

Usage of this `opengl1` driver also requires agreeing to Godot's MIT License, and Godot's other third-party licenses.
