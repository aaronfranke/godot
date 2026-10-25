def is_active():
    return True


def get_name():
    return "Windows NT 5"


def can_build():
    # Runtime support is not implemented yet; only the editor exporter is available.
    return False


def get_opts():
    from SCons.Variables import BoolVariable
    # Evaluated by SCons after loading build options; env does not exist here yet.
    is_not_64bit = '${arch not in ("x86_64", "arm64", "rv64", "ppc64", "loongarch64")}'
    return [
        ("target_win_version", "Targeted Windows version, >= 0x0400 (Windows NT 4.0)", "0x0500"),
        BoolVariable("d3d3_software", "Build the Fixed Function Direct3D 3 software device backend (x86_32 only)", is_not_64bit),
        BoolVariable("d3d7", "Build the Fixed Function Direct3D 7 device backend (x86_32 only)", is_not_64bit),
        BoolVariable("d3d9", "Build the Fixed Function Direct3D 9 device backend", True),
        BoolVariable("opengl1", "Build the Fixed Function OpenGL 1.1 device backend", True),
    ]
