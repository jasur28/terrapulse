class Module:
    daemon = True
    order = 50

    def start_args(self, env):
        return ["--port", env.param("slmon_port", 8081)]
