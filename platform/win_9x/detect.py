import os

from drivers.win_legacy import build as legacy_build


def is_active():
    return True


def get_name():
    return "Windows 9x"


def can_build():
    return os.name == "nt"


def get_opts():
    from SCons.Variables import BoolVariable

    return legacy_build.get_opts() + [
        ("target_win_version", "Minimum Windows version (auto = 0x0400 aka Windows 95)", "auto"),
        BoolVariable("d3d3_software", "Build Fixed Function D3D3 Software", True),
        BoolVariable("d3d7", "Build Fixed Function D3D7", True),
        BoolVariable("d3d9", "Build Fixed Function D3D9", True),
        BoolVariable("opengl1", "Build Fixed Function OpenGL1", True),
    ]


def get_flags():
    return [("bits", "32")] + legacy_build.get_flags()


def configure(env):
    if env["bits"] == "default":
        env["bits"] = "32"
    if env["bits"] != "32":
        legacy_build.fail("Windows 9x requires bits=32.")
    minimum = 0x0400
    try:
        if env["target_win_version"] != "auto":
            minimum = int(env["target_win_version"], 0)
    except ValueError:
        legacy_build.fail("target_win_version must be 'auto' or a numeric Windows version.")
    if minimum < 0x0400 or minimum >= 0x0500:
        legacy_build.fail("win_9x requires 0x0400 <= target_win_version < 0x0500.")
    if env["target_win_version"] == "auto":
        env["target_win_version"] = f"0x{minimum:04x}"
    env["target_win_version_int"] = minimum
    if env["lto"] == "auto":
        env["lto"] = "none"
    if env["lto"] == "thin":
        legacy_build.fail("Windows 9x supports lto=none or lto=full.")
    env.msvc = not env["use_mingw"]
    if env.msvc:
        legacy_build.configure_msvc(env)
        env.Append(CPPDEFINES=["MBEDTLS_TEST_SW_INET_PTON"])
    else:
        legacy_build.configure_mingw(env, minimum, windows_9x=True)
        env.Append(CCFLAGS=["-march=i686", "-mno-sse", "-mno-sse2", "-mfpmath=387"])
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
    fixed_function = any(env[option] for option in ("d3d3_software", "d3d7", "d3d9", "opengl1"))
    env["fixed_function"] = fixed_function
    env["win_legacy_windowed"] = fixed_function
    if env.msvc:
        env["LINKFLAGS"].remove("/SUBSYSTEM:CONSOLE")
        env.Append(LINKFLAGS=["/SUBSYSTEM:WINDOWS" if fixed_function else "/SUBSYSTEM:CONSOLE"])
    else:
        env["LINKFLAGS"].remove("-Wl,--subsystem,console")
        env.Append(LINKFLAGS=["-mwindows" if fixed_function else "-mconsole"])
    system_libs = ["advapi32", "iphlpapi", "ole32", "shell32", "user32", "ws2_32"]
    if fixed_function:
        env.Append(CPPDEFINES=["WIN_LEGACY_WINDOWED_ENABLED", "FIXED_FUNCTION_ENABLED"])
        system_libs += ["gdi32", "winmm"]
    else:
        env.Append(CPPDEFINES=["SERVER_ENABLED", "WINDOWS_SUBSYSTEM_CONSOLE"])
    for option in ("d3d3_software", "d3d7", "d3d9", "opengl1"):
        if env[option]:
            env.Append(CPPDEFINES=[option.upper() + "_ENABLED"])
    if env["d3d3_software"] or env["d3d7"]:
        system_libs.append("dxguid")
    if env["opengl1"]:
        system_libs.append("opengl32")
    if env.msvc:
        env.Append(LINKFLAGS=[lib + env["LIBSUFFIX"] for lib in system_libs])
    else:
        env.Append(LIBS=system_libs)
