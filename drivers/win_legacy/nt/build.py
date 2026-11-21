import os
import subprocess
import sys

from methods import print_error


def get_flags(arch, opengl3=False):
    return {
        "arch": arch,
        "rendering_device": False,
        "opengl3": opengl3,
        "accesskit": False,
        "angle": False,
        "sdl": False,
        # New upstream modules need compatibility review before enabling them on legacy Windows.
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


def fail(message):
    print_error(message)
    sys.exit(255)


def configure(env, minimum):
    env.Prepend(CPPPATH=[f"#platform/{env['platform']}"])
    env.Append(
        CPPDEFINES=[
            "WINDOWS_ENABLED",
            "WIN_LEGACY_NT_ENABLED",
            env["platform"].upper() + "_ENABLED",
            ("WINVER", hex(minimum)),
            ("_WIN32_WINNT", hex(minimum)),
        ]
    )
    fixed_function = env["d3d7"] or env["d3d9"] or env.get("opengl1", False) or env.get("d3d3_software", False)
    env["win_legacy_windowed"] = fixed_function or env["opengl3"]
    env["target_win_version_int"] = minimum
    system_libs = [
        "advapi32",
        "crypt32",
        "gdi32",
        "iphlpapi",
        "ole32",
        "oleaut32",
        "shell32",
        "user32",
        "wbemuuid",
        "ws2_32",
    ]
    if env.editor_build:
        system_libs.append("shlwapi")
    if env["win_legacy_windowed"]:
        env.Append(CPPDEFINES=["WIN_LEGACY_WINDOWED_ENABLED", "WINMIDI_ENABLED"])
        system_libs += ["comdlg32", "imm32", "winmm"]
    if fixed_function:
        env.Append(CPPDEFINES=["FIXED_FUNCTION_ENABLED"])
    if env["opengl3"]:
        env.Append(CPPDEFINES=["GLES3_ENABLED"])
    if env["d3d7"]:
        env.Append(CPPDEFINES=["D3D7_ENABLED"])
    if env["d3d9"]:
        env.Append(CPPDEFINES=["D3D9_ENABLED"])
    if env.get("opengl1", False):
        env.Append(CPPDEFINES=["OPENGL1_ENABLED"])
    if env.get("opengl1", False) or env["opengl3"]:
        system_libs.append("opengl32")
    if env.get("d3d3_software", False):
        env.Append(CPPDEFINES=["D3D3_SOFTWARE_ENABLED"])
    if env["d3d7"] or env.get("d3d3_software", False):
        system_libs.append("dxguid")
    env.msvc = "mingw" not in env["TOOLS"]
    if env.msvc:
        env.Append(CPPDEFINES=["TYPED_METHOD_BIND"])
        env.Append(CCFLAGS=["/MT" if env["use_static_cpp"] else "/MD", "/utf-8", "/bigobj", "/fp:strict"])
        subsystem = "WINDOWS" if env["win_legacy_windowed"] else "CONSOLE"
        env.Append(LINKFLAGS=[f"/SUBSYSTEM:{subsystem}", "/STACK:8388608", "/INCREMENTAL:NO"])
        env.Append(LINKFLAGS=[lib + env["LIBSUFFIX"] for lib in system_libs])
    else:
        from SCons.Subst import quote_spaces

        prefix = env["mingw_prefix"]
        if prefix:
            env.PrependENVPath("PATH", os.path.join(os.path.abspath(prefix), "bin"))
        machine = "i686" if env["arch"] == "x86_32" else "x86_64"
        for key, tool in {"CC": "gcc", "CXX": "g++", "AR": "gcc-ar", "RANLIB": "gcc-ranlib"}.items():
            env[key] = env.WhereIs(f"{machine}-w64-mingw32-{tool}") or env.WhereIs(tool)
            if not env[key]:
                fail(f"Could not find {tool}. Set mingw_prefix to the MinGW-w64 MSVCRT toolchain root.")
        triple = subprocess.check_output([env["CXX"], "-dumpmachine"], text=True).strip()
        if triple != f"{machine}-w64-mingw32":
            fail(f"Expected a compiler targeting {machine}-w64-mingw32, found {triple}.")

        # Use the POSIX MinGW runtime's standard C++ threads.
        env.use_windows_spawn_fix()
        env["ARCOM_ORIG"] = env["ARCOM"]
        env["ARCOM"] = "${TEMPFILE('$ARCOM_ORIG', '$ARCOMSTR')}"
        env["TEMPFILESUFFIX"] = ".rsp"
        env["TEMPFILEARGESCFUNC"] = lambda arg: quote_spaces(arg.replace("\\", "/"))
        env.Append(CCFLAGS=["-Wa,-mbig-obj", "-ffp-contract=off"])
        env.Append(LINKFLAGS=["-mwindows" if env["win_legacy_windowed"] else "-mconsole", "-Wl,--stack,8388608"])
        env.Append(LIBS=system_libs)
        major = minimum >> 8
        minor = minimum & 0xFF
        env.Append(
            LINKFLAGS=[
                f"-Wl,--major-os-version,{major},--minor-os-version,{minor}",
                f"-Wl,--major-subsystem-version,{major},--minor-subsystem-version,{minor}",
            ]
        )
        if env["use_static_cpp"]:
            env.Append(LINKFLAGS=["-static"])
        env.extra_suffix = ".mingw" + env.extra_suffix

    if env["lto"] == "auto":
        env["lto"] = "none"
