def can_build(env, platform):
    # Supported architectures and platforms depend on the Embree library.
    if env["arch"] == "arm64" and platform == "windows" and env.msvc:
        return False
    if env["arch"] in ["x86_64", "arm64", "wasm32"]:
        return True
    is_windows_nt = platform in ("windows", "win_nt5", "win_nt6")
    if env["arch"] == "x86_32" and is_windows_nt:
        return True
    return False


def configure(env):
    pass
