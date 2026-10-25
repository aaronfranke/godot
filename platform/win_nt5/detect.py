def is_active():
    return True


def get_name():
    return "Windows NT 5"


def can_build():
    # Runtime support is not implemented yet; only the editor exporter is available.
    return False


def get_opts():
    return [
        ("target_win_version", "Targeted Windows version, >= 0x0400 (Windows NT 4.0)", "0x0500"),
    ]
