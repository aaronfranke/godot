def can_build(env, platform):
    # Fixed Function also needs CPU decoding on modern Windows, X11 and macOS.
    return any(env.get(driver, False) for driver in ("d3d3_software", "d3d7", "d3d9", "opengl1"))


def configure(env):
    pass
