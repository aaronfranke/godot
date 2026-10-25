def is_active():
    return True


def get_name():
    return "Windows NT 6"


def can_build():
    # Runtime support is not implemented yet; only the editor exporter is available.
    return False
