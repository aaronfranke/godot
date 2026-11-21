import os
import sys

from methods import print_error
from platform_methods import validate_arch


def _fail(message):
    print_error(message)
    sys.exit(255)


def is_active():
    return True


def get_name():
    return "Windows 9x"


def can_build():
    # Build Windows 9x from modern Windows.
    return os.name == "nt"


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("target_win_version", "Minimum Windows version (auto = 0x0400 aka Windows 95)", "auto"),
        BoolVariable(
            "use_mingw",
            "Use MinGW GCC instead of MSVC. Required to make the build actually portable to old Windows.",
            True,
        ),
        ("mingw_prefix", "Path to the MinGW-w64 MSVCRT toolchain root", os.getenv("MINGW_PREFIX", "")),
        BoolVariable("use_static_cpp", "Link the compiler runtime statically", True),
        BoolVariable("d3d3_software", "Build the Fixed Function Direct3D 3 software device backend", True),
        BoolVariable("d3d7", "Build the Fixed Function Direct3D 7 device backend", True),
        BoolVariable("d3d9", "Build the Fixed Function Direct3D 9 device backend", True),
        BoolVariable("opengl1", "Build the Fixed Function OpenGL 1.1 device backend", True),
    ]


def get_flags():
    return {
        "arch": "x86_32",
        "rendering_device": False,
        "opengl3": False,
        "accesskit": False,
        "angle": False,
        "sdl": False,
        "modules_enabled_by_default": False,
        "module_astcenc_enabled": True,
        "module_basis_universal_enabled": True,
        "module_bcdec_enabled": True,
        "module_bmp_enabled": True,
        "module_csg_enabled": True,
        "module_cvtt_enabled": True,
        "module_dds_enabled": True,
        "module_enet_enabled": True,
        "module_etcpak_enabled": True,
        "module_fbx_enabled": True,
        "module_gdscript_enabled": True,
        "module_freetype_enabled": True,
        "module_gltf_enabled": True,
        "module_godot_physics_2d_enabled": True,
        "module_godot_physics_3d_enabled": True,
        "module_gridmap_enabled": True,
        "module_hdr_enabled": True,
        "module_interactive_music_enabled": True,
        "module_jpg_enabled": True,
        "module_jsonrpc_enabled": True,
        "module_ktx_enabled": True,
        "module_mbedtls_enabled": True,
        "module_meshoptimizer_enabled": True,
        "module_mp3_enabled": True,
        "module_msdfgen_enabled": True,
        "module_multiplayer_enabled": True,
        "module_navigation_2d_enabled": True,
        "module_navigation_3d_enabled": True,
        "module_noise_enabled": True,
        "module_objectdb_profiler_enabled": True,
        "module_ogg_enabled": True,
        "module_svg_enabled": True,
        "module_text_server_adv_enabled": True,
        "module_text_server_fb_enabled": True,
        "module_tga_enabled": True,
        "module_theora_enabled": True,
        "module_tilemap_enabled": True,
        "module_tinyexr_enabled": True,
        "module_upnp_enabled": True,
        "module_vhacd_enabled": True,
        "module_visual_shader_enabled": True,
        "module_vorbis_enabled": True,
        "module_webp_enabled": True,
        "module_webrtc_enabled": True,
        "module_websocket_enabled": True,
        "module_zip_enabled": True,
    }


def get_tools(env):
    from SCons.Tool.MSCommon import msvc_exists

    if env["use_mingw"] or not msvc_exists():
        return ["mingw"]
    env["TARGET_ARCH"] = "x86" if env["arch"] == "x86_32" else "amd64"
    return ["msvc", "mslink", "mslib"]


