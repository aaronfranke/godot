import os

from drivers.win_legacy.nt import build as nt_build


def is_active():
    return True


def get_name():
    return "Windows NT 6"


def can_build():
    return os.name == "nt"


def get_opts():
    from SCons.Variables import BoolVariable

    return nt_build.get_opts() + [
        ("target_win_version", "Minimum Windows NT version (auto = 0x0601 aka Windows 7)", "auto"),
        BoolVariable("opengl3", "Build the existing GLES2/GLES3 renderers", True),
        BoolVariable(
            "d3d3_software", "Build the Fixed Function Direct3D 3 software device backend (x86_32 only)", False
        ),
        BoolVariable("d3d7", "Build the Fixed Function Direct3D 7 device backend (x86_32 only)", False),
        BoolVariable("d3d9", "Build the Fixed Function Direct3D 9 device backend", True),
        BoolVariable("opengl1", "Build the Fixed Function OpenGL 1.1 device backend", True),
    ]


def get_flags():
    return [("bits", "64"), ("module_bullet_enabled", True)] + nt_build.get_flags()


def configure(env):
    if env["bits"] == "default":
        env["bits"] = "64"
    floor = 0x0601
    minimum = floor
    try:
        minimum = floor if env["target_win_version"] == "auto" else int(env["target_win_version"], 0)
    except ValueError:
        nt_build.fail("target_win_version must be 'auto' or a numeric Windows version.")
    if minimum < floor:
        nt_build.fail(f"Invalid target_win_version for win_nt6: {minimum:#06x}.")
    if minimum > 0x0A00:
        print("WARNING: Target Windows NT version is higher than expected. Consider using platform=windows instead.")
    if env["target_win_version"] == "auto":
        env["target_win_version"] = f"0x{minimum:04x}"
    nt_build.configure(env, minimum)
