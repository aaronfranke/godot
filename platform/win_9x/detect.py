import os
import sys


def _fail(message):
    print("ERROR: " + message, file=sys.stderr)
    sys.exit(255)


def is_active():
    return True


def get_name():
    return "Windows 9x"


def can_build():
    return False


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
    ]


def get_flags():
    return [("bits", "32")]


def configure(env):
    if env["bits"] == "default":
        env["bits"] = "32"
    if env["bits"] not in ["32"]:
        _fail("Windows 9x supports only bits=32.")
    # 0x0400 = Windows 95, 0x0410 = Windows 98, or 0x0490 = Windows ME.
    target_win_version_int = 0x0400
    if env["target_win_version"] == "auto":
        env["target_win_version"] = "0x0400"
        target_win_version_int = 0x0400
    else:
        try:
            target_win_version_int = int(env["target_win_version"], 0)
        except ValueError:
            _fail("target_win_version must be 'auto' or a numeric Windows version such as 0x0400.")
        if target_win_version_int < 0x0400:
            _fail("Target Windows version too low for `win_9x`.")
        elif target_win_version_int >= 0x0500:
            _fail("Target Windows version too high for `win_9x`. Consider using `platform=win_nt5` instead.")
    env["target_win_version_int"] = target_win_version_int
