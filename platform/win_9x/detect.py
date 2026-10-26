def is_active():
    return True


def get_name():
    return "Windows 9x"


def can_build():
    # Runtime support is not implemented yet; only the editor exporter is available.
    return False


def get_opts():
    from SCons.Variables import BoolVariable

    return [
        ("target_win_version", "Targeted Windows version, >= 0x0400 (Windows 95)", "0x0400"),
        BoolVariable("d3d3_software", "Build the Fixed Function Direct3D 3 software device backend", True),
        BoolVariable("d3d7", "Build the Fixed Function Direct3D 7 device backend", True),
        BoolVariable("d3d9", "Build the Fixed Function Direct3D 9 device backend", True),
        BoolVariable("opengl1", "Build the Fixed Function OpenGL 1.1 device backend", True),
    ]
