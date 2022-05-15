def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "Basis4D",
        "Euler4D",
        "Node4D",
        "Octonion",
        "Transform4D",
        "Vector4",
        "Vector4i",
    ]


def get_doc_path():
    return "doc_classes"