def configure(env):
    validate_arch(env["arch"], get_name(), ["x86_32"])
    if env.editor_build or env["rendering_device"] or env["opengl3"]:
        _fail("win_9x supports native export templates with Fixed Function or headless rendering.")
    # 0x0400 = Windows 95, 0x0410 = Windows 98, or 0x0490 = Windows ME.
    if env["target_win_version"] == "auto":
        env["target_win_version"] = "0x0400"
        env["target_win_version_int"] = 0x0400
    else:
        version_int = 0x0400
        try:
            version_int = int(env["target_win_version"], 0)
        except ValueError:
            _fail("target_win_version must be 'auto' or a numeric Windows version such as 0x0400.")
        if version_int < 0x0400:
            _fail("Target Windows version too low for `win_9x`.")
        elif version_int >= 0x0500:
            _fail("Target Windows version too high for `win_9x`. Consider using `platform=win_nt5` instead.")
        env["target_win_version_int"] = version_int

    minimum = env["target_win_version_int"]
    env.Prepend(CPPPATH=["#platform/win_9x"])
    env.Append(
        CPPDEFINES=[
            "WINDOWS_ENABLED",
            "WIN_9X_ENABLED",
            ("WINVER", hex(minimum)),
            ("_WIN32_WINDOWS", hex(minimum)),
            ("_WIN32_WINNT", "0x0400"),
        ]
    )
    system_libs = ["advapi32", "gdi32", "iphlpapi", "ole32", "shell32", "user32", "ws2_32"]
    env["win_legacy_windowed"] = env["d3d3_software"] or env["d3d7"] or env["d3d9"] or env["opengl1"]
    if env["win_legacy_windowed"]:
        env.Append(CPPDEFINES=["WIN_LEGACY_WINDOWED_ENABLED", "FIXED_FUNCTION_ENABLED"])
        system_libs.append("winmm")
    for option, define in (
        ("d3d3_software", "D3D3_SOFTWARE_ENABLED"),
        ("d3d7", "D3D7_ENABLED"),
        ("d3d9", "D3D9_ENABLED"),
        ("opengl1", "OPENGL1_ENABLED"),
    ):
        if env[option]:
            env.Append(CPPDEFINES=[define])
    if env["d3d3_software"] or env["d3d7"]:
        system_libs.append("dxguid")
    if env["opengl1"]:
        system_libs.append("opengl32")
    env.msvc = "mingw" not in env["TOOLS"]
    if env.msvc:
        env.Append(CPPDEFINES=["TYPED_METHOD_BIND", "MBEDTLS_TEST_SW_INET_PTON"])
        env.Append(CCFLAGS=["/MT" if env["use_static_cpp"] else "/MD", "/utf-8", "/bigobj", "/fp:strict"])
        subsystem = "WINDOWS" if env["win_legacy_windowed"] else "CONSOLE"
        env.Append(LINKFLAGS=[f"/SUBSYSTEM:{subsystem}", "/STACK:8388608", "/INCREMENTAL:NO"])
        env.Append(LINKFLAGS=[lib + env["LIBSUFFIX"] for lib in system_libs])
    else:
        import subprocess

        from SCons.Subst import quote_spaces

        if env["mingw_prefix"]:
            env.PrependENVPath("PATH", os.path.join(os.path.abspath(env["mingw_prefix"]), "bin"))
        for key, tool in {"CC": "gcc", "CXX": "g++", "AR": "gcc-ar", "RANLIB": "gcc-ranlib"}.items():
            env[key] = env.WhereIs(f"i686-w64-mingw32-{tool}") or env.WhereIs(tool)
            if not env[key]:
                _fail(f"Could not find {tool}. Set mingw_prefix to an i686 MSVCRT/POSIX MinGW toolchain.")
        triple = subprocess.check_output([env["CXX"], "-dumpmachine"], text=True).strip()
        if triple != "i686-w64-mingw32":
            _fail(f"Expected i686-w64-mingw32, found {triple}.")
        # Use the pinned runtime's standard threads and x87 code, without the bundled XP-only wrappers.
        env.use_windows_spawn_fix()
        env["ARCOM_ORIG"] = env["ARCOM"]
        env["ARCOM"] = "${TEMPFILE('$ARCOM_ORIG', '$ARCOMSTR')}"
        env["TEMPFILESUFFIX"] = ".rsp"
        env["TEMPFILEARGESCFUNC"] = lambda arg: quote_spaces(arg.replace("\\", "/"))
        env.Append(
            CCFLAGS=[
                "-Wa,-mbig-obj",
                "-ffp-contract=off",
                "-march=i686",
                "-mno-sse",
                "-mno-sse2",
                "-mfpmath=387",
            ]
        )
        # WINVER uses 0x10/0x90 for 98/ME; PE version fields use decimal 10/90.
        minor = ((minimum >> 4) & 0xF) * 10 + (minimum & 0xF)
        env.Append(
            LINKFLAGS=[
                "-mwindows" if env["win_legacy_windowed"] else "-mconsole",
                "-Wl,--stack,8388608",
                f"-Wl,--major-os-version,4,--minor-os-version,{minor}",
                f"-Wl,--major-subsystem-version,4,--minor-subsystem-version,{minor}",
            ]
        )
        env.Append(LIBS=system_libs)
        if env["use_static_cpp"]:
            env.Append(LINKFLAGS=["-static"])
        env.extra_suffix = ".mingw" + env.extra_suffix
    if env["lto"] == "auto":
        env["lto"] = "none"
