def can_build(env, platform):
    return env["tools"] or platform == "win_nt5"


def configure(env):
    pass
