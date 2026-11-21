import os
import sys

from methods import print_error, print_warning
from platform_methods import validate_arch


def _fail(message):
    print_error(message)
    sys.exit(255)


def is_active():
    return True


def get_name():
    return "Windows NT 6"


def can_build():
    return False


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("target_win_version", "Minimum Windows NT version (auto = 0x0601 aka Windows 7)", "auto"),
        BoolVariable(
            "use_mingw",
            "Use MinGW GCC instead of MSVC. Strongly recommended for compatibility with old Windows.",
            True,
        ),
        ("mingw_prefix", "Path to the MinGW-w64 MSVCRT toolchain root", os.getenv("MINGW_PREFIX", "")),
        BoolVariable("use_static_cpp", "Link the compiler runtime statically", True),
    ]


def get_flags():
    return {"arch": "x86_64"}


def configure(env):
    validate_arch(env["arch"], get_name(), ["x86_32", "x86_64"])
    # 0x0601 = Windows 7, 0x0602 = Windows 8, 0x0603 = Windows 8.1.
    # 0x0A00 = Both Windows 10 and Windows 11 (Microsoft failed at counting).
    if env["target_win_version"] == "auto":
        env["target_win_version"] = "0x0601"
        env["target_win_version_int"] = 0x0601
    else:
        version_int = 0x0601
        try:
            version_int = int(env["target_win_version"], 0)
        except ValueError:
            _fail("target_win_version must be 'auto' or a numeric Windows NT version such as 0x0601.")
        if version_int < 0x0601:
            _fail("Target Windows NT version too low for `win_nt6`. Consider using `platform=win_nt5` instead.")
        elif version_int > 0x0A00:
            print_warning(
                "Target Windows NT version is higher than expected. Consider using the modern `platform=windows` instead."
            )
        env["target_win_version_int"] = version_int
