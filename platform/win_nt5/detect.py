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
    return "Windows NT 5"


def can_build():
    return False


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        (
            "target_win_version",
            "Minimum Windows NT version (auto = 0x0500/2000 for x86_32, or 0x0502/XPx64 for x86_64)",
            "auto",
        ),
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
    validate_arch(env["arch"], get_name(), ["x86_32", "x86_64"])
    # 0x0500 = Windows 2000, 0x0501 = Windows XP 32-bit, 0x0502 = Windows XP x64 Edition / Server 2003.
    # 0x0600 = Windows Vista / Server 2008.
    target_win_version_int = 0x0500
    if env["target_win_version"] == "auto":
        env["target_win_version"] = "0x0502" if env["arch"] == "x86_64" else "0x0500"
        target_win_version_int = 0x0502 if env["arch"] == "x86_64" else 0x0500
    else:
        try:
            target_win_version_int = int(env["target_win_version"], 0)
        except ValueError:
            _fail("target_win_version must be 'auto' or a numeric Windows NT version such as 0x0500.")
        if target_win_version_int < 0x0500:
            _fail("Target Windows NT version too low for `win_nt5`. Consider using `platform=win_nt4` instead.")
        elif target_win_version_int < 0x0502 and env["arch"] == "x86_64":
            _fail("The x86_64 architecture requires target_win_version=0x0502 (Windows XP x64 Edition) or newer.")
        elif target_win_version_int >= 0x0601:
            _fail("Target Windows NT version too high for `win_nt5`. Consider using `platform=win_nt6` instead.")
    env["target_win_version_int"] = target_win_version_int
