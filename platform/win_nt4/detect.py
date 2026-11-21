import os
import sys


def _fail(message):
    print("ERROR: " + message, file=sys.stderr)
    sys.exit(255)


def is_active():
    return True


def get_name():
    return "Windows NT 4"


def can_build():
    return False


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("target_win_version", "Minimum Windows NT version (auto = 0x0400 aka Windows NT 4)", "auto"),
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
        _fail("win_nt4 supports only bits=32.")
    # 0x0400 = Windows NT 4 (the only real NT 4.x target).
    target_win_version_int = 0x0400
    if env["target_win_version"] == "auto":
        env["target_win_version"] = "0x0400"
        target_win_version_int = 0x0400
    else:
        try:
            target_win_version_int = int(env["target_win_version"], 0)
        except ValueError:
            _fail("target_win_version must be 'auto' or a numeric Windows NT version such as 0x0400.")
        if target_win_version_int < 0x0400:
            _fail("Target Windows NT version too low for `win_nt4`.")
        elif target_win_version_int >= 0x0500:
            _fail("Target Windows NT version too high for `win_nt4`. Consider using `platform=win_nt5` instead.")
    env["target_win_version_int"] = target_win_version_int
