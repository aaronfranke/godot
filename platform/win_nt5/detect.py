import os

from drivers.win_legacy.nt import build as nt_build


def is_active():
    return True


def get_name():
    return "Windows NT 5"


def can_build():
    return os.name == "nt"


def get_opts():
    from SCons.Variables import BoolVariable

    return nt_build.get_opts() + [
        (
            "target_win_version",
            "Minimum Windows NT version (auto = 0x0500/2000 for x86_32, or 0x0502/XPx64 for x86_64)",
            "auto",
        ),
        BoolVariable("d3d3_software", "Build Fixed Function D3D3 Software (x86_32 only)", False),
        BoolVariable("d3d7", "Build Fixed Function D3D7 (x86_32 only)", '${bits != "64"}'),
        BoolVariable("d3d9", "Build Fixed Function D3D9", True),
        BoolVariable("opengl1", "Build Fixed Function OpenGL1", True),
        BoolVariable("opengl3", "Build the existing GLES2/GLES3 renderers", False),
    ]


def get_flags():
    return [("bits", "32")] + nt_build.get_flags()


def configure(env):
    if env["bits"] == "default":
        env["bits"] = "32"
    floor = 0x0500
    if env["bits"] == "64" and floor < 0x0502:
        floor = 0x0502
    minimum = floor
    try:
        minimum = floor if env["target_win_version"] == "auto" else int(env["target_win_version"], 0)
    except ValueError:
        nt_build.fail("target_win_version must be 'auto' or a numeric Windows version.")
    if minimum < floor or minimum >= 0x0601:
        nt_build.fail(f"Invalid target_win_version for win_nt5: {minimum:#06x}.")
    if env["target_win_version"] == "auto":
        env["target_win_version"] = f"0x{minimum:04x}"
    nt_build.configure(env, minimum)
