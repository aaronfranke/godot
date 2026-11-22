import os
import subprocess
import sys


def get_flags():
    return [
        ("modules_enabled_by_default", False),
        ("module_bcdec_enabled", True),
        ("module_bmp_enabled", True),
        ("module_csg_enabled", True),
        ("module_cvtt_enabled", "${tools}"),
        ("module_dds_enabled", True),
        ("module_enet_enabled", True),
        ("module_etcpak_enabled", True),
        ("module_freetype_enabled", True),
        ("module_gdscript_enabled", True),
        ("module_gltf_enabled", True),
        ("module_gridmap_enabled", True),
        ("module_hdr_enabled", True),
        ("module_jpg_enabled", True),
        ("module_jsonrpc_enabled", True),
        ("module_mbedtls_enabled", True),
        ("module_minimp3_enabled", True),
        ("module_navigation_enabled", True),
        ("module_ogg_enabled", True),
        ("module_opensimplex_enabled", True),
        ("module_squish_enabled", True),
        ("module_stb_vorbis_enabled", True),
        ("module_svg_enabled", True),
        ("module_tga_enabled", True),
        ("module_theora_enabled", True),
        ("module_tinyexr_enabled", True),
        ("module_upnp_enabled", True),
        ("module_vhacd_enabled", True),
        ("module_vorbis_enabled", True),
        ("module_webp_enabled", True),
        ("module_webrtc_enabled", True),
        ("module_websocket_enabled", True),
        ("module_xatlas_unwrap_enabled", True),
    ]


def fail(message):
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(255)


def configure_msvc(env):
    # SCons defaults to the host architecture. Re-run discovery for the template architecture.
    architecture = "x86" if env["bits"] == "32" else "x86_64"
    env["TARGET_ARCH"] = architecture
    env["MSVC_SETUP_RUN"] = False
    env.Tool("msvc")
    if not env.get("MSVC_VERSION") or not env.WhereIs("cl.exe"):
        fail(f"Could not find MSVC tools for {architecture}. Install the C++ build tools or use use_mingw=yes.")
    env.Tool("mslink")
    env.Tool("mslib")
    env.extra_suffix = ".msvc" + env.extra_suffix

    env.Append(
        CPPDEFINES=["TYPED_METHOD_BIND"],
        CCFLAGS=["/MT" if env["use_static_cpp"] else "/MD", "/utf-8", "/bigobj", "/fp:strict"],
        LINKFLAGS=["/SUBSYSTEM:CONSOLE", "/STACK:8388608", "/INCREMENTAL:NO"],
    )
    if env["target"] == "debug":
        env.Append(CCFLAGS=["/Od"])
    else:
        env.Append(CCFLAGS=[{"speed": "/O2", "size": "/O1", "none": "/Od"}[env["optimize"]]])
    if env["target"] == "debug" or env["debug_symbols"]:
        env.Append(CCFLAGS=["/Zi", "/FS"], LINKFLAGS=["/DEBUG"])
    if env["lto"] == "full":
        env.Append(CCFLAGS=["/GL"], ARFLAGS=["/LTCG"], LINKFLAGS=["/LTCG"])


def configure_mingw(env, minimum):
    expected = "i686-w64-mingw32" if env["bits"] == "32" else "x86_64-w64-mingw32"
    prefix_option = "mingw_prefix_" + env["bits"]
    prefix = env[prefix_option]
    if env["mingw_prefix"]:
        prefix_option = "mingw_prefix"
        toolchain_bin = os.path.join(os.path.abspath(env["mingw_prefix"]), "bin")
        prefix = os.path.join(
            toolchain_bin, "" if os.path.isfile(os.path.join(toolchain_bin, "gcc.exe")) else expected + "-"
        )
    if os.path.dirname(prefix):
        prefix = os.path.join(os.path.abspath(os.path.dirname(prefix)), os.path.basename(prefix))
        env.PrependENVPath("PATH", os.path.dirname(prefix))
    for key, tool in (("CC", "gcc"), ("CXX", "g++"), ("AR", "gcc-ar"), ("RANLIB", "gcc-ranlib")):
        env[key] = env.WhereIs(prefix + tool)
        if not env[key]:
            fail(f"Could not find {prefix + tool}. Check {prefix_option} and the MinGW toolchain PATH.")
    actual = subprocess.check_output([env["CXX"], "-dumpmachine"], text=True).strip()
    if actual != expected:
        fail(f"Expected a MinGW compiler targeting {expected}, found {actual}.")

    env.use_windows_spawn_fix()
    env["split_libmodules"] = True
    if env["target"] == "debug":
        env.Append(CCFLAGS=["-g3"])
    else:
        env.Append(CCFLAGS=[{"speed": "-O2", "size": "-Os", "none": "-O0"}[env["optimize"]]])
        if env["debug_symbols"]:
            env.Append(CCFLAGS=["-g2"])
    env.Append(CCFLAGS=["-Wa,-mbig-obj", "-ffp-contract=off"])
    major = minimum >> 8
    minor = minimum & 0xFF
    env.Append(
        LINKFLAGS=[
            "-Wl,--subsystem,console",
            "-Wl,--stack,8388608",
            f"-Wl,--major-os-version,{major},--minor-os-version,{minor}",
            f"-Wl,--major-subsystem-version,{major},--minor-subsystem-version,{minor}",
        ]
    )
    if env["use_static_cpp"]:
        env.Append(LINKFLAGS=["-static"])
    if env["lto"] == "full":
        env.Append(CCFLAGS=["-flto"], LINKFLAGS=["-flto"])


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        BoolVariable("use_mingw", "Use MinGW GCC (required for portability to old Windows)", True),
        ("mingw_prefix", "Optional MinGW-w64 toolchain root", os.getenv("MINGW_PREFIX", "")),
        ("mingw_prefix_32", "i686 MinGW compiler prefix", os.getenv("MINGW32_PREFIX", "i686-w64-mingw32-")),
        ("mingw_prefix_64", "x86_64 MinGW compiler prefix", os.getenv("MINGW64_PREFIX", "x86_64-w64-mingw32-")),
        BoolVariable("use_static_cpp", "Link the compiler runtime statically", True),
        BoolVariable("debug_symbols", "Add debugging symbols to release templates", False),
    ]
