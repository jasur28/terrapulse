class Module:
    daemon = True
    order = 45

    def start_args(self, env):
        return ["--dir", env.path("var", "events")]
