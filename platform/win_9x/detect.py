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
    return {"arch": "x86_32"}


def configure(env):
    validate_arch(env["arch"], get_name(), ["x86_32"])
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
