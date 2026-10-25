def is_active():
    return True


def get_name():
    return "Windows 9x"


def can_build():
    # Runtime support is not implemented yet; only the editor exporter is available.
    return False


def get_opts():
    return [
        ("target_win_version", "Targeted Windows version, >= 0x0400 (Windows 95)", "0x0400"),
    ]
