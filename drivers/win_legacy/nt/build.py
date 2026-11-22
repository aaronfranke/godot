from drivers.win_legacy import build as legacy_build

fail = legacy_build.fail


def get_opts():
    return legacy_build.get_opts()


def get_flags():
    return legacy_build.get_flags()


def configure(env, minimum):
    if env["bits"] == "default":
        env["bits"] = "64" if env["platform"] == "win_nt6" else "32"
    if env["bits"] not in ("32", "64"):
        fail("Legacy Windows supports bits=32 or bits=64.")
    if env["bits"] == "64" and (env["d3d7"] or env["d3d3_software"]):
        fail("D3D7 and D3D3 Software require bits=32; disable both for x86_64.")
    if env["lto"] == "auto":
        env["lto"] = "none"
    if env["lto"] == "thin":
        fail("Legacy Windows supports lto=none or lto=full.")
    env["target_win_version_int"] = minimum
    env.msvc = not env["use_mingw"]
    if env.msvc:
        legacy_build.configure_msvc(env)
        if minimum < 0x0600:
            env.Append(CPPDEFINES=["MBEDTLS_TEST_SW_INET_PTON"])
    else:
        legacy_build.configure_mingw(env, minimum)

    env.Prepend(CPPPATH=["#platform/" + env["platform"]])
    env.Append(
        CPPDEFINES=[
            "WINDOWS_ENABLED",
            "WIN_LEGACY_NT_ENABLED",
            env["platform"].upper() + "_ENABLED",
            ("WINVER", hex(minimum)),
            ("_WIN32_WINNT", hex(minimum)),
        ]
    )
    fixed_function = any(env.get(option, False) for option in ("d3d3_software", "d3d7", "d3d9", "opengl1"))
    env["fixed_function"] = fixed_function
    env["win_legacy_windowed"] = fixed_function or env.get("opengl3", False)
    if env.msvc:
        env["LINKFLAGS"].remove("/SUBSYSTEM:CONSOLE")
        env.Append(LINKFLAGS=["/SUBSYSTEM:WINDOWS" if env["win_legacy_windowed"] else "/SUBSYSTEM:CONSOLE"])
    else:
        env["LINKFLAGS"].remove("-Wl,--subsystem,console")
        env.Append(LINKFLAGS=["-mwindows" if env["win_legacy_windowed"] else "-mconsole"])
    if not env["win_legacy_windowed"]:
        env.Append(CPPDEFINES=["WINDOWS_SUBSYSTEM_CONSOLE"])
    system_libs = ["advapi32", "iphlpapi", "ole32", "oleaut32", "shell32", "wbemuuid", "ws2_32"]
    if env["win_legacy_windowed"]:
        env.Append(CPPDEFINES=["WIN_LEGACY_WINDOWED_ENABLED", "WINMIDI_ENABLED"])
        system_libs += ["user32", "gdi32", "imm32", "winmm", "comdlg32"]
    else:
        env.Append(CPPDEFINES=["SERVER_ENABLED"])
    if fixed_function:
        env.Append(CPPDEFINES=["FIXED_FUNCTION_ENABLED"])
    for option in ("d3d3_software", "d3d7", "d3d9", "opengl1"):
        if env.get(option, False):
            env.Append(CPPDEFINES=[option.upper() + "_ENABLED"])
    if env.get("opengl3", False):
        # Set desktop GL headers before SCons creates the GLES source objects.
        env.Append(CPPDEFINES=["GLES_ENABLED", "GLES_OVER_GL", "GLAD_ENABLED"])
        env.Prepend(CPPPATH=["#thirdparty/glad"])
    if env.get("opengl1", False) or env.get("opengl3", False):
        system_libs.append("opengl32")
    if env["d3d7"] or env["d3d3_software"]:
        system_libs.append("dxguid")
    if env.msvc:
        env.Append(LINKFLAGS=[lib + env["LIBSUFFIX"] for lib in system_libs])
    else:
        env.Append(LIBS=system_libs)
